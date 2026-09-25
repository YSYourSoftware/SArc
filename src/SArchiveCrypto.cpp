#include "SArc/Helpers.hpp"

#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

using namespace SArc;

static EVP_PKEY *load_der_pubkey(const byte_span_const_t &der) {
	EVP_PKEY *pkey;

	auto der_ptr = reinterpret_cast<const uint8_t *>(der.data());
	SARC_RUNTIME_ASSERT(d2i_PUBKEY(&pkey, &der_ptr, der.size()), helpers::crypto_error,
						"Failed to convert DER to PKEY");

	return pkey;
}

static EVP_PKEY *load_der_private_key(const byte_span_const_t &der) {
	EVP_PKEY *pkey;

	auto der_ptr = reinterpret_cast<const uint8_t *>(der.data());
	SARC_RUNTIME_ASSERT(d2i_PrivateKey(EVP_PKEY_RSA, &pkey, &der_ptr, der.size()), helpers::crypto_error,
						"Failed to convert DER to PKEY");

	return pkey;
}

static sha256_hash_t fingerprint_evp_pkey(const EVP_PKEY *pkey) {
	const int der_len = i2d_PUBKEY(pkey, nullptr);
	SARC_RUNTIME_ASSERT(der_len > 0, helpers::crypto_error, "Failed to convert PKEY to DER");

	auto *der = static_cast<uint8_t *>(OPENSSL_malloc(der_len));
	SARC_RUNTIME_ASSERT(der, memory_error, "Could not allocate DER buffer");

	SARC_RUNTIME_ASSERT(i2d_PUBKEY(pkey, &der) == der_len, helpers::crypto_error, "Failed to convert PKEY to DER");

	bytes_t der_bytes;
	der_bytes.resize(der_len);
	std::memcpy(der_bytes.data(), der, der_len);

	OPENSSL_free(der);

	helpers::BytesIStream bytes_istream{der_bytes};

	return helpers::calculate_sha256(bytes_istream, der_len);
}

sha256_hash_t helpers::fingerprint_key(const byte_span_const_t &public_key) {
	EVP_PKEY *pkey = nullptr;

	try {
		pkey = load_der_pubkey(public_key);

		const sha256_hash_t result = fingerprint_evp_pkey(pkey);
		EVP_PKEY_free(pkey);

		return result;
	} catch (std::exception &) {
		EVP_PKEY_free(pkey);

		throw;
	}
}

bytes_t helpers::sign_data(const byte_span_const_t &data, const byte_span_const_t &private_key) {
	EVP_PKEY *signing_key = nullptr;
	EVP_PKEY_CTX *ctx = nullptr;
	uint8_t *sig = nullptr;

	try {
		signing_key = load_der_private_key(private_key);

		ctx = EVP_PKEY_CTX_new(signing_key, nullptr);
		SARC_RUNTIME_ASSERT(ctx, crypto_error, "Could not init EVP PKEY context");
		SARC_RUNTIME_ASSERT(EVP_PKEY_sign_init(ctx), crypto_error, "Could not init context for signature");
		SARC_RUNTIME_ASSERT(EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) &&
								EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING),
							crypto_error, "Could not set context message digest or padding");

		size_t siglen;
		SARC_RUNTIME_ASSERT(
			EVP_PKEY_sign(ctx, nullptr, &siglen, reinterpret_cast<const uint8_t *>(data.data()), data.size()),
			crypto_error, "Failed to determine signature length");

		sig = static_cast<uint8_t *>(OPENSSL_malloc(siglen));
		SARC_RUNTIME_ASSERT(sig, memory_error, "Could not allocate signature buffer");

		SARC_RUNTIME_ASSERT(
			EVP_PKEY_sign(ctx, sig, &siglen, reinterpret_cast<const uint8_t *>(data.data()), data.size()), crypto_error,
			"RSA signature failed");

		EVP_PKEY_free(signing_key);
		EVP_PKEY_CTX_free(ctx);

		bytes_t result;
		result.resize(siglen);
		std::memcpy(result.data(), sig, siglen);

		OPENSSL_free(sig);

		return result;
	} catch (std::exception &) {
		EVP_PKEY_free(signing_key);
		EVP_PKEY_CTX_free(ctx);

		OPENSSL_free(sig);

		throw;
	}
}

bytes_t helpers::sign_data(std::istream &stream, const size_t length, const byte_span_const_t &private_key) {
	EVP_PKEY *signing_key = nullptr;
	EVP_PKEY_CTX *ctx = nullptr;
	EVP_MD_CTX *md_ctx = nullptr;

	try {
		signing_key = load_der_private_key(private_key);

		md_ctx = EVP_MD_CTX_new();
		SARC_RUNTIME_ASSERT(md_ctx, memory_error, "Could not allocate digest context");

		SARC_RUNTIME_ASSERT(EVP_DigestSignInit(md_ctx, &ctx, EVP_sha256(), nullptr, signing_key), crypto_error,
							"Could not init context for signature");
		SARC_RUNTIME_ASSERT(EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING), crypto_error,
							"Could not set RSA padding");

		std::array<uint8_t, 256> buffer{};

		for (size_t i = 0; i < length / 256; i++) {
			stream.read(reinterpret_cast<char *>(buffer.data()), 256);
			SARC_RUNTIME_ASSERT(EVP_DigestSignUpdate(md_ctx, buffer.data(), 256), crypto_error,
								"Could not update signature digest");
		}

		stream.read(reinterpret_cast<char *>(buffer.data()), length % 256);
		SARC_RUNTIME_ASSERT(EVP_DigestSignUpdate(md_ctx, buffer.data(), length % 256), crypto_error,
							"Could not update signature digest");

		size_t siglen;
		SARC_RUNTIME_ASSERT(EVP_DigestSignFinal(md_ctx, nullptr, &siglen) == 0, crypto_error,
							"Failed to determine signature length");

		bytes_t result;
		result.resize(siglen);

		SARC_RUNTIME_ASSERT(EVP_DigestSignFinal(md_ctx, reinterpret_cast<unsigned char *>(result.data()), &siglen) == 0,
							crypto_error, "Failed to determine signature length");

		EVP_PKEY_free(signing_key);
		EVP_PKEY_CTX_free(ctx);

		return result;
	} catch (std::exception &) {
		EVP_PKEY_free(signing_key);
		EVP_PKEY_CTX_free(ctx);

		throw;
	}
}

bool helpers::validate_signature(const byte_span_const_t &data, const byte_span_const_t &signature,
								 const byte_span_const_t &public_key, const sha256_hash_t *fingerprint) {
	EVP_PKEY *verify_key = nullptr;
	EVP_PKEY_CTX *ctx = nullptr;

	try {
		verify_key = load_der_pubkey(public_key);

		if (fingerprint) {
			if (fingerprint_evp_pkey(verify_key) != *fingerprint) {
				EVP_PKEY_free(verify_key);
				return false;
			}
		}

		ctx = EVP_PKEY_CTX_new(verify_key, nullptr);
		SARC_RUNTIME_ASSERT(ctx, crypto_error, "Could not init EVP PKEY context");
		SARC_RUNTIME_ASSERT(EVP_PKEY_sign_init(ctx) == 0, crypto_error, "Could not init context for signature");
		SARC_RUNTIME_ASSERT(EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) == 0 &&
								EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) == 0,
							crypto_error, "Could not set context message digest or padding");

		const int ret = EVP_PKEY_verify(ctx, reinterpret_cast<const uint8_t *>(signature.data()), signature.size(),
										reinterpret_cast<const uint8_t *>(data.data()), data.size());

		SARC_RUNTIME_ASSERT(ret >= 0, crypto_error, "Could not verify signature");

		EVP_PKEY_free(verify_key);
		EVP_PKEY_CTX_free(ctx);

		return ret == 0;
	} catch (std::exception &) {
		EVP_PKEY_free(verify_key);
		EVP_PKEY_CTX_free(ctx);

		throw;
	}
}
