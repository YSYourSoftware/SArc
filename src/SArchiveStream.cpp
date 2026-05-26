#include "SArc/Streaming.hpp"

#include "SArc/Helpers.hpp"

#include <rnp/rnp.h>
#include <rnp/rnp_err.h>

#include <fstream>
#include <iostream>
#include <ranges>

#include "7zWindows.h"

using namespace SArc;

static helpers::pgp_sigver_result_t sarcstm_stream_verify_sig(std::istream &serialised, rnp_ffi_t ffi,
															  const bool verify_signature) {
	std::array<uint8_t, 4> tempvar_buf{};

	serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 2);
	const auto signature_size =
		helpers::from_big_endian<uint16_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 2});

	auto *signature_data = new char[signature_size];
	serialised.read(signature_data, signature_size);

	if (!verify_signature) {
		delete[] signature_data;

		return {false, {}};
	}

	const std::streamsize block_data_offset = serialised.tellg();

	try {
		const helpers::pgp_sigver_result_t result = helpers::verify_detached_pgg_signature(
			ffi, {reinterpret_cast<std::byte *>(signature_data), signature_size}, serialised);

		serialised.clear();
		serialised.seekg(block_data_offset, std::ios::beg);

		delete[] signature_data;

		return result;
	} catch (const std::exception &e) {
		serialised.clear();
		serialised.seekg(block_data_offset, std::ios::beg);

		delete[] signature_data;
		throw;
	}
}

static bytes_t sarcstm_stream_block_get_file_data(std::istream &stream, const std::string &path) {
	std::array<uint8_t, 4> tempvar_buf{};
	stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 1);
	const uint8_t file_count = tempvar_buf[0];

	uint8_t target_index = 0;
	bool found = false;

	for (uint8_t i = 0; i < file_count; i++) {
		if (helpers::read_null_terminated_utf8(stream) == path) {
			found = true;
			target_index = i;
		}
	}

	SARC_RUNTIME_ASSERT(found, file_not_found_error, "File " + path + " not found in block");

	stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	const auto decompressed_crc32 =
		helpers::from_big_endian<uint32_t>({reinterpret_cast<const std::byte *>(tempvar_buf.data()), 4});

	stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	const auto decompressed_size =
		helpers::from_big_endian<uint32_t>({reinterpret_cast<const std::byte *>(tempvar_buf.data()), 4});

	stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	const auto compressed_size =
		helpers::from_big_endian<uint32_t>({reinterpret_cast<const std::byte *>(tempvar_buf.data()), 4});

	const auto compressed_data = new char[compressed_size];

	bytes_t decompressed_data;

	try {
		stream.read(compressed_data, compressed_size);
		decompressed_data = helpers::lzma_decompress(
			{reinterpret_cast<const std::byte *>(compressed_data), compressed_size}, decompressed_size);
	} catch (std::exception &e) {
		delete[] compressed_data;
		throw;
	}

	delete[] compressed_data;

	SARC_RUNTIME_ASSERT(helpers::calculate_crc32(decompressed_data) == decompressed_crc32, corrupted_data,
						"Block CRC32 mismatch");

	helpers::BytesIStream byte_stream{decompressed_data};

	for (uint8_t i = 0; i < target_index; i++) {
		byte_stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
		byte_stream.seekg(
			helpers::from_big_endian<uint32_t>({reinterpret_cast<const std::byte *>(tempvar_buf.data()), 4}),
			std::ios::cur);
	}

	byte_stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	bytes_t result{helpers::from_big_endian<uint32_t>({reinterpret_cast<const std::byte *>(tempvar_buf.data()), 4})};

	byte_stream.read(reinterpret_cast<char *>(result.data()), result.size());

	return result;
}

SArchiveStream::SArchiveStream(std::istream &stream) : m_start_offset(stream.tellg()), m_stream(stream) {
	p_init_ffi();
	p_verify_headers(false, {});
}

SArchiveStream::SArchiveStream(std::istream &stream, const byte_span_const_t &public_key,
							   const pgp_fingerprint_t &key_fingerprint) :
	m_start_offset(stream.tellg()), m_stream(stream) {
	p_init_ffi();
	p_load_public_key(public_key, key_fingerprint);
	p_verify_headers(true, key_fingerprint);
}

SArchiveStream::~SArchiveStream() {
	if (m_ffi) rnp_ffi_destroy(m_ffi);
	if (m_signing_public_key) rnp_key_handle_destroy(m_signing_public_key);
}

const SArchiveFile *SArchiveStream::get_file_by_path_const(const std::string &path) const {
	if (m_file_block_map.contains(path)) {
		const uint32_t block = m_file_block_map.at(path);
		m_stream.seekg(m_block_start_offsets[block], std::ios::beg);

		auto *file = new SArchiveFile();
		file->data = sarcstm_stream_block_get_file_data(m_stream, path);
		return file;
	}

	map_next_block();

	for (uint32_t block = get_latest_mapped_block() + 1; block <= m_block_count; block++) {
		if (m_file_block_map.contains(path)) {
			m_stream.seekg(m_block_start_offsets[block], std::ios::beg);

			auto *file = new SArchiveFile();
			file->data = sarcstm_stream_block_get_file_data(m_stream, path);
			return file;
		}

		if (block < m_block_count) map_next_block();
	}

	throw file_not_found_error("File " + path + " not found in archive");
}

std::vector<std::string> SArchiveStream::get_all_paths() const {
	while (get_latest_mapped_block() < m_block_count - 1) map_next_block();

	std::vector<std::string> paths;
	paths.reserve(m_file_block_map.size());

	for (const auto &path : m_file_block_map | std::views::keys) { paths.emplace_back(path); }

	return paths;
}

void SArchiveStream::map_next_block(const bool seek_align) const {
	const uint32_t latest_block_mapped = get_latest_mapped_block();

	if (seek_align) {
		m_stream.seekg(m_block_start_offsets[latest_block_mapped], std::ios::beg);

		uint8_t block_file_count;
		m_stream.read(reinterpret_cast<char *>(&block_file_count), 1);

		// We've already mapped this block, so we can just quickly go through it
		for (uint8_t i = 0; i < block_file_count; i++) helpers::read_null_terminated_utf8(m_stream);

		m_stream.seekg(8, std::ios::cur);

		uint32_t compressed_size;
		m_stream.read(reinterpret_cast<char *>(&compressed_size), 4);

		m_stream.seekg(compressed_size, std::ios::cur);
	}

	m_block_start_offsets.push_back(m_stream.tellg());

	const uint32_t block = latest_block_mapped + 1;

	m_stream.seekg(m_block_start_offsets[block], std::ios::beg);

	uint8_t block_file_count;
	m_stream.read(reinterpret_cast<char *>(&block_file_count), 1);

	for (uint8_t i = 0; i < block_file_count; i++) {
		const std::string file_path = helpers::read_null_terminated_utf8(m_stream);
		m_file_block_map.emplace(file_path, block);
	}
}

SArchiveMemory SArchiveStream::load_into_memory() const {
	m_stream.seekg(m_start_offset, std::ios::beg);
	return SArchiveMemory(m_stream);
}

void SArchiveStream::p_init_ffi() {
	SARC_RUNTIME_ASSERT(rnp_ffi_create(&m_ffi, RNP_KEYSTORE_GPG, RNP_KEYSTORE_GPG) == RNP_SUCCESS, ffi_error,
						"Failed to create FFI object");
}

void SArchiveStream::p_verify_headers(const bool verify_signature, const pgp_fingerprint_t &key_fingerprint) {
	m_stream.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);

	std::array<uint8_t, 4> tempvar_buf{};
	m_stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	SARC_RUNTIME_ASSERT(std::memcmp(tempvar_buf.data(), helpers::to_big_endian<uint32_t>(SARC_MAGIC).data(), 4) == 0,
						malformed_headers, "SArc magic missing");

	m_stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 1);
	SARC_RUNTIME_ASSERT(*reinterpret_cast<std::byte *>(tempvar_buf.data()) == SARC_VERSION, version_mismatch,
						std::format("SArc version mismatch (expected {}, got {})", +static_cast<uint8_t>(SARC_VERSION),
									+tempvar_buf[0]));

	m_stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	m_block_count = helpers::from_big_endian<uint32_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 4});
	m_block_start_offsets.reserve(m_block_count);

	m_stream.read(reinterpret_cast<char *>(tempvar_buf.data()), 1);
	if (tempvar_buf[0] == 0x01) {
		const auto [valid, fingerprint] = sarcstm_stream_verify_sig(m_stream, m_ffi, verify_signature);

		if (verify_signature) {
			SARC_RUNTIME_ASSERT(valid, invalid_signature, "Signature not valid");
			SARC_RUNTIME_ASSERT(std::memcmp(fingerprint.data(), key_fingerprint.data(), 20) == 0, invalid_signature,
								std::format("Signature using incorrect fingerprint (expected {}, got {})",
											helpers::fingerprint_to_hex_str(key_fingerprint),
											helpers::fingerprint_to_hex_str(fingerprint)));
		}
	}

	m_stream.clear();

	m_block_start_offsets.push_back(m_stream.tellg());

	m_stream.seekg(m_block_start_offsets[0], std::ios::beg);

	uint8_t block_file_count;
	m_stream.read(reinterpret_cast<char *>(&block_file_count), 1);

	for (uint8_t i = 0; i < block_file_count; i++) {
		const std::string file_path = helpers::read_null_terminated_utf8(m_stream);
		m_file_block_map.emplace(file_path, 0);
	}
}

void SArchiveStream::p_load_public_key(const byte_span_const_t &public_key, const pgp_fingerprint_t &key_fingerprint) {
	rnp_input_t gpg_key;

	SARC_RUNTIME_ASSERT(rnp_input_from_memory(&gpg_key, reinterpret_cast<const uint8_t *>(public_key.data()),
											  public_key.size(), true) == RNP_SUCCESS,
						invalid_gpg_data, "Failed to load public key data");

	SARC_RUNTIME_ASSERT(rnp_load_keys(m_ffi, RNP_KEYSTORE_GPG, gpg_key, RNP_LOAD_SAVE_PUBLIC_KEYS) == RNP_SUCCESS,
						invalid_gpg_data, "Failed to load public keys from data");

	rnp_input_destroy(gpg_key);
	gpg_key = nullptr;

	const std::string fp_hex = helpers::fingerprint_to_hex_str(key_fingerprint);

	SARC_RUNTIME_ASSERT(rnp_locate_key(m_ffi, "fingerprint", fp_hex.c_str(), &m_signing_public_key) == RNP_SUCCESS,
						invalid_gpg_data, "Failed to locate signing key by fingerprint");
}
