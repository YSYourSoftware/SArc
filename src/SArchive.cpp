#include "SArc.hpp"

#include "Sarc/Helpers.hpp"

#include <iostream>
#include <ranges>

using namespace SArc;

SArchiveMemory::SArchiveMemory(const bytes_t &serialised) { this->load_from_serialised(serialised); }
SArchiveMemory::SArchiveMemory(const std::filesystem::path &path) {
	this->load_from_serialised(helpers::read_file(path));
}

SArchiveMemory::SArchiveMemory(std::istream &stream, const std::size_t size) {
	bytes_t data(size);
	SARC_RUNTIME_ASSERT(stream.read(reinterpret_cast<char *>(data.data()), size), io_error,
						"Failed to read from stream");
	this->load_from_serialised(data);
}

bytes_t SArchiveMemory::serialise(const uint8_t compression_level, const uint32_t block_target_size,
								  const file_block_map_t &file_block_map) const {
	bytes_t serialised;
	helpers::BytesOStream out{serialised};

	serialise_to_stream(compression_level, out, block_target_size, file_block_map);

	serialised.shrink_to_fit();
	return serialised;
}

void SArchiveMemory::serialise_to_stream(const uint8_t compression_level, std::ostream &stream,
										 const uint32_t block_target_size,
										 const file_block_map_t &file_block_map) const {
	SARC_RUNTIME_ASSERT(compression_level <= 9, std::invalid_argument,
						"Compression lavel must satisfy 0 <= compresison_level <= 9 for LZMA");

	const bool pgp_signed = m_signing_key != nullptr;
	const file_block_map_t final_file_block_map = helpers::auto_mappings(*this, block_target_size, file_block_map);

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(SARC_MAGIC).data()), sizeof(uint32_t));
	stream.write(reinterpret_cast<const char *>(&SARC_VERSION), 1);

	uint32_t block_count = 0;
	for (const auto block : final_file_block_map | std::views::values)
		if (block > block_count) block_count = block;

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(block_count).data()), sizeof(uint32_t));

	const uint8_t temp = pgp_signed ? 0x01 : 0x00;
	stream.write(reinterpret_cast<const char *>(&temp), 1);

	if (pgp_signed) {
		bytes_t buffer;
		helpers::BytesOStream out{buffer};

		helpers::archive_serialise_blocks_to_stream(*this, final_file_block_map, compression_level, out);

		const bytes_t signature = sign_data(buffer);

		stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint16_t>(signature.size()).data()), sizeof(uint16_t));
		stream.write(reinterpret_cast<const char *>(signature.data()), signature.size());

		stream.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());

		return;
	}

	helpers::archive_serialise_blocks_to_stream(*this, final_file_block_map, compression_level, stream);
}

SArchiveFile &SArchiveMemory::get_file_by_path(const std::string &path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	return this->m_files.find(path)->second;
}

const SArchiveFile &SArchiveMemory::get_file_by_path_const(const std::string &path) const {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	return this->m_files.find(path)->second;
}

std::vector<std::string> SArchiveMemory::get_all_paths() const {
	std::vector<std::string> paths;
	paths.reserve(this->m_files.size());

	for (const auto &key : this->m_files | std::views::keys) paths.push_back(key);

	return paths;
}

void SArchiveMemory::add_file(SArchiveFile file, const std::string &path) {
	SARC_RUNTIME_ASSERT(!this->m_files.contains(path), file_already_exists_error,
						"File at " + path + " in archive already exists");
	this->m_files.emplace(path, std::move(file));
}

void SArchiveMemory::move_file(const std::string &old_path, const std::string &new_path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(old_path), file_not_found_error,
						"No file at " + old_path + " in archive");
	SARC_RUNTIME_ASSERT(!this->m_files.contains(new_path), file_already_exists_error,
						"File at " + new_path + " in archive already exists");

	auto node = this->m_files.extract(old_path);
	node.key() = new_path;

	this->m_files.insert(std::move(node));
}

SArchiveFile &SArchiveMemory::create_file(const std::string &path) {
	SARC_RUNTIME_ASSERT(!this->m_files.contains(path), file_already_exists_error,
						"File at " + path + " in archive already exists");

	auto [it, inserted] = this->m_files.emplace(path, SArchiveFile{});
	return it->second;
}

void SArchiveMemory::delete_file(const std::string &path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	this->m_files.erase(path);
}

void SArchiveMemory::load_from_serialised(const bytes_t &serialised) {
	/*size_t offset = 0;

	SARC_RUNTIME_ASSERT(helpers::retrieve_multibyte<uint32_t>(serialised, offset) == SARC_MAGIC, malformed_headers,
						"SArc magic missing");
	offset += 4;
	SARC_RUNTIME_ASSERT(serialised.at(offset++) == SARC_VERSION, version_mismatch, "SArc version mismatch");

	const auto file_count = helpers::retrieve_multibyte<uint32_t>(serialised, offset);
	offset += 4;
	const auto crc32_checksum = helpers::retrieve_multibyte<uint32_t>(serialised, offset);
	offset += 4;

	const auto decompressed_size = helpers::retrieve_multibyte<uint64_t>(serialised, offset);
	offset += 8;

	const bytes_t decompressed = helpers::lzma_decompress(std::span(serialised).subspan(offset), decompressed_size);
	SARC_RUNTIME_ASSERT(helpers::calculate_crc32(decompressed) == crc32_checksum, corrupted_data,
						"CRC32 checksum mismatch");

	offset = 0;
	for (int i = 0; i < file_count; ++i) {
		const std::string file_path = helpers::retrieve_null_terminated_utf8(decompressed, offset);
		offset += file_path.size() + 1;
		const auto file_size = helpers::retrieve_multibyte<uint32_t>(decompressed, offset);
		offset += 4;

		this->add_file(SArchiveFile{decompressed, file_size, offset}, file_path);
		offset += file_size;
	}*/
}
