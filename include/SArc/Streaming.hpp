#pragma once

#include "SArc.hpp"

namespace SArc {
	SARC_ADD_RUNTIME_ERROR(stream_not_supported);

	class SArchiveStream : public SArchive {
	public:
		SArchiveStream();

		explicit SArchiveStream(std::istream &stream);
		explicit SArchiveStream(std::istream &stream, const byte_span_const_t &public_key,
								const pgp_fingerprint_t &key_fingerprint);

		~SArchiveStream() override;

		[[nodiscard]] bool is_streamed() override { return true; }

		[[nodiscard]] bytes_t serialise(CompressionType compression_type, uint8_t compression_level,
										uint32_t block_target_size, const file_block_map_t &file_block_map,
										const progress_callback_t &progress_callback) const override {
			throw stream_not_supported("Streamed archives don't support serialisation");
		}

		void serialise_to_stream(CompressionType compression_type, uint8_t compression_level, std::ostream &stream,
								 uint32_t block_target_size, const file_block_map_t &file_block_map,
								 const progress_callback_t &progress_callback) const override {
			throw stream_not_supported("Streamed archives don't support serialisation");
		}

		[[nodiscard]] SArchiveFile *get_file_by_path(const std::string &path) override {
			throw stream_not_supported("Streamed archives are read-only (did you forget 'const'?)");
		}

		[[nodiscard]] const SArchiveFile *get_file_by_path_const(const std::string &path) const override;
		[[nodiscard]] std::vector<std::string> get_all_paths() const override;

		void add_file(SArchiveFile file, const std::string &path) override {
			throw stream_not_supported("Streamed archives are read-only");
		}

		void move_file(const std::string &old_path, const std::string &new_path) override {
			throw stream_not_supported("Streamed archives are read-only");
		}

		[[nodiscard]] SArchiveFile &create_file(const std::string &path) override {
			throw stream_not_supported("Streamed archives are read-only");
		}

		void delete_file(const std::string &path) override {
			throw stream_not_supported("Streamed archives are read-only");
		}

		/**
		 * <summary>
		 * Map all files in and the start position of the next block.
		 * </summary>
		 *
		 * @param seek_align Whether to seek to the previous block and travel through it, it is recommended to keep this
		 * set to <c>true</c> for general use
		 */
		void map_next_block(bool seek_align = true) const;

		/**
		 * <summary>
		 * Get the index of the latest mapped block. latest mapped block + 1 is the number of blocks mapped.
		 * </summary>
		 *
		 * @return The index of the latest mapped block
		 */
		[[nodiscard]] uint32_t get_latest_mapped_block() const { return m_block_start_offsets.size() - 1; }

		/**
		 * <summary>
		 * Create a memory archive object from this archive, for signing, editing or any other action that modifies the
		 * archive.
		 * </summary>
		 *
		 * @return <c>SArchiveMemory</c> object identical to this archive
		 */
		[[nodiscard]] SArchiveMemory load_into_memory() const;
	private:
		void p_init_ffi();

		void p_verify_headers(bool verify_signature, const pgp_fingerprint_t &key_fingerprint);

		void p_load_public_key(const byte_span_const_t &public_key, const pgp_fingerprint_t &key_fingerprint);

		const std::istream::pos_type m_start_offset;
		mutable std::vector<std::istream::pos_type> m_block_start_offsets;

		std::istream &m_stream;

		uint32_t m_block_count = 0;
		mutable file_block_map_t m_file_block_map;

		rnp_ffi_t m_ffi = nullptr;
		rnp_key_handle_t m_signing_public_key = nullptr;
	};
} // namespace SArc
