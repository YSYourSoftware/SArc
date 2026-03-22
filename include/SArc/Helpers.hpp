#pragma once

#include "SArc.hpp"

#include <span>

namespace SArc::helpers {
	template <typename T> void emplace_multibyte(bytes_t &bytes, T value) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		for (int i = sizeof(T) - 1; i >= 0; --i) bytes.emplace_back(static_cast<std::byte>((value >> (i * 8)) & 0xFF));
	}

	template <typename T> void set_multibyte(bytes_t &bytes, T value, const size_t offset) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		for (size_t i = 0; i < sizeof(T); ++i) bytes.at(offset + i) = static_cast<std::byte>((value >> ((sizeof(T) - 1 - i) * 8)) & 0xFF);
	}

	template <typename T> T retrieve_multibyte(const bytes_t &bytes, const size_t offset) {
		static_assert(std::is_integral_v<T>, "T must be an integral type");

		if (offset + sizeof(T) > bytes.size()) throw std::out_of_range("Attempt to retrieve out of range");

		std::make_unsigned_t<T> value = 0;

		for (size_t i = 0; i < sizeof(T); ++i) value = (value << 8) | std::to_integer<uint8_t>(bytes[offset + i]);

		return static_cast<T>(value);
	}

	bytes_t read_file(const std::filesystem::path &path);

	bytes_t lzma_compress(const byte_span_const_t &data, uint8_t level = 5);
	bytes_t lzma_decompress(const byte_span_const_t &data, size_t decompressed_size);

	inline bytes_t lzma_compress(const bytes_t &data, const uint8_t level = 5) {return lzma_compress(std::span(data), level);}
	inline bytes_t lzma_decompress(const bytes_t &data, const size_t decompressed_size) {return lzma_decompress(std::span(data), decompressed_size);}

	void emplace_null_terminated_utf8(bytes_t &bytes, const std::string &string);
	std::string retrieve_null_terminated_utf8(const byte_span_const_t &bytes, size_t offset);

	inline std::string retrieve_null_terminated_utf8(const bytes_t &bytes, const size_t offset) {return retrieve_null_terminated_utf8(std::span(bytes), offset);}

	uint32_t calculate_crc32(const bytes_t &data);
}