#include "SArc.hpp"

#include "SArc/Helpers.hpp"

#include <fstream>
#include <iostream>
#include <ranges>
#include <utility>

using namespace SArc;

SArchiveMemoryFile::SArchiveMemoryFile(bytes_t data) : data(std::move(data)) {}
SArchiveMemoryFile::SArchiveMemoryFile(const bytes_t &data, const size_t size, const size_t offset) :
	data(data | std::ranges::views::drop(offset) | std::ranges::views::take(size) | std::ranges::to<bytes_t>()) {}

SArchiveMemoryFile::SArchiveMemoryFile(const std::filesystem::path &path) {
	std::ifstream stream{path, std::ios::binary | std::ios::ate};
	const std::streamsize size = stream.tellg();
	stream.seekg(0, std::ios::beg);

	data.resize(size);
	SARC_RUNTIME_ASSERT(stream.read(reinterpret_cast<char *>(data.data()), size), io_error,
						"Failed to read from stream");
}

SArchiveMemoryFile::SArchiveMemoryFile(std::istream &stream, const std::size_t size) {
	data.resize(size);
	SARC_RUNTIME_ASSERT(stream.read(reinterpret_cast<char *>(data.data()), size), io_error,
						"Failed to read from stream");
}

void SArchiveMemoryFile::serialise_append(bytes_t &bytes) const {
	SARC_RUNTIME_ASSERT(data.size() < UINT32_MAX, std::overflow_error, "Size of data vector larger than UINT32_MAX");

	helpers::emplace_multibyte<uint32_t>(bytes, data.size());
	bytes.insert(bytes.end(), data.begin(), data.end());
}

uint32_t SArchiveMemoryFile::get_serialised_size() const { return 4 + data.size(); }
