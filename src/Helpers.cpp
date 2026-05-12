#include "SArc/Helpers.hpp"

#include <LzmaLib.h>
#include <crc.h>

#include <fstream>
#include <iostream>
#include <ranges>

using namespace SArc;

file_block_map_t helpers::auto_mappings(const SArchive &archive, const uint32_t target_block_size,
										const file_block_map_t &file_block_map) {
	file_block_map_t result{file_block_map};

	result.reserve(archive.get_all_paths().size());

	uint32_t latest_block_size = 0;
	uint32_t latest_block = 0;
	for (const auto block : result | std::views::values)
		if (block > latest_block) latest_block = block;

	// TODO: Map by file extension rather than filesystem order
	for (auto [path, file] : archive.iterate()) {
		if (!result.contains(path)) {
			uint64_t new_block_size = latest_block_size + file.data.size() + 4;

			if (new_block_size > UINT32_MAX) {
				latest_block++;
				new_block_size = file.data.size() + 4;
			}

			result[path] = latest_block;
			latest_block_size = new_block_size;

			if (latest_block_size >= target_block_size) {
				latest_block++;
				latest_block_size = 0;
			}
		}
	}

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

void helpers::emplace_null_terminated_utf8(bytes_t &bytes, const std::string &string) {
	for (const char c : string) bytes.push_back(static_cast<std::byte>(c));
	bytes.push_back(static_cast<std::byte>(0));
}

std::string helpers::retrieve_null_terminated_utf8(const byte_span_const_t &bytes, size_t offset) {
	if (offset >= bytes.size()) throw std::out_of_range("Attempt to retrieve out of range");

	std::string result;
	while (offset < bytes.size()) {
		const auto c = static_cast<unsigned char>(bytes[offset]);
		offset++;

		if (c == 0) break;
		result.push_back(static_cast<char>(c));
	}

	return result;
}

uint32_t helpers::calculate_crc32(const bytes_t &data) {
	return crc32buf(reinterpret_cast<const char *>(data.data()), data.size());
}

void helpers::archive_serialise_blocks_to_stream(const SArchive &archive, const file_block_map_t &file_block_map,
												 const uint8_t compression_level, std::ostream &stream) {
	uint32_t block_count = 0;
	for (const auto block : file_block_map | std::views::values)
		if (block > block_count) block_count = block;

	for (uint32_t block = 0; block < block_count; block++) {
		std::vector<std::string> paths;

		for (const auto &[path, f_block] : file_block_map)
			if (f_block == block) paths.push_back(path);

		paths.shrink_to_fit();

		stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(paths.size()).data()),
					 sizeof(uint32_t));

		uint32_t block_uncompressed_size = 0;
		bytes_t path_buffer{256};

		for (const auto &path : paths) {
			block_uncompressed_size += archive[path].get_serialised_size();

			emplace_null_terminated_utf8(path_buffer, path);
			stream.write(reinterpret_cast<const char *>(path_buffer.data()), path_buffer.size());
			path_buffer.clear();
		}

		bytes_t block_data{block_uncompressed_size};
		for (const auto &path : paths) {
			const SArchiveFile &file = archive[path];
			file.serialise_append(block_data);
		}

		stream.write(
			reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(calculate_crc32(block_data)).data()),
			sizeof(uint32_t));

		bytes_t compressed = lzma_compress(block_data, compression_level);

		stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(compressed.size()).data()),
					 sizeof(uint32_t));
		stream.write(reinterpret_cast<const char *>(compressed.data()), compressed.size());
	}
}
