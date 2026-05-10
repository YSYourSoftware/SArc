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

bytes_t SArchiveMemory::serialise(const uint8_t compression_level, CompressStats *compression_stats) const {
	SARC_RUNTIME_ASSERT(compression_level <= 9, std::invalid_argument,
						"Compression lavel must satisfy 0 <= compresison_level <= 9 for LZMA");

	const bool sign_archive = m_signing_key != nullptr;

	size_t uncompressed_size_alloc = 0;
	for (const auto &[filename, file] : this->m_files) {
		uncompressed_size_alloc += filename.size()	   // Filename UTF-8         [std::string]
								   + 1				   // String null-terminator [std::byte]
								   + 4				   // Data length            [uint32_t]
								   + file.data.size(); // Data                   [bytes_t]
	}

	bytes_t to_compress;
	to_compress.reserve(uncompressed_size_alloc);

	for (const auto &[filename, file] : this->m_files) {
		helpers::emplace_null_terminated_utf8(to_compress, filename);
		file.serialise_append(to_compress);
	}

	to_compress.shrink_to_fit();

	bytes_t compressed = helpers::lzma_compress(to_compress, compression_level);

	if (compression_stats) compression_stats->decompressed_size = to_compress.size();
	if (compression_stats) compression_stats->compressed_size = compressed.size();

	bytes_t sign_data;

	if (sign_archive) sign_data = this->sign_data(compressed);

	bytes_t serialised;
	serialised.reserve(4										// SArc magic        [uint32_t]
					   + 1										// SArc version      [uint8_t]
					   + 4										// File count        [uint32_t]
					   + 1										// Is signed?        [bool]
					   + (sign_archive ? 2						// Signing data size [uint16_t]
											 + sign_data.size() // Signing data      [bytes_t]
									   : 0) +
					   4				   // CRC32 checksum    [uint32_t]
					   + 8				   // Decompressed size [uint64_t]
					   + compressed.size() // Compressed data   [bytes_t]
	);

	helpers::emplace_multibyte<uint32_t>(serialised, SARC_MAGIC);
	serialised.emplace_back(SARC_VERSION);
	helpers::emplace_multibyte<uint32_t>(serialised, this->m_files.size());

	serialised.emplace_back(static_cast<std::byte>(sign_archive ? 0x00 : 0x01));
	if (sign_archive)
		serialised.insert(serialised.end(), std::make_move_iterator(sign_data.begin()),
						  std::make_move_iterator(sign_data.end()));

	helpers::emplace_multibyte<uint32_t>(serialised, helpers::calculate_crc32(to_compress));

	helpers::emplace_multibyte<uint64_t>(serialised, to_compress.size());
	serialised.insert(serialised.end(), std::make_move_iterator(compressed.begin()),
					  std::make_move_iterator(compressed.end()));

	serialised.shrink_to_fit();

	return serialised;
}

void SArchiveMemory::serialise_to_stream(uint8_t compression_level, std::ostream &stream,
										 CompressStats *compression_stats) const {
	uint8_t var_u8;

	const bool sign_archive = m_signing_key != nullptr;

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(SARC_MAGIC).data()), sizeof(SARC_MAGIC));
	stream.write(reinterpret_cast<const char *>(&SARC_VERSION), 1);

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(this->m_files.size()).data()), 4);

	var_u8 = sign_archive ? 0x01 : 0x00;
	stream.write(reinterpret_cast<const char *>(&var_u8), 1);

	if (sign_archive) {

	}

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(0).data()), 4);
}

SArchiveFile &SArchiveMemory::get_file_by_path(const std::string &path) {
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
	size_t offset = 0;

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
	}
}
