#include "SArc/BlockEncoder.hpp"
#include "SArc/Helpers.hpp"

using namespace SArc;

BlockEncoder::BlockEncoder(std::ostream &stream) : m_stream(stream) {
	m_stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(SARC_MAGIC).data()), 4);
	m_stream.write(reinterpret_cast<const char *>(&SARC_VERSION), 1);

	m_block_count_offset = m_stream.tellp();

	m_stream.write(reinterpret_cast<const char *>(&m_block_count), 4);

	constexpr uint8_t zero = 0;
	m_stream.write(reinterpret_cast<const char *>(&zero), 1);
}

void BlockEncoder::start_block() {
	SARC_RUNTIME_ASSERT(m_state == OUT_OF_BLOCK, state_error,
						"start_block can only be called when out of a block (did you forget end_block()?)");

	m_block_count++;

	m_block_file_count_offset = m_stream.tellp();

	m_stream.seekp(m_block_count_offset);
	m_stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(m_block_count).data()), 4);

	m_stream.seekp(m_block_file_count_offset);
	m_stream.write(reinterpret_cast<const char *>(&m_block_written_file_count), 1);

	m_state = IN_BLOCK_HEADERS;
}

void BlockEncoder::block_add_file_path(const std::string &path) {
	SARC_RUNTIME_ASSERT(m_state == IN_BLOCK_HEADERS, state_error,
						"block_add_file_path can only be called when in block headers");

	m_block_target_file_count++;

	m_stream.write(path.c_str(), path.size() + 1);
}

void BlockEncoder::block_add_file_data(const bytes_t &data) {
	SARC_RUNTIME_ASSERT(m_state == IN_BLOCK_HEADERS || m_state == IN_BLOCK_FILE_DATA, state_error,
						"block_add_file_data can only be called when in block headers or file data");

	if (m_state == IN_BLOCK_HEADERS) {
		m_block_files_data.reserve(m_block_target_file_count);

		m_block_size_headers_offset = m_stream.tellp();

		constexpr uint32_t zero = 0;
		m_stream.write(reinterpret_cast<const char *>(&zero), 4);
		m_stream.write(reinterpret_cast<const char *>(&zero), 4);
		m_stream.write(reinterpret_cast<const char *>(&zero), 4);

		m_state = IN_BLOCK_FILE_DATA;
	}

	m_block_written_file_count++;
	SARC_RUNTIME_ASSERT(m_block_written_file_count <= m_block_target_file_count, std::overflow_error,
						"Too many files added");

	m_block_files_data.emplace_back(data);
}

struct encoded_block_data {
	bytes_t compressed_data;
	uint32_t decompressed_crc32;
};

static encoded_block_data sarcbenc_encode_block(const uint32_t decompressed_size,
												const std::vector<bytes_t> &block_files_data,
												const uint8_t compression_level) {
	uint32_t cursor = 0;
	bytes_t decompressed_data{decompressed_size};

	for (const auto &data : block_files_data) {
		std::memcpy(decompressed_data.data() + cursor, helpers::to_big_endian<uint32_t>(data.size()).data(), 4);
		cursor += 4;

		std::memcpy(decompressed_data.data() + cursor, data.data(), data.size());
		cursor += data.size();
	}

	return {helpers::lzma_compress(decompressed_data, compression_level), helpers::calculate_crc32(decompressed_data)};
}

void BlockEncoder::end_block(const uint8_t compression_level) {
	SARC_RUNTIME_ASSERT(m_state == IN_BLOCK_FILE_DATA, state_error, "end_block can only be called when in file data");
	SARC_RUNTIME_ASSERT(m_block_written_file_count == m_block_target_file_count, std::underflow_error,
						"Not enough files added");

	SARC_RUNTIME_ASSERT(compression_level <= 9, std::invalid_argument,
						"Compression lavel must satisfy 0 <= compresison_level <= 9 for LZMA");

	m_stream.seekp(m_block_file_count_offset);
	m_stream.write(reinterpret_cast<const char *>(&m_block_written_file_count), 1);

	uint32_t decompressed_size = 0;
	for (const auto &data : m_block_files_data) decompressed_size += data.size() + 4;

	const auto [compressed_data, decompressed_crc32] =
		sarcbenc_encode_block(decompressed_size, m_block_files_data, compression_level);

	m_stream.seekp(m_block_size_headers_offset);
	m_stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(decompressed_crc32).data()), 4);
	m_stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(decompressed_size).data()), 4);
	m_stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(compressed_data.size()).data()), 4);

	m_stream.write(reinterpret_cast<const char *>(compressed_data.data()), compressed_data.size());

	m_block_files_data.clear();
	m_block_files_data.shrink_to_fit();
	m_block_target_file_count = 0;
	m_block_written_file_count = 0;

	m_state = OUT_OF_BLOCK;
}
