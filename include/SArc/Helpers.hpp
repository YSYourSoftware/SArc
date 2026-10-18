#pragma once

#include "SArc.hpp"

#include <span>

namespace SArc::helpers {
	SARC_ADD_RUNTIME_ERROR(crypto_error);

	class SARC_API BytesStream : public std::streambuf {
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

	class SARC_API ConstBytesStream : public std::streambuf {
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

	class SARC_API BytesOStream : public std::ostream {
	public:
		explicit BytesOStream(bytes_t &buffer) : std::ostream(&buf_), buf_(buffer) {}
	private:
		BytesStream buf_;
	};

	class SARC_API BytesIStream : public std::istream {
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

	[[nodiscard]] SARC_API std::string read_null_terminated_utf8(std::istream &stream);

	[[nodiscard]] SARC_API bytes_t read_file(const std::filesystem::path &path);

	[[nodiscard]] SARC_API file_block_map_t auto_mappings(std::vector<std::string> files,
														  const std::vector<uint32_t> &file_sizes,
														  uint32_t target_block_size,
														  const file_block_map_t &file_block_map);

	[[nodiscard]] SARC_API bytes_t lzma_compress(const byte_span_const_t &data, uint8_t level = 5);
	[[nodiscard]] SARC_API bytes_t lzma_decompress(const byte_span_const_t &data, size_t decompressed_size);

	[[nodiscard]] SARC_API bytes_t lz4_compress(const byte_span_const_t &data, uint8_t level = 5);
	[[nodiscard]] SARC_API bytes_t lz4_decompress(const byte_span_const_t &data, size_t decompressed_size);

	[[nodiscard]] SARC_API bytes_t oneshot_aes256_encrypt(const byte_span_const_t &data, const std::string &passphrase);
	[[nodiscard]] SARC_API bytes_t oneshot_aes256_decrypt(const byte_span_const_t &data, const std::string &passphrase);

	[[nodiscard]] SARC_API sha256_hash_t fingerprint_key(const byte_span_const_t &public_key);

	[[nodiscard]] SARC_API bytes_t sign_data(const byte_span_const_t &data, const byte_span_const_t &private_key);
	[[nodiscard]] SARC_API bytes_t sign_data(std::istream &stream, size_t length, const byte_span_const_t &private_key);
	[[nodiscard]] SARC_API bool validate_signature(const byte_span_const_t &data, const byte_span_const_t &signature,
												   const byte_span_const_t &public_key,
												   const sha256_hash_t *fingerprint = nullptr);
	[[nodiscard]] SARC_API bool validate_signature(std::istream &stream, size_t length, const byte_span_const_t &signature,
												   const byte_span_const_t &public_key,
												   const sha256_hash_t *fingerprint = nullptr);

	[[nodiscard]] SARC_API uint32_t calculate_crc32(std::istream &stream, size_t len);
	[[nodiscard]] SARC_API uint32_t calculate_crc32(const bytes_t &data);
	[[nodiscard]] SARC_API std::array<std::byte, 32> calculate_sha256(std::istream &stream, size_t len);

	[[nodiscard]] SARC_API sha256_hash_t hex_str_to_fingerprint(const std::string &hex);
	[[nodiscard]] SARC_API std::string fingerprint_to_hex_str(const sha256_hash_t &fingerprint);
} // namespace SArc::helpers
