#include "SArc/Helpers.hpp"

#include <crc.h>
#include <lz4.h>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <fstream>
#include <iostream>
#include <ranges>

#ifdef SARC_USE_IPP
#include <LzmaLib.h>
#include <ipp.h>
#elifdef SARC_USE_AAC
#include <Accelerate/Accelerate.h>
#else
#include <LzmaLib.h>
#include <lz4hc.h>
#endif

using namespace SArc;

#ifdef SARC_USE_IPP
#define SARC_IPP_CHECK(call)                                                                                           \
	do {                                                                                                               \
		const IppStatus status = call;                                                                                 \
		SARC_RUNTIME_ASSERT(status == ippStsNoErr, std::runtime_error,                                                 \
							std::format("IPP: {}", ippGetStatusString(status)));                                       \
	} while (false)
#endif

std::string helpers::read_null_terminated_utf8(std::istream &stream) {
	std::string result;
	std::getline(stream, result, '\0');
	return result;
}

bytes_t helpers::read_file(const std::filesystem::path &path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) throw io_error("Cannot open file: " + path.string());

	const std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	bytes_t buffer(size);
	if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) throw io_error("Cannot read file: " + path.string());

	return buffer;
}

static std::string_view get_extension(const std::string &file) {
	const auto pos = file.rfind('.');

	if (pos == std::string::npos) return {};

	return std::string_view(file).substr(pos + 1);
}

file_block_map_t helpers::auto_mappings(const SArchive &archive, const uint32_t target_block_size,
										const file_block_map_t &file_block_map) {
	file_block_map_t result{file_block_map};

	result.reserve(archive.get_all_paths().size());

	uint8_t latest_block_files = 0;
	uint32_t latest_block_size = 0;
	uint32_t latest_block = 0;
	for (const auto block : result | std::views::values)
		if (block > latest_block) latest_block = block;

	std::vector<std::string> files = archive.get_all_paths();
	std::ranges::sort(files, [&](const std::string &a, const std::string &b) {
		const std::string_view ext_a = get_extension(a);
		const std::string_view ext_b = get_extension(b);

		if (ext_a == ext_b) return a < b;

		return ext_a < ext_b;
	});

	for (auto [path, file] : archive.const_iterate()) {
		if (!result.contains(path)) {
			uint64_t new_block_size = latest_block_size + file->data.size() + 4;

			if (new_block_size > UINT32_MAX || latest_block_files >= UINT8_MAX) {
				latest_block++;
				latest_block_files = 0;
				new_block_size = file->data.size() + 4;
			}

			result[path] = latest_block;
			latest_block_size = new_block_size;

			latest_block_files++;

			if (latest_block_size >= target_block_size) {
				latest_block++;
				latest_block_size = 0;
			}
		}
	}

	return result;
}

file_block_map_t helpers::auto_mappings(std::vector<std::string> files, const std::vector<uint32_t> &file_sizes,
										const uint32_t target_block_size, const file_block_map_t &file_block_map) {
	file_block_map_t result{file_block_map};

	result.reserve(files.size());

	uint8_t latest_block_files = 0;
	uint32_t latest_block_size = 0;
	uint32_t latest_block = 0;
	for (const auto block : result | std::views::values)
		if (block > latest_block) latest_block = block;

	std::ranges::sort(files, [&](const std::string &a, const std::string &b) {
		const std::string_view ext_a = get_extension(a);
		const std::string_view ext_b = get_extension(b);

		if (ext_a == ext_b) return a < b;

		return ext_a < ext_b;
	});

	for (uint64_t i = 0; i < files.size(); i++) {
		const std::string &path = files[i];
		const uint32_t file_size = file_sizes[i];

		if (!result.contains(path)) {
			uint64_t new_block_size = latest_block_size + file_size + 4;

			if (new_block_size > UINT32_MAX || latest_block_files >= UINT8_MAX) {
				latest_block++;
				latest_block_files = 0;
				new_block_size = file_size + 4;
			}

			result[path] = latest_block;
			latest_block_size = new_block_size;

			latest_block_files++;

			if (latest_block_size >= target_block_size) {
				latest_block++;
				latest_block_size = 0;
			}
		}
	}

	return result;
}

bytes_t helpers::lzma_compress(const byte_span_const_t &data, const uint8_t level) {
	size_t compressed_size = data.size() + data.size() / 3 + 128;

#ifdef SARC_USE_AAC
	// Apple Accelerate (https://developer.apple.com/documentation/compression/compression_lzma?language=objc)
#else
	uint8_t props[LZMA_PROPS_SIZE];
	size_t props_size = LZMA_PROPS_SIZE;

	bytes_t compressed(props_size + compressed_size);

	int result = LzmaCompress(reinterpret_cast<Byte *>(compressed.data() + props_size), &compressed_size,
							  reinterpret_cast<const Byte *>(data.data()), data.size(), props, &props_size, level, 0,
							  -1, -1, -1, -1, -1);

	if (result == SZ_ERROR_OUTPUT_EOF) {
		compressed_size = data.size();
		compressed.resize(props_size + compressed_size);

		result = LzmaCompress(reinterpret_cast<Byte *>(compressed.data() + props_size), &compressed_size,
							  reinterpret_cast<const Byte *>(data.data()), data.size(), props, &props_size, level, 0,
							  -1, -1, -1, -1, -1);
	}

	if (result == SZ_ERROR_MEM) throw memory_error("LZMA: Memory allocation error");
	if (result == SZ_ERROR_PARAM) throw std::invalid_argument("LZMA: Invalid parameters");
	if (result == SZ_ERROR_OUTPUT_EOF)
		throw memory_error("LZMA: Compressed data too large (try a higher compression level)");
	if (result == SZ_ERROR_THREAD) throw thread_error("LZMA: Error in multithreading funcitons");
	if (result != SZ_OK) throw std::runtime_error("LZMA: Unknown error in compression");

	compressed.resize(props_size + compressed_size);

	std::memcpy(compressed.data(), props, props_size);

	return compressed;
#endif
}

bytes_t helpers::lzma_decompress(const byte_span_const_t &data, const size_t decompressed_size) {
	size_t dc_size = decompressed_size;
	size_t c_size = data.size();

	bytes_t decompressed(dc_size);

#ifdef SARC_USE_AAC
// https://developer.apple.com/documentation/compression/compression_decode_buffer(_:_:_:_:_:_:)?language=objc
#else
	const int result = LzmaUncompress(reinterpret_cast<Byte *>(decompressed.data()), &dc_size,
									  reinterpret_cast<const Byte *>(data.data() + LZMA_PROPS_SIZE), &c_size,
									  reinterpret_cast<const Byte *>(data.data()), LZMA_PROPS_SIZE);

	if (result == SZ_ERROR_DATA) throw corrupted_data("LZMA: Data decompression error");
	if (result == SZ_ERROR_MEM) throw memory_error("LZMA: Memory allocation error");
	if (result == SZ_ERROR_UNSUPPORTED) throw std::invalid_argument("LZMA: Unsupported properties");
	if (result == SZ_ERROR_INPUT_EOF) throw memory_error("LZMA: Input vector too small");
	if (result != SZ_OK) throw std::runtime_error("LZMA: Unknown error in decompression");
#endif
	return decompressed;
}

#ifdef SARC_USE_IPP
namespace SArc::helpers {
	class IPPMemoryAutoFree {
	public:
		explicit IPPMemoryAutoFree(const IppSizeL size) {
			data = ippsMalloc_8u_L(size);
			SARC_RUNTIME_ASSERT(data, std::runtime_error, "IPP allocation failed");
		}

		~IPPMemoryAutoFree() { ippsFree(data); }

		uint8_t *data;
	};
} // namespace SArc::helpers
#endif

bytes_t helpers::lz4_compress(const byte_span_const_t &data, const uint8_t level) {
	bytes_t result;
	result.resize(LZ4_compressBound(data.size()));

#ifdef SARC_USE_IPP
	if (level < 5) {
		int hash_table_size;
		SARC_IPP_CHECK(ippsEncodeLZ4HashTableGetSize_8u(&hash_table_size));

		const IPPMemoryAutoFree hash_table{hash_table_size};
		SARC_RUNTIME_ASSERT(hash_table.data, std::runtime_error, "Count not allocate LZ4 hash table for IPP");
		SARC_IPP_CHECK(ippsEncodeLZ4HashTableInit_8u(hash_table.data, data.size()));

		int result_size = result.size();

		SARC_IPP_CHECK(ippsEncodeLZ4Fast_8u(reinterpret_cast<const uint8_t *>(data.data()), data.size(),
											reinterpret_cast<uint8_t *>(result.data()), &result_size, hash_table.data,
											level == 0 ? 17 : std::roundf(level / 5.f * 17.f)));

		result.resize(result_size);
		result.shrink_to_fit();
	} else {
		int hash_table_size;
		int prev_table_size;
		SARC_IPP_CHECK(ippsEncodeLZ4HCHashTableGetSize_8u(&hash_table_size, &prev_table_size));

		const IPPMemoryAutoFree hash_table{hash_table_size};
		const IPPMemoryAutoFree prev_table{prev_table_size};

		uint8_t *tables[2] = {hash_table.data, prev_table.data};
		SARC_IPP_CHECK(ippsEncodeLZ4HCHashTableInit_8u(tables));

		int data_length = data.size();
		int result_size = result.size();

		SARC_IPP_CHECK(ippsEncodeLZ4HC_8u(reinterpret_cast<const uint8_t *>(data.data()), 0, &data_length,
										  reinterpret_cast<uint8_t *>(result.data()), &result_size, tables, nullptr, 0,
										  level));

		result.resize(result_size);
		result.shrink_to_fit();
	}
#elifdef SARC_USE_AAC
	// Apple Accelerate stuff (https://developer.apple.com/documentation/compression/compression_lz4_raw?language=objc)
#else
	if (level < 5) {
		const int32_t size = LZ4_compress_fast(reinterpret_cast<const uint8_t *>(data.data()),
											   reinterpret_cast<uint8_t *>(result.data()), data.size(), result.size(),
											   level == 0 ? 17 : std::roundf(level / 5.f * 17.f));
		SARC_RUNTIME_ASSERT(size > 0, std::runtime_error, "LZ4: Error in compression");

		result.resize(size);
		result.shrink_to_fit();
	} else {
		const int32_t size =
			LZ4_compress_HC(reinterpret_cast<const uint8_t *>(data.data()), reinterpret_cast<uint8_t *>(result.data()),
							data.size(), result.size(), level);
		SARC_RUNTIME_ASSERT(size > 0, std::runtime_error, "LZ4HC: Error in compression");

		result.resize(size);
		result.shrink_to_fit();
	}
#endif
	return result;
}

bytes_t helpers::lz4_decompress(const byte_span_const_t &data, const size_t decompressed_size) {
	bytes_t result;
	result.resize(decompressed_size);

#ifdef SARC_USE_IPP
	int result_size = decompressed_size;
	ippsDecodeLZ4_8u(reinterpret_cast<const uint8_t *>(data.data()), data.size(),
					 reinterpret_cast<uint8_t *>(result.data()), &result_size);

	result.resize(result_size);
	result.shrink_to_fit();
#elifdef SARC_USE_AAC
	// https://developer.apple.com/documentation/compression/compression_decode_buffer(_:_:_:_:_:_:)?language=objc
#else
	SARC_RUNTIME_ASSERT(LZ4_decompress_safe(reinterpret_cast<const char *>(data.data()),
											reinterpret_cast<char *>(result.data()), data.size(),
											result.size()) == decompressed_size,
						std::runtime_error, "LZ4: Error in decompression");
#endif
	return result;
}

bytes_t helpers::oneshot_aes256_encrypt(const byte_span_const_t &data, const std::string &passphrase) {
	std::array<uint8_t, SHA256_DIGEST_LENGTH> hash{};
	std::array<uint8_t, 16> aes_iv{};

	bytes_t ciphertext;
	ciphertext.resize(data.size());

	RAND_bytes(aes_iv.data(), aes_iv.size());
	SHA256(reinterpret_cast<const unsigned char *>(passphrase.c_str()), data.size(), hash.data());

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	EVP_EncryptInit(ctx, EVP_aes_256_cbc(), hash.data(), aes_iv.data());

	int32_t len = 0;

	EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char *>(ciphertext.data()), &len,
					  reinterpret_cast<const unsigned char *>(data.data()), data.size());
	EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char *>(ciphertext.data() + len), &len);

	EVP_CIPHER_CTX_free(ctx);

	return ciphertext;
}

bytes_t helpers::oneshot_aes256_decrypt(const byte_span_const_t &data, const std::string &passphrase) {
	throw std::runtime_error("Not implemented");
}

uint32_t helpers::calculate_crc32(std::istream &stream, const size_t len) {
	return crc32strm(
		[](void *userdata) -> char {
			char buf;
			static_cast<std::istream *>(userdata)->read(&buf, 1);
			return buf;
		},
		len, &stream);
}

uint32_t helpers::calculate_crc32(const bytes_t &data) {
	return crc32buf(reinterpret_cast<const char *>(data.data()), data.size());
}

std::array<std::byte, 32> helpers::calculate_sha256(std::istream &stream, const size_t len) {
	EVP_MD_CTX *ctx = nullptr;

	try {
		ctx = EVP_MD_CTX_new();
		SARC_RUNTIME_ASSERT(ctx, crypto_error, "Failed to create EVP MD context");

		SARC_RUNTIME_ASSERT(EVP_DigestInit_ex2(ctx, EVP_sha256(), nullptr), crypto_error,
							"Failed to initialise SHA256 MD context");

		std::array<char, 256> buffer{};
		std::array<std::byte, 32> digest{};

		for (size_t i = 0; i < len / 256; i++) {
			stream.read(buffer.data(), 256);

			SARC_RUNTIME_ASSERT(EVP_DigestUpdate(ctx, buffer.data(), 256), crypto_error,
								std::format("SHA256 failed at position {}", i * 256));
		}

		stream.read(buffer.data(), len % 256);
		SARC_RUNTIME_ASSERT(EVP_DigestUpdate(ctx, buffer.data(), len % 256), crypto_error,
							std::format("SHA256 failed at position {}", len - (len % 256)));

		SARC_RUNTIME_ASSERT(EVP_DigestFinal_ex(ctx, reinterpret_cast<unsigned char *>(digest.data()), nullptr),
							crypto_error, "Failed to finalise SHA256 hash");

		EVP_MD_CTX_free(ctx);

		return digest;
	} catch (std::exception &) {
		EVP_MD_CTX_free(ctx);

		throw;
	}
}

void helpers::archive_serialise_blocks_to_stream(const SArchive &archive, const file_block_map_t &file_block_map,
												 const uint8_t compression_level, std::ostream &stream,
												 const progress_callback_t &progress_callback) {
	uint32_t block_count = 0;
	for (const auto block : file_block_map | std::views::values)
		if (block >= block_count) block_count = block + 1;

	for (uint32_t block = 0; block < block_count; block++) {
		std::vector<std::string> paths;
		paths.reserve(255);

		for (const auto &[path, f_block] : file_block_map)
			if (f_block == block) paths.push_back(path);

		paths.shrink_to_fit();

		uint8_t temp = paths.size();
		stream.write(reinterpret_cast<const char *>(&temp), 1);

		uint32_t block_uncompressed_size = 0;

		for (const auto &path : paths) {
			block_uncompressed_size += archive[path]->get_serialised_size();

			stream.write(path.c_str(), path.size() + 1);
		}

		bytes_t block_data;
		block_data.reserve(block_uncompressed_size);
		for (const auto &path : paths) {
			const SArchiveFile *file = archive[path];
			file->serialise_append(block_data);
		}

		stream.write(
			reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(calculate_crc32(block_data)).data()), 4);

		stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(block_data.size()).data()), 4);

		progress_callback(block / (block_count + 1.f), std::format("Compressing block {}/{}", block + 1, block_count));
		bytes_t compressed = lzma_compress(block_data, compression_level);

		stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(compressed.size()).data()), 4);
		stream.write(reinterpret_cast<const char *>(compressed.data()), compressed.size());
	}

	progress_callback(1.f, "Serialised all blocks");
}

static bool read_stream(void *app_ctx, void *buf, size_t len, size_t *readres) {
	auto *stream = static_cast<std::istream *>(app_ctx);

	stream->read(static_cast<char *>(buf), len);
	*readres = static_cast<size_t>(stream->gcount());

	return !stream->bad();
}

static uint8_t hex_val(const char c) {
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	throw std::invalid_argument("Invalid hex character");
}

sha256_hash_t helpers::hex_str_to_fingerprint(const std::string &hex) {
	SARC_RUNTIME_ASSERT(hex.length() == 256 / 4, std::invalid_argument, "Hex string not 64 chars long");

	sha256_hash_t out{};

	for (uint8_t i = 0; i < 20; i++) {
		const uint8_t hi = hex_val(hex[2 * i]);
		const uint8_t lo = hex_val(hex[2 * i + 1]);
		out[i] = static_cast<std::byte>((hi << 4) | lo);
	}

	return out;
}

std::string helpers::fingerprint_to_hex_str(const sha256_hash_t &fingerprint) {
	static constexpr char hexmap[] = "0123456789ABCDEF";

	std::string out;
	out.resize(256 / 4);

	const std::byte *src = fingerprint.data();

	for (uint8_t i = 0; i < 128; i++) {
		const std::byte b = src[i];
		out[2 * i] = hexmap[static_cast<uint8_t>(b >> 4)];
		out[2 * i + 1] = hexmap[static_cast<uint8_t>(b) & 0x0F];
	}

	return out;
}
