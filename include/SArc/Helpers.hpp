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
			return ch;
		}
	private:
		bytes_t &buffer_;
	};

	class BytesOStream : public std::ostream {
	public:
		explicit BytesOStream(bytes_t &buffer) : std::ostream(&buf_), buf_(buffer) {}
	private:
		BytesStream buf_;
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

	template <typename T> std::vector<uint8_t> to_big_endian(T value) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		using UnsignedT = std::make_unsigned_t<T>;
		auto uvalue = static_cast<UnsignedT>(value);

		std::vector<uint8_t> bytes(sizeof(T));

		for (size_t i = 0; i < sizeof(T); ++i) {
			bytes[sizeof(T) - 1 - i] = static_cast<uint8_t>(uvalue & 0xFF);
			uvalue >>= 8;
		}

		return bytes;
	}

	file_block_map_t auto_mappings(const SArchive &archive, uint32_t target_block_size,
								   const file_block_map_t &file_block_map);

	bytes_t read_file(const std::filesystem::path &path);

	size_t lzma_get_compressed_size(const byte_span_const_t &data, uint8_t level = 5);
	bytes_t lzma_compress(const byte_span_const_t &data, uint8_t level = 5);
	bytes_t lzma_decompress(const byte_span_const_t &data, size_t decompressed_size);

	void emplace_null_terminated_utf8(bytes_t &bytes, const std::string &string);
	std::string retrieve_null_terminated_utf8(const byte_span_const_t &bytes, size_t offset);

	uint32_t calculate_crc32(const bytes_t &data);

	void archive_serialise_blocks_to_stream(const SArchive &archive, const file_block_map_t &file_block_map,
											uint8_t compression_level, std::ostream &stream);
} // namespace SArc::helpers
