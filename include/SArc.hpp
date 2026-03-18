#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace SArc {
	typedef std::vector<std::byte> bytes_t;

	/**
	* <summary>
	* A file stored within a <c>SArchive</c>.
	* </summary>
	*/
	class SArchiveFile {
		public:
			SArchiveFile() = default;

			/**
			 * <summary>
			 * Initialise with a specified data vector (will be copied!).
			 * </summary>
			 *
			 * @param data Data vector to <b>copy</b> to the new <c>SArchiveFile</c>
			 */
			explicit SArchiveFile(const bytes_t &data);

			/**
			 * <summary>
			 * Initialise using data from a file.
			 * </summary>
			 *
			 * @param path Path of file to read into data vector
			 */
			explicit SArchiveFile(const std::filesystem::path &path);

			/**
			 * <summary>
			 * Initialise using data from a stream.
			 * </summary>
			 *
			 * @param stream Input stream to read into data vector
			 */
			explicit SArchiveFile(std::istream &stream);

			[[nodiscard]] bytes_t serialize() const;

			bytes_t data;
			std::string file_path;
		private:
	};

	/**
	 * <summary>
	 * </summary>
	 */
	class SArchive {
		public:
			SArchive() = default;
			explicit SArchive(const bytes_t &data);
			explicit SArchive(const std::filesystem::path &path);
			explicit SArchive(std::ifstream &file);

			[[nodiscard]] bytes_t serialize() const;

			[[nodiscard]] SArchiveFile &get_file_by_path(const std::string &path) const;
			[[nodiscard]] std::vector<SArchiveFile> get_files() const;

			void add_file(SArchiveFile file);
		private:
			std::unordered_map<std::string, SArchiveFile> m_files;
	};
}
