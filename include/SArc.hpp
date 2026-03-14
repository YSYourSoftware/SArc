#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace SArc {
	typedef std::vector<std::byte> bytes_t;

	char *bytes_to_buffer(const bytes_t &bytes);

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
			SArchiveFile() = default;
			explicit SArchiveFile(SArcCompression compression_type);
			explicit SArchiveFile(const bytes_t &data, SArcCompression compression_type = SArcCompression::NONE);
			explicit SArchiveFile(const std::filesystem::path &path, SArcCompression compression_type = SArcCompression::NONE);
			explicit SArchiveFile(std::ifstream &file, SArcCompression compression_type = SArcCompression::NONE);

			bytes_t serialize();

			void move_file(const std::string &new_path);

			void set_compression_type(SArcCompression compression_type);
			float get_compression_ratio(SArcCompression compression_type);

			bytes_t get_decompressed_data();
		private:
			std::string m_file_path;
			SArcCompression m_compression_type = SArcCompression::NONE;
			bytes_t m_compressed_data;
	};

	class SArchive {
		public:
			SArchive() = default;
			explicit SArchive(const bytes_t &data);
			explicit SArchive(const std::filesystem::path &path);
			explicit SArchive(std::ifstream &file);

			bytes_t serialize();

			SArchiveFile &get_file_by_path(const std::string &path);
			std::vector<SArchiveFile> get_files();

			void add_file(SArchiveFile file);
		private:
			std::unordered_map<std::string, SArchiveFile> m_files;
	};
}