#include "SArc.hpp"

#include "Sarc/Helpers.hpp"

#include <fstream>
#include <iostream>
#include <ranges>

using namespace SArc;

SArchiveMemory::SArchiveMemory(const bytes_t &serialised) {
	p_init_ffi();
	p_load_from_serialised(serialised, false, {}, {});
}

SArchiveMemory::SArchiveMemory(const bytes_t &serialised, const byte_span_const_t &public_key,
							   const pgp_fingerprint_t &key_fingerprint) {
	p_init_ffi();
	p_load_from_serialised(serialised, true, public_key, key_fingerprint);
}

SArchiveMemory::SArchiveMemory(const std::filesystem::path &path) {
	p_init_ffi();
	std::ifstream in{path};
	p_load_from_serialised_stream(in, false, {}, {});
}

SArchiveMemory::SArchiveMemory(const std::filesystem::path &path, const byte_span_const_t &public_key,
							   const pgp_fingerprint_t &key_fingerprint) {
	p_init_ffi();
	std::ifstream in{path};
	p_load_from_serialised_stream(in, true, public_key, key_fingerprint);
}

SArchiveMemory::SArchiveMemory(std::istream &stream) {
	p_init_ffi();
	p_load_from_serialised_stream(stream, false, {}, {});
}

SArchiveMemory::SArchiveMemory(std::istream &stream, const byte_span_const_t &public_key,
							   const pgp_fingerprint_t &key_fingerprint) {
	p_init_ffi();
	p_load_from_serialised_stream(stream, true, public_key, key_fingerprint);
}

bytes_t SArchiveMemory::serialise(const uint8_t compression_level, const uint32_t block_target_size,
								  const file_block_map_t &file_block_map,
								  const progress_callback_t &progress_callback) const {
	bytes_t serialised;
	helpers::BytesOStream out{serialised};

	serialise_to_stream(compression_level, out, block_target_size, file_block_map, progress_callback);

	serialised.shrink_to_fit();
	return serialised;
}

void SArchiveMemory::serialise_to_stream(const uint8_t compression_level, std::ostream &stream,
										 const uint32_t block_target_size, const file_block_map_t &file_block_map,
										 const progress_callback_t &progress_callback) const {
	SARC_RUNTIME_ASSERT(compression_level <= 9, std::invalid_argument,
						"Compression lavel must satisfy 0 <= compresison_level <= 9 for LZMA");

	const bool pgp_signed = m_signing_key != nullptr;
	const file_block_map_t final_file_block_map = helpers::auto_mappings(*this, block_target_size, file_block_map);

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(SARC_MAGIC).data()), 4);
	stream.write(reinterpret_cast<const char *>(&SARC_VERSION), 1);

	uint32_t block_count = 0;
	for (const auto block : final_file_block_map | std::views::values)
		if (block >= block_count) block_count = block + 1;

	stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint32_t>(block_count).data()), 4);

	const uint8_t temp = pgp_signed ? 0x01 : 0x00;
	stream.write(reinterpret_cast<const char *>(&temp), 1);

	if (pgp_signed) {
		bytes_t buffer;
		helpers::BytesOStream out{buffer};

		helpers::archive_serialise_blocks_to_stream(*this, final_file_block_map, compression_level, out,
													progress_callback);

		const bytes_t signature = p_sign_data(buffer);

		stream.write(reinterpret_cast<const char *>(helpers::to_big_endian<uint16_t>(signature.size()).data()), 2);
		stream.write(reinterpret_cast<const char *>(signature.data()), signature.size());

		stream.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());

		return;
	}

	helpers::archive_serialise_blocks_to_stream(*this, final_file_block_map, compression_level, stream,
												progress_callback);
}

SArchiveFile *SArchiveMemory::get_file_by_path(const std::string &path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	return &this->m_files.find(path)->second;
}

const SArchiveFile *SArchiveMemory::get_file_by_path_const(const std::string &path) const {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	return &this->m_files.find(path)->second;
}

std::vector<std::string> SArchiveMemory::get_all_paths() const {
	std::vector<std::string> paths;
	paths.reserve(this->m_files.size());

	for (const auto &key : this->m_files | std::views::keys) paths.push_back(key);

	return paths;
}

void SArchiveMemory::add_file(SArchiveFile file, const std::string &path) {
	SARC_RUNTIME_ASSERT(!this->m_files.contains(path), file_already_exists_error,
						"File at " + path + " in archive already exists");
	this->m_files.emplace(path, std::move(file));
}

void SArchiveMemory::move_file(const std::string &old_path, const std::string &new_path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(old_path), file_not_found_error,
						"No file at " + old_path + " in archive");
	SARC_RUNTIME_ASSERT(!this->m_files.contains(new_path), file_already_exists_error,
						"File at " + new_path + " in archive already exists");

	auto node = this->m_files.extract(old_path);
	node.key() = new_path;

	this->m_files.insert(std::move(node));
}

SArchiveFile &SArchiveMemory::create_file(const std::string &path) {
	SARC_RUNTIME_ASSERT(!this->m_files.contains(path), file_already_exists_error,
						"File at " + path + " in archive already exists");

	auto [it, inserted] = this->m_files.emplace(path, SArchiveFile{});
	return it->second;
}

void SArchiveMemory::delete_file(const std::string &path) {
	SARC_RUNTIME_ASSERT(this->m_files.contains(path), file_not_found_error, "No file at " + path + " in archive");
	this->m_files.erase(path);
}

void SArchiveMemory::p_load_from_serialised(const bytes_t &serialised, const bool verify_signature,
											const byte_span_const_t &public_key,
											const pgp_fingerprint_t &key_fingerprint) {
	helpers::BytesIStream stream{serialised};
	p_load_from_serialised_stream(stream, verify_signature, public_key, key_fingerprint);
}

static helpers::pgp_sigver_result_t sarcmem_stream_verify_sig(std::istream &serialised, rnp_ffi_t ffi,
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

void SArchiveMemory::p_load_from_serialised_stream(std::istream &serialised, const bool verify_signature,
												   const byte_span_const_t &public_key,
												   const pgp_fingerprint_t &key_fingerprint) {
	std::array<uint8_t, 4> tempvar_buf{};
	serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	SARC_RUNTIME_ASSERT(std::memcmp(tempvar_buf.data(), helpers::to_big_endian<uint32_t>(SARC_MAGIC).data(), 4) == 0,
						malformed_headers, "SArc magic missing");

	serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 1);
	SARC_RUNTIME_ASSERT(*reinterpret_cast<std::byte *>(tempvar_buf.data()) == SARC_VERSION, version_mismatch,
						std::format("SArc version mismatch (expected {}, got {})", +static_cast<uint8_t>(SARC_VERSION),
									+tempvar_buf[0]));

	serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
	const auto block_count = helpers::from_big_endian<uint32_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 4});

	serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 1);
	if (tempvar_buf[0] == 0x01) {
		if (verify_signature) p_load_public_key(public_key, key_fingerprint);

		const auto [valid, fingerprint] = sarcmem_stream_verify_sig(serialised, m_ffi, verify_signature);

		if (verify_signature) {
			SARC_RUNTIME_ASSERT(valid, invalid_signature, "Signature not valid");
			SARC_RUNTIME_ASSERT(std::memcmp(fingerprint.data(), key_fingerprint.data(), 20) == 0, invalid_signature,
								std::format("Signature using incorrect fingerprint (expected {}, got {})",
											helpers::fingerprint_to_hex_str(key_fingerprint),
											helpers::fingerprint_to_hex_str(fingerprint)));
		}
	}

	for (uint32_t block = 0; block < block_count; block++) {
		serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 1);
		const uint8_t file_count = tempvar_buf[0];

		std::vector<std::string> paths;
		paths.reserve(file_count);

		for (uint8_t i = 0; i < file_count; i++) paths.push_back(helpers::read_null_terminated_utf8(serialised));

		serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
		const uint32_t decompressed_crc32 =
			helpers::from_big_endian<uint32_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 4});

		serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
		const uint32_t decompressed_size =
			helpers::from_big_endian<uint32_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 4});

		serialised.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
		const uint32_t compressed_size =
			helpers::from_big_endian<uint32_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 4});

		bytes_t compressed_data{compressed_size};
		serialised.read(reinterpret_cast<char *>(compressed_data.data()), compressed_size);

		const bytes_t decompressed_data = helpers::lzma_decompress(compressed_data, decompressed_size);
		helpers::BytesIStream in{decompressed_data};

		SARC_RUNTIME_ASSERT(decompressed_crc32 == helpers::calculate_crc32(decompressed_data), corrupted_data,
							std::format("Block {} CRC32 mismatch", block));

		for (const auto &path : paths) {
			in.read(reinterpret_cast<char *>(tempvar_buf.data()), 4);
			const uint32_t file_size =
				helpers::from_big_endian<uint32_t>({reinterpret_cast<std::byte *>(tempvar_buf.data()), 4});

			SArchiveFile &file = create_file(path);
			file.data.resize(file_size);

			in.read(reinterpret_cast<char *>(file.data.data()), file_size);
		}
	}
}
