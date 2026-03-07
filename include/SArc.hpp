#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace SArc {
	enum SArcCompression {
		NONE = 0,
		DEFLATE = 1,
		PPM = 2,
		LZMA = 3,
		ZSTD = 4,
		LZ4 = 5,
		BZIP2 = 6
	};

	class SArchiveFile {
		public:
			SArchiveFile();
			explicit SArchiveFile(const std::vector<std::uint8_t> &data);
			explicit SArchiveFile(const std::filesystem::path &path);
			~SArchiveFile();

			std::vector<std::uint8_t> serialize();
		private:
			std::string m_file_path;
			SArcCompression m_compression_type;
			std::vector<std::uint8_t> m_data;
	};

	class SArchive {
		public:
			SArchive();
			explicit SArchive(const std::vector<std::uint8_t> &data);
			explicit SArchive(const std::filesystem::path &path);
			~SArchive();

			std::vector<std::uint8_t> serialize();
		private:
			std::vector<SArchiveFile> m_files;
	};
}