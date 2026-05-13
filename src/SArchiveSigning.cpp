#include "SArc.hpp"

#include <rnp/rnp.h>
#include <rnp/rnp_err.h>

using namespace SArc;

SArchiveMemory::~SArchiveMemory() {
	if (m_ffi) rnp_ffi_destroy(m_ffi);
	if (m_signing_key) rnp_key_handle_destroy(m_signing_key);
}

void SArchiveMemory::sign(const byte_span_const_t &key, const std::string &key_passphrase,
						  const pgp_fingerprint_t &key_fingerprint) {
	rnp_input_t gpg_key;

	if (!m_ffi)
		SARC_RUNTIME_ASSERT(rnp_ffi_create(&m_ffi, RNP_KEYSTORE_GPG, RNP_KEYSTORE_GPG) == RNP_SUCCESS, ffi_error,
							"Failed to create FFI object");

	SARC_RUNTIME_ASSERT(
		rnp_input_from_memory(&gpg_key, reinterpret_cast<const uint8_t *>(key.data()), key.size(), true) == RNP_SUCCESS,
		invalid_gpg_data, "Failed to load secret key data");

	SARC_RUNTIME_ASSERT(rnp_load_keys(m_ffi, RNP_KEYSTORE_GPG, gpg_key, RNP_LOAD_SAVE_SECRET_KEYS) == RNP_SUCCESS,
						invalid_gpg_data, "Failed to load secret keys from data");

	rnp_input_destroy(gpg_key);
	gpg_key = nullptr;

	m_signing_key_passphrase = key_passphrase;

	std::ostringstream fp_stream;

	for (const uint8_t b : key_fingerprint)
		fp_stream << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);

	const std::string fp_hex = fp_stream.str();

	SARC_RUNTIME_ASSERT(rnp_locate_key(m_ffi, "fingerprint", fp_hex.c_str(), &m_signing_key) == RNP_SUCCESS,
						invalid_gpg_data, "Failed to locate signing key by fingerprint");
}

bytes_t SArchiveMemory::sign_data(const byte_span_const_t &data) const {
	rnp_input_t input = nullptr;
	rnp_output_t output = nullptr;
	rnp_op_sign_t sign = nullptr;

	uint8_t *output_buf = nullptr;
	size_t output_len = 0;

	bytes_t result;

	bool has_secret = false;
	bool locked = false;

	rnp_key_have_secret(m_signing_key, &has_secret);
	rnp_key_is_locked(m_signing_key, &locked);

	if (locked) {
		SARC_RUNTIME_ASSERT(rnp_key_unlock(m_signing_key, m_signing_key_passphrase.c_str()) == RNP_SUCCESS, sign_error,
							"Passphrase is incorrect");
		rnp_key_is_locked(m_signing_key, &locked);
	}

	SARC_RUNTIME_ASSERT(has_secret, sign_error, "Signing key has no secret material");
	SARC_RUNTIME_ASSERT(!locked, sign_error, "Signing key is locked");

	SARC_RUNTIME_ASSERT(rnp_input_from_memory(&input, reinterpret_cast<const uint8_t *>(data.data()), data.size(),
											  false) == RNP_SUCCESS,
						sign_error, "Failed to create input");
	SARC_RUNTIME_ASSERT(rnp_output_to_memory(&output, 0) == RNP_SUCCESS, sign_error, "Failed to create output");
	SARC_RUNTIME_ASSERT(rnp_op_sign_detached_create(&sign, m_ffi, input, output) == RNP_SUCCESS, sign_error,
						"Failed to create sign op");

	rnp_op_sign_set_armor(sign, false);
	rnp_op_sign_set_hash(sign, RNP_ALGNAME_SHA256);

	SARC_RUNTIME_ASSERT(rnp_op_sign_add_signature(sign, m_signing_key, nullptr) == RNP_SUCCESS, sign_error,
						"Failed to add signature");
	SARC_RUNTIME_ASSERT(rnp_op_sign_execute(sign) == RNP_SUCCESS, sign_error,
						"Failed to sign");
	SARC_RUNTIME_ASSERT(rnp_output_memory_get_buf(output, &output_buf, &output_len, false) == RNP_SUCCESS, sign_error,
						"Failed to get signed buffer");

	result.resize(output_len);

	std::memcpy(result.data(), output_buf, output_len);

	rnp_op_sign_destroy(sign);
	rnp_input_destroy(input);
	rnp_output_destroy(output);

	return result;
}

