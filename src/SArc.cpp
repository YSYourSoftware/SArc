#include "SArc.hpp"

#include "SArc/Helpers.hpp"
#include "SArc/TermColour.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <utility>

using namespace SArc;

#pragma region SArchiveFile

SArchiveFile::SArchiveFile(bytes_t data) : data(std::move(data)) {}
SArchiveFile::SArchiveFile(const bytes_t &data, const size_t size, const size_t offset) : data(data | std::ranges::views::drop(offset) | std::ranges::views::take(size) | std::ranges::to<bytes_t>()) {}
SArchiveFile::SArchiveFile(const std::filesystem::path &path) : data(helpers::read_file(path)) {}
SArchiveFile::SArchiveFile(std::istream &stream, const std::size_t size) {
	this->data.resize(size);
	SARC_RUNTIME_ASSERT(stream.read(reinterpret_cast<char*>(this->data.data()), size), io_error, "Failed to read from stream");
}

void SArchiveFile::serialise_append(bytes_t &bytes) const {
	if (this->data.size() > UINT32_MAX) throw std::overflow_error("Size of data vector larger than UINT32_MAX");

	helpers::emplace_multibyte<uint32_t>(bytes, this->data.size());
	bytes.insert(bytes.end(), this->data.begin(), this->data.end());
}

#pragma endregion
#pragma region SArchive

SArchive::SArchive(const bytes_t &serialised) {this->load_from_serialised(serialised);}
SArchive::SArchive(const std::filesystem::path &path) {this->load_from_serialised(helpers::read_file(path));}
SArchive::SArchive(std::istream &stream, const std::size_t size) {
	bytes_t data(size);
	SARC_RUNTIME_ASSERT(stream.read(reinterpret_cast<char*>(data.data()), size), io_error, "Failed to read from stream");
	this->load_from_serialised(data);
}

bytes_t SArchive::serialise(const uint8_t compression_level, CompressStats *compression_stats) const {
	SARC_RUNTIME_ASSERT(compression_level <= 9, std::invalid_argument, "Compression lavel must satisfy 0 <= compresison_level <= 9 for LZMA");
	
	bytes_t serialised;
	serialised.reserve(
		4 // SArc magic        [uint32_t]
	  + 1 // SArc version      [std::byte]
	  + 4 // File count        [uint32_t]
	  + 4 // CRC32 checksum    [uint32_t]
	  + 8 // Decompressed size [uint64_t]
		// + Compressed data   [bytes_t]
	);

	helpers::emplace_multibyte<uint32_t>(serialised, SARC_MAGIC);
	serialised.emplace_back(SARC_VERSION);
	helpers::emplace_multibyte<uint32_t>(serialised, this->m_files.size());

	size_t uncompressed_size_alloc = 0;
	for (const auto &[filename, file] : this->m_files) {
		uncompressed_size_alloc +=
			filename.size()   // Filename UTF-8         [std::string]
		  + 1                 // String null-terminator [std::byte]
		  + 4                 // Data length            [uint32_t]
		  + file.data.size(); // Data                   [bytes_t]
	}

	bytes_t to_compress;
	to_compress.reserve(uncompressed_size_alloc);

	for (const auto &[filename, file] : this->m_files) {
		helpers::emplace_null_terminated_utf8(to_compress, filename);
		file.serialise_append(to_compress);
	}

	to_compress.shrink_to_fit();

	helpers::emplace_multibyte<uint32_t>(serialised, helpers::calculate_crc32(to_compress));
	if (compression_stats) compression_stats->decompressed_size = to_compress.size();

	bytes_t compressed = helpers::lzma_compress(to_compress, compression_level);
	if (compression_stats) compression_stats->compressed_size = compressed.size();

	helpers::emplace_multibyte<uint64_t>(serialised, to_compress.size());
	serialised.insert(serialised.end(), std::make_move_iterator(compressed.begin()), std::make_move_iterator(compressed.end()));

	serialised.shrink_to_fit();

	return serialised;
}

SArchiveFile &SArchive::get_file_by_path(const std::string& path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	return this->m_files.find(path)->second;
}

const SArchiveFile &SArchive::get_file_by_path(const std::string& path) const {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	return this->m_files.find(path)->second;
}

std::vector<std::string> SArchive::get_all_paths() const {
	std::vector<std::string> paths;
	paths.reserve(this->m_files.size());

	for (const auto &key : this->m_files | std::views::keys) paths.push_back(key);

	return paths;
}

void SArchive::add_file(SArchiveFile file, const std::string &path) {
	SARC_RUNTIME_ASSERT(!this->m_files.contains(path), file_already_exists_error, "File at " + path + " in archive already exists");
	this->m_files.emplace(path, std::move(file));
}

void SArchive::move_file(const std::string &old_path, const std::string &new_path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(old_path), file_not_found_error, "No file at " + old_path + " in archive");
	SARC_RUNTIME_ASSERT(!this->m_files.contains(new_path), file_already_exists_error, "File at " + new_path + " in archive already exists");

	auto node = this->m_files.extract(old_path);
	node.key() = new_path;

	this->m_files.insert(std::move(node));
}

SArchiveFile &SArchive::create_file(const std::string &path) {
	SARC_RUNTIME_ASSERT(!this->m_files.contains(path), file_already_exists_error, "File at " + path + " in archive already exists");

	auto [it, inserted] = this->m_files.emplace(path, SArchiveFile{});
	return it->second;
}

void SArchive::delete_file(const std::string &path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	this->m_files.erase(path);
}

void SArchive::load_from_serialised(const bytes_t &serialised) {
	size_t offset = 0;

	SARC_RUNTIME_ASSERT(helpers::retrieve_multibyte<uint32_t>(serialised, offset) == SARC_MAGIC, malformed_headers, "SArc magic missing"); offset += 4;
	SARC_RUNTIME_ASSERT(serialised.at(offset++) == SARC_VERSION, version_mismatch, "SArc version mismatch");

	const auto file_count = helpers::retrieve_multibyte<uint32_t>(serialised, offset); offset += 4;
	const auto crc32_checksum = helpers::retrieve_multibyte<uint32_t>(serialised, offset); offset += 4;

	const auto decompressed_size = helpers::retrieve_multibyte<uint64_t>(serialised, offset); offset += 8;

	const bytes_t decompressed = helpers::lzma_decompress(std::span(serialised).subspan(offset), decompressed_size);
	SARC_RUNTIME_ASSERT(helpers::calculate_crc32(decompressed) == crc32_checksum, corrupted_data, "CRC32 checksum mismatch");

	offset = 0;
	for (int i = 0; i < file_count; ++i) {
		const std::string file_path = helpers::retrieve_null_terminated_utf8(decompressed, offset); offset += file_path.size() + 1;
		const uint32_t file_size = helpers::retrieve_multibyte<uint32_t>(decompressed, offset); offset += 4;

		this->add_file(SArchiveFile{decompressed, file_size, offset}, file_path);
	}
}
#pragma endregion