#pragma once

#include "SArc.hpp"

#include <span>

namespace SArc::helpers {
	class BytesStream : public std::streambuf {
	public:
		explicit BytesStream(bytes_t &buffer) : buffer_(buffer) {}
	protected:
		std::streamsize xsputn(const char *s, const std::streamsize n) override {
			const auto begin = reinterpret_cast<const std::byte *>(s);
			buffer_.insert(buffer_.end(), begin, begin + n);
			return n;
		}

		int_type overflow(int_type ch) override {
			if (ch != traits_type::eof()) { buffer_.push_back(static_cast<std::byte>(ch)); }
			return traits_type::not_eof(ch);
		}
	private:
		bytes_t &buffer_;
	};

	class ConstBytesStream : public std::streambuf {
	public:
		explicit ConstBytesStream(const bytes_t &buffer) {
			auto *begin = reinterpret_cast<char *>(const_cast<std::byte *>(buffer.data()));

			setg(begin, begin, begin + buffer.size());
		}
	protected:
		pos_type seekoff(const off_type off, const std::ios_base::seekdir dir,
						 const std::ios_base::openmode which) override {
			if (!(which & std::ios_base::in)) return {static_cast<off_type>(-1)};

			char *newpos = nullptr;

			if (dir == std::ios_base::beg) newpos = eback() + off;
			else if (dir == std::ios_base::cur) newpos = gptr() + off;
			else if (dir == std::ios_base::end) newpos = egptr() + off;

			if (newpos < eback() || newpos > egptr()) return {static_cast<off_type>(-1)};

			setg(eback(), newpos, egptr());

			return gptr() - eback();
		}

		pos_type seekpos(const pos_type sp, const std::ios_base::openmode which) override {
			return seekoff(sp, std::ios_base::beg, which);
		}
	};

	class BytesOStream : public std::ostream {
	public:
		explicit BytesOStream(bytes_t &buffer) : std::ostream(&buf_), buf_(buffer) {}
	private:
		BytesStream buf_;
	};

	class BytesIStream : public std::istream {
	public:
		explicit BytesIStream(const bytes_t &buffer) : std::istream(&buf_), buf_(buffer) {}
	private:
		ConstBytesStream buf_;
	};

	template <typename T> void emplace_multibyte(bytes_t &bytes, T value) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		for (int i = sizeof(T) - 1; i >= 0; --i) bytes.emplace_back(static_cast<std::byte>((value >> (i * 8)) & 0xFF));
	}

	template <typename T> void set_multibyte(bytes_t &bytes, T value, const size_t offset) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		for (size_t i = 0; i < sizeof(T); ++i)
			bytes.at(offset + i) = static_cast<std::byte>((value >> ((sizeof(T) - 1 - i) * 8)) & 0xFF);
	}

	template <typename T> T retrieve_multibyte(const bytes_t &bytes, const size_t offset) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		if (offset + sizeof(T) > bytes.size()) throw std::out_of_range("Attempt to retrieve out of range");

		std::make_unsigned_t<T> value = 0;

		for (size_t i = 0; i < sizeof(T); ++i) value = (value << 8) | std::to_integer<uint8_t>(bytes[offset + i]);

		return static_cast<T>(value);
	}

	template <typename T> [[nodiscard]] bytes_t to_big_endian(T value) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		using UnsignedT = std::make_unsigned_t<T>;
		auto uvalue = static_cast<UnsignedT>(value);

		bytes_t bytes(sizeof(T));

		for (size_t i = 0; i < sizeof(T); ++i) {
			bytes[sizeof(T) - 1 - i] = static_cast<std::byte>(uvalue & 0xFF);
			uvalue >>= 8;
		}

		return bytes;
	}

	template <typename T> [[nodiscard]] T from_big_endian(const byte_span_const_t &bytes) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		if (bytes.size() != sizeof(T)) throw std::invalid_argument("Byte span size does not match target type size");

		using UnsignedT = std::make_unsigned_t<T>;
		UnsignedT value = 0;

		for (std::byte b : bytes) { value = (value << 8) | static_cast<UnsignedT>(b); }

		return static_cast<T>(value);
	}

	typedef struct {
		bool valid;
		pgp_fingerprint_t fingerprint;
	} pgp_sigver_result_t;

	[[nodiscard]] std::string read_null_terminated_utf8(std::istream &stream);

	[[nodiscard]] bytes_t read_file(const std::filesystem::path &path);

	[[nodiscard]] file_block_map_t auto_mappings(const SArchive &archive, uint32_t target_block_size,
												 const file_block_map_t &file_block_map);
	[[nodiscard]] file_block_map_t auto_mappings(std::vector<std::string> files,
												 const std::vector<uint32_t> &file_sizes, uint32_t target_block_size,
												 const file_block_map_t &file_block_map);

	[[nodiscard]] size_t lzma_get_compressed_size(const byte_span_const_t &data, uint8_t level = 5);
	[[nodiscard]] bytes_t lzma_compress(const byte_span_const_t &data, uint8_t level = 5);
	[[nodiscard]] bytes_t lzma_decompress(const byte_span_const_t &data, size_t decompressed_size);

	[[nodiscard]] uint32_t calculate_crc32(const bytes_t &data);

	void archive_serialise_blocks_to_stream(const SArchive &archive, const file_block_map_t &file_block_map,
											uint8_t compression_level, std::ostream &stream,
											const progress_callback_t &progress_callback);

	[[nodiscard]] pgp_sigver_result_t verify_detached_pgg_signature(rnp_ffi_t ffi, const byte_span_const_t &signature,
																	std::istream &stream);
	[[nodiscard]] pgp_fingerprint_t hex_str_to_fingerprint(const std::string &hex);
	[[nodiscard]] std::string fingerprint_to_hex_str(const pgp_fingerprint_t &fingerprint);
} // namespace SArc::helpers
