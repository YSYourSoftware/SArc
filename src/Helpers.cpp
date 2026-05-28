#include "SArc/Helpers.hpp"

#include <LzmaLib.h>
#include <crc.h>
#include <rnp/rnp.h>
#include <rnp/rnp_err.h>

#include <fstream>
#include <iostream>
#include <ranges>

using namespace SArc;

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

size_t helpers::lzma_get_compressed_size(const byte_span_const_t &data, const uint8_t level) {
	size_t compressed_size = data.size() + data.size() / 3 + 128;

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

	return props_size + compressed_size;
}

bytes_t helpers::lzma_compress(const byte_span_const_t &data, const uint8_t level) {
	size_t compressed_size = data.size() + data.size() / 3 + 128;

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
}

bytes_t helpers::lzma_decompress(const byte_span_const_t &data, const size_t decompressed_size) {
	size_t dc_size = decompressed_size;
	size_t c_size = data.size();

	bytes_t decompressed(dc_size);

	const int result = LzmaUncompress(reinterpret_cast<Byte *>(decompressed.data()), &dc_size,
									  reinterpret_cast<const Byte *>(data.data() + LZMA_PROPS_SIZE), &c_size,
									  reinterpret_cast<const Byte *>(data.data()), LZMA_PROPS_SIZE);

	if (result == SZ_ERROR_DATA) throw corrupted_data("LZMA: Data decompression error");
	if (result == SZ_ERROR_MEM) throw memory_error("LZMA: Memory allocation error");
	if (result == SZ_ERROR_UNSUPPORTED) throw std::invalid_argument("LZMA: Unsupported properties");
	if (result == SZ_ERROR_INPUT_EOF) throw memory_error("LZMA: Input vector too small");
	if (result != SZ_OK) throw std::runtime_error("LZMA: Unknown error in decompression");

	return decompressed;
}

uint32_t helpers::calculate_crc32(const bytes_t &data) {
	return crc32buf(reinterpret_cast<const char *>(data.data()), data.size());
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

helpers::pgp_sigver_result_t helpers::verify_detached_pgg_signature(rnp_ffi_t ffi, const byte_span_const_t &signature,
																	std::istream &stream) {
	rnp_input_t data_input = nullptr;
	rnp_input_t signature_input = nullptr;
	rnp_op_verify_t verify = nullptr;

	try {
		SARC_RUNTIME_ASSERT(rnp_input_from_callback(&data_input, &read_stream, nullptr, &stream) == RNP_SUCCESS,
							std::runtime_error, "Failed to read data from stream");
		SARC_RUNTIME_ASSERT(rnp_input_from_memory(&signature_input, reinterpret_cast<const uint8_t *>(signature.data()),
												  signature.size(), false) == RNP_SUCCESS,
							std::runtime_error, "Failed to read signature data");

		SARC_RUNTIME_ASSERT(rnp_op_verify_detached_create(&verify, ffi, data_input, signature_input) == RNP_SUCCESS,
							ffi_error, "Failed to create detached verifier");

		SARC_RUNTIME_ASSERT(rnp_op_verify_execute(verify) == RNP_SUCCESS, ffi_error,
							"Failed to execute verification operation");

		size_t signature_count = 0;
		SARC_RUNTIME_ASSERT(rnp_op_verify_get_signature_count(verify, &signature_count) == RNP_SUCCESS, ffi_error,
							"Failed to get signature count");

		for (size_t i = 0; i < signature_count; i++) {
			rnp_op_verify_signature_t s_verify_op = nullptr;

			if (rnp_op_verify_get_signature_at(verify, i, &s_verify_op) != RNP_SUCCESS) continue;

			if (rnp_op_verify_signature_get_status(s_verify_op) == RNP_SUCCESS) {
				rnp_key_handle_t signing_key = nullptr;
				char *fp_buf = nullptr;

				SARC_RUNTIME_ASSERT(rnp_op_verify_signature_get_key(s_verify_op, &signing_key) == RNP_SUCCESS,
									ffi_error, "Failed to get key from signature");

				if (rnp_key_get_fprint(signing_key, &fp_buf) != RNP_SUCCESS) {
					rnp_key_handle_destroy(signing_key);
					throw invalid_gpg_data("Could not get fingerprint from signing key");
				}

				rnp_key_handle_destroy(signing_key);
				rnp_op_verify_destroy(verify);
				rnp_input_destroy(data_input);
				rnp_input_destroy(signature_input);

				const pgp_fingerprint_t fp = hex_str_to_fingerprint({fp_buf});

				rnp_buffer_destroy(fp_buf);

				return {true, fp};
			}
		}

		rnp_op_verify_destroy(verify);
		rnp_input_destroy(data_input);
		rnp_input_destroy(signature_input);

		return {false, {}};
	} catch (const std::exception &e) {
		rnp_op_verify_destroy(verify);
		rnp_input_destroy(data_input);
		rnp_input_destroy(signature_input);

		throw;
	}
}

static uint8_t hex_val(const char c) {
	if ('0' <= c && c <= '9') return c - '0';
	if ('a' <= c && c <= 'f') return c - 'a' + 10;
	if ('A' <= c && c <= 'F') return c - 'A' + 10;
	throw std::invalid_argument("Invalid hex character");
}

pgp_fingerprint_t helpers::hex_str_to_fingerprint(const std::string &hex) {
	SARC_RUNTIME_ASSERT(hex.length() == 40, std::invalid_argument, "Hex string not 40 chars long");

	pgp_fingerprint_t out{};

	for (uint8_t i = 0; i < 20; i++) {
		const uint8_t hi = hex_val(hex[2 * i]);
		const uint8_t lo = hex_val(hex[2 * i + 1]);
		out[i] = (hi << 4) | lo;
	}

	return out;
}

std::string helpers::fingerprint_to_hex_str(const pgp_fingerprint_t &fingerprint) {
	static constexpr char hexmap[] = "0123456789ABCDEF";

	std::string out;
	out.resize(40);

	const uint8_t *src = fingerprint.data();

	for (int i = 0; i < 20; i++) {
		const uint8_t b = src[i];
		out[2 * i] = hexmap[b >> 4];
		out[2 * i + 1] = hexmap[b & 0x0F];
	}

	return out;
}
