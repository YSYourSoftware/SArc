#include "SArc.hpp"

#include <print>

using namespace SArc;

bytes_t compress(const bytes_t &data, const SArcCompression compression_type) {
	switch (compression_type) {
		case NONE:
			return data;
		case DEFLATE:
			break;
		case LZMA:
			break;
		case ZSTD:
			break;
		case LZ4:
			break;
		case BZIP2:
			break;
	}

	throw std::runtime_error(std::format("Compression type not regognised or supported: %X", static_cast<int>(compression_type)));
};

SArchiveFile::SArchiveFile(const SArcCompression compression_type) {
	this->m_compression_type = compression_type;
}

SArchiveFile::SArchiveFile(const bytes_t &data, const SArcCompression compression_type) {
	this->m_compression_type = compression_type;
}