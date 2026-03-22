#include "SArc.hpp"

#include "SArc/Helpers.hpp"

#include <fstream>
#include <iostream>
#include <ranges>
#include <utility>

using namespace SArc;

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