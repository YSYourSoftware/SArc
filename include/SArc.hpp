#pragma once

#include <array>
#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#define SARC_ADD_RUNTIME_ERROR(name)                                                                                   \
	class name : public std::runtime_error {                                                                           \
	public:                                                                                                            \
		explicit name(const std::string &message) : std::runtime_error(message) {}                                     \
		explicit name(const char *message) : std::runtime_error(message) {}                                            \
	}

#define SARC_RUNTIME_ASSERT(condition, error_type, message)                                                            \
	do {                                                                                                               \
		if (!(condition)) throw error_type(message);                                                                   \
	} while (0)

typedef struct rnp_ffi_st *rnp_ffi_t;
typedef struct rnp_key_handle_st *rnp_key_handle_t;

namespace SArc {
	constexpr uint32_t SARC_MAGIC = 0x53417263; // "SArc" in ASCII
	constexpr std::byte SARC_VERSION{0x01};
	constexpr int LZMA_COMPRESSION_LEVEL = 5;

	typedef std::vector<std::byte> bytes_t;
	typedef std::span<std::byte> byte_span_t;
	typedef std::span<const std::byte> byte_span_const_t;

	typedef std::array<uint8_t, 20> pgp_fingerprint_t;

	SARC_ADD_RUNTIME_ERROR(file_not_found_error);
	SARC_ADD_RUNTIME_ERROR(file_already_exists_error);
	SARC_ADD_RUNTIME_ERROR(io_error);
	SARC_ADD_RUNTIME_ERROR(memory_error);
	SARC_ADD_RUNTIME_ERROR(thread_error);

	SARC_ADD_RUNTIME_ERROR(corrupted_data);
	SARC_ADD_RUNTIME_ERROR(malformed_headers);
	SARC_ADD_RUNTIME_ERROR(version_mismatch);

	SARC_ADD_RUNTIME_ERROR(invalid_signature);
	SARC_ADD_RUNTIME_ERROR(invalid_gpg_data);
	SARC_ADD_RUNTIME_ERROR(ffi_error);
	SARC_ADD_RUNTIME_ERROR(sign_error);

	struct CompressStats {
		size_t decompressed_size;
		size_t compressed_size;
	};

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
		 * Initialise with a specified data vector.
		 * </summary>
		 *
		 * @param data Data vector of the new <c>SArchiveFile</c>
		 */
		explicit SArchiveFile(bytes_t data);

		/**
		 * <summary>
		 * Initialise using a part of a specified data vector (part will be copied!).
		 * </summary>
		 *
		 * @param data Data vector to <b>copy a part of</b> to the new <c>SArchiveFile</c>
		 * @param size Size of data to copy
		 * @param offset Offset of data to copy
		 */
		explicit SArchiveFile(const bytes_t &data, size_t size, size_t offset);

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
		 * @param size Number of bytes to read from input stream
		 */
		explicit SArchiveFile(std::istream &stream, size_t size);

		/**
		 * <summary>
		 * Append the serialised data of this file to a data vector.
		 * </summary>
		 *
		 * @param bytes Reference to data vector
		 */
		void serialise_append(bytes_t &bytes) const;

		/**
		 * <summary>
		 * Data vector used by this file.
		 * </summary>
		 */
		bytes_t data;
	};

	/**
	 * <summary>
	 * A SArc Archive in memory.
	 * </summary>
	 */
	class SArchive {
	public:
		virtual ~SArchive() = default;

		/**
		 * <summary>
		 * Serialise this archive and all its files ready to write to disk.
		 * </summary>
		 *
		 * @param compression_level LZMA compression level (0-9)
		 * @param compression_stats (Optional) <c>CompressStats</c> struct to fill out
		 * @returns Byte vector of serialised data
		 */
		[[nodiscard]] virtual bytes_t serialise(uint8_t compression_level, CompressStats *compression_stats) const = 0;

		/**
		 * <summary>
		 * Serialise this archive and all its files to a stream.
		 * </summary>
		 *
		 * @param compression_level LZMA compression level (0-9)
		 * @param stream Output data stream to write to
		 * @param compression_stats (Optional) <c>CompressStats</c> struct to fill out
		 */
		virtual void serialise_to_stream(uint8_t compression_level, std::ostream &stream,
										 CompressStats *compression_stats) const = 0;

		/**
		 * <summary>
		 * Retrive a file by its path.
		 * </summary>
		 *
		 * @param path Path of file to find
		 * @returns Reference to file
		 * @throws file_not_found_error if file is not found in this archive
		 */
		[[nodiscard]] virtual SArchiveFile &get_file_by_path(const std::string &path) = 0;

		/**
		 * <summary>
		 * Get the path of every file in this archive.
		 * </summary>
		 *
		 * @returns Vector containing all file paths
		 */
		[[nodiscard]] virtual std::vector<std::string> get_all_paths() const = 0;

		/**
		 * <summary>
		 * Add a file to this archive.
		 * </summary>
		 *
		 * @param file File to add to this archive
		 * @param path Path of this file
		 * @throws file_already_exists_error if file with path already exists in this archive
		 */
		virtual void add_file(SArchiveFile file, const std::string &path) = 0;

		/**
		 * <summary>
		 * Move a file in this archive to a new path.
		 * </summary>
		 *
		 * @param old_path Path of file
		 * @param new_path New path to move to
		 * @throws file_already_exists_error if file with path already exists in this archive
		 * @throws file_not_found_error if file is not found in this archive
		 */
		virtual void move_file(const std::string &old_path, const std::string &new_path) = 0;

		/**
		 * <summary>
		 * Create a new, blank file in this archive.
		 * </summary>
		 *
		 * @param path Path of this file
		 * @returns Reference to new file
		 * @throws file_already_exists_error if file with path already exists in this archive
		 */
		[[nodiscard]] virtual SArchiveFile &create_file(const std::string &path) = 0;

		/**
		 * <summary>
		 * Delete a file from this archive.
		 * </summary>
		 *
		 * @param path Path of file to delete
		 * @throws file_not_found_error if file is not found in this archive
		 */
		virtual void delete_file(const std::string &path) = 0;
	};

	class SArchiveMemory : public SArchive {
	public:
		SArchiveMemory() = default;
		explicit SArchiveMemory(const bytes_t &serialised);
		explicit SArchiveMemory(const std::filesystem::path &path);
		explicit SArchiveMemory(std::istream &stream, std::size_t size);

		~SArchiveMemory();

		[[nodiscard]] bytes_t serialise(uint8_t compression_level, CompressStats *compression_stats) const override;

		void serialise_to_stream(uint8_t compression_level, std::ostream &stream,
								 CompressStats *compression_stats) const override;

		[[nodiscard]] SArchiveFile &get_file_by_path(const std::string &path) override;
		[[nodiscard]] std::vector<std::string> get_all_paths() const override;

		void add_file(SArchiveFile file, const std::string &path) override;
		void move_file(const std::string &old_path, const std::string &new_path) override;

		[[nodiscard]] SArchiveFile &create_file(const std::string &path) override;
		void delete_file(const std::string &path) override;

		/**
		 * <summary>
		 * Sign the archive using a GPG format secret key.
		 * </summary>
		 *
		 * @param key Secret key data
		 * @param key_passphrase Secret key passphrase
		 * @throws invalid_gpg_data if the key failed to load
		 * @throws ffi_error if initialising or using the FFI object fails
		 * @param key_fingerprint Fingerprint of secret key
		 */
		void sign(const byte_span_const_t &key, const std::string &key_passphrase, const pgp_fingerprint_t &key_fingerprint);
	private:
		void load_from_serialised(const bytes_t &serialised);

		[[nodiscard]] bytes_t sign_data(const byte_span_const_t &data) const;

		std::unordered_map<std::string, SArchiveFile> m_files;

		rnp_ffi_t m_ffi = nullptr;
		rnp_key_handle_t m_signing_key = nullptr;
		std::string m_signing_key_passphrase;
	};
} // namespace SArc
