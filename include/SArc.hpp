#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef SARC_IN_SARC_BUILD
#ifdef WIN32
#define SARC_API __declspec(dllexport)
#elif __GNUC__ || __clang__
#define SARC_API [[gnu::dllexport]]
#endif
#else
#ifdef WIN32
#define SARC_API __declspec(dllimport)
#elif __GNUC__ || __clang__
#define SARC_API [[gnu::dllimport]]
#endif
#endif

#define SARC_ADD_RUNTIME_ERROR(name)                                                                                   \
	class SARC_API name : public std::runtime_error {                                                                  \
	public:                                                                                                            \
		explicit name(const std::string &message) : std::runtime_error(message) {}                                     \
		explicit name(const char *message) : std::runtime_error(message) {}                                            \
	}

#define SARC_RUNTIME_ASSERT(condition, error_type, message)                                                            \
	do {                                                                                                               \
		if (!(condition)) throw error_type(message);                                                                   \
	} while (0)

namespace SArc {
	typedef std::vector<std::byte> bytes_t;
	typedef std::span<std::byte> byte_span_t;
	typedef std::span<const std::byte> byte_span_const_t;

	typedef std::array<std::byte, 32> sha256_hash_t;

	typedef std::unordered_map<std::string, uint32_t> file_block_map_t;

	SARC_ADD_RUNTIME_ERROR(file_not_found_error);
	SARC_ADD_RUNTIME_ERROR(file_already_exists_error);
	SARC_ADD_RUNTIME_ERROR(io_error);
	SARC_ADD_RUNTIME_ERROR(memory_error);
	SARC_ADD_RUNTIME_ERROR(thread_error);

	SARC_ADD_RUNTIME_ERROR(corrupted_data);
	SARC_ADD_RUNTIME_ERROR(malformed_headers);
	SARC_ADD_RUNTIME_ERROR(version_mismatch);

	SARC_ADD_RUNTIME_ERROR(invalid_signature);
} // namespace SArc
