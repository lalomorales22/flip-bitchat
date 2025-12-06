#include "crypto.h"
#include <furi.h>
#include <furi_hal_random.h>
#include <mbedtls/chachapoly.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/sha256.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#define TAG "Crypto"

// Global entropy and DRBG contexts for random number generation
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static bool crypto_initialized = false;

bool crypto_init(void) {
    FURI_LOG_I(TAG, "Initializing crypto system with mbedtls");
    
    if(crypto_initialized) {
        return true;
    }
    
    // Initialize entropy and DRBG for random number generation
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    
    const char* pers = "flipbitchat";
    int ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char*)pers,
        strlen(pers));
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to seed DRBG: -0x%04x", -ret);
        return false;
    }
    
    crypto_initialized = true;
    FURI_LOG_I(TAG, "Crypto system initialized successfully");
    
    return true;
}

void crypto_deinit(void) {
    FURI_LOG_I(TAG, "Deinitializing crypto system");
    
    if(!crypto_initialized) {
        return;
    }
    
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    
    crypto_initialized = false;
}

bool crypto_generate_key(uint8_t* key, size_t key_size) {
    furi_assert(key);
    
    if(!crypto_initialized) {
        FURI_LOG_E(TAG, "Crypto not initialized");
        return false;
    }
    
    // Use mbedtls DRBG for cryptographically secure random numbers
    int ret = mbedtls_ctr_drbg_random(&ctr_drbg, key, key_size);
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to generate random key: -0x%04x", -ret);
        return false;
    }
    
    return true;
}

bool crypto_encrypt(
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* key,
    const uint8_t* nonce,
    uint8_t* ciphertext,
    size_t* ciphertext_len) {
    
    furi_assert(plaintext);
    furi_assert(key);
    furi_assert(nonce);
    furi_assert(ciphertext);
    furi_assert(ciphertext_len);
    
    if(!crypto_initialized) {
        FURI_LOG_E(TAG, "Crypto not initialized");
        return false;
    }
    
    FURI_LOG_D(TAG, "Encrypting %zu bytes with ChaCha20-Poly1305", plaintext_len);
    
    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);
    
    // Set key
    int ret = mbedtls_chachapoly_setkey(&ctx, key);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to set encryption key: -0x%04x", -ret);
        mbedtls_chachapoly_free(&ctx);
        return false;
    }
    
    // Encrypt and authenticate
    // ChaCha20-Poly1305 uses 12-byte nonces, but we allocated 24 bytes
    // Use first 12 bytes of the nonce
    uint8_t tag[CRYPTO_MAC_SIZE];
    ret = mbedtls_chachapoly_encrypt_and_tag(
        &ctx,
        plaintext_len,
        nonce,              // 12-byte nonce (we'll use first 12 bytes)
        NULL,               // No additional authenticated data
        0,
        plaintext,
        ciphertext,
        tag);
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "Encryption failed: -0x%04x", -ret);
        mbedtls_chachapoly_free(&ctx);
        return false;
    }
    
    // Append MAC tag to ciphertext
    memcpy(ciphertext + plaintext_len, tag, CRYPTO_MAC_SIZE);
    *ciphertext_len = plaintext_len + CRYPTO_MAC_SIZE;
    
    mbedtls_chachapoly_free(&ctx);
    
    return true;
}

bool crypto_decrypt(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* key,
    const uint8_t* nonce,
    uint8_t* plaintext,
    size_t* plaintext_len) {
    
    furi_assert(ciphertext);
    furi_assert(key);
    furi_assert(nonce);
    furi_assert(plaintext);
    furi_assert(plaintext_len);
    
    if(!crypto_initialized) {
        FURI_LOG_E(TAG, "Crypto not initialized");
        return false;
    }
    
    if(ciphertext_len < CRYPTO_MAC_SIZE) {
        FURI_LOG_E(TAG, "Ciphertext too short: %zu bytes", ciphertext_len);
        return false;
    }
    
    FURI_LOG_D(TAG, "Decrypting %zu bytes with ChaCha20-Poly1305", ciphertext_len);
    
    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);
    
    // Set key
    int ret = mbedtls_chachapoly_setkey(&ctx, key);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to set decryption key: -0x%04x", -ret);
        mbedtls_chachapoly_free(&ctx);
        return false;
    }
    
    // Extract tag from end of ciphertext
    size_t actual_ciphertext_len = ciphertext_len - CRYPTO_MAC_SIZE;
    const uint8_t* tag = ciphertext + actual_ciphertext_len;
    
    // Decrypt and verify
    ret = mbedtls_chachapoly_auth_decrypt(
        &ctx,
        actual_ciphertext_len,
        nonce,              // 12-byte nonce (use first 12 bytes)
        NULL,               // No additional authenticated data
        0,
        tag,
        ciphertext,
        plaintext);
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "Decryption or authentication failed: -0x%04x", -ret);
        mbedtls_chachapoly_free(&ctx);
        return false;
    }
    
    *plaintext_len = actual_ciphertext_len;
    
    mbedtls_chachapoly_free(&ctx);
    
    return true;
}

bool crypto_key_exchange(
    const uint8_t* private_key,
    const uint8_t* public_key,
    uint8_t* shared_secret) {
    
    furi_assert(private_key);
    furi_assert(public_key);
    furi_assert(shared_secret);
    
    if(!crypto_initialized) {
        FURI_LOG_E(TAG, "Crypto not initialized");
        return false;
    }
    
    FURI_LOG_D(TAG, "Performing Curve25519 key exchange");
    
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    
    // Set up for Curve25519 (Montgomery curve)
    int ret = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to load Curve25519: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Load our private key
    ret = mbedtls_mpi_read_binary(&ctx.d, private_key, CRYPTO_KEY_SIZE);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to read private key: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Load their public key (X coordinate for Curve25519)
    ret = mbedtls_mpi_read_binary(&ctx.Qp.X, public_key, CRYPTO_KEY_SIZE);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to read public key: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Set Z coordinate to 1 for Montgomery curve
    ret = mbedtls_mpi_lset(&ctx.Qp.Z, 1);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to set Z coordinate: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Compute shared secret
    size_t olen;
    ret = mbedtls_ecdh_calc_secret(
        &ctx,
        &olen,
        shared_secret,
        CRYPTO_KEY_SIZE,
        mbedtls_ctr_drbg_random,
        &ctr_drbg);
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to compute shared secret: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    mbedtls_ecdh_free(&ctx);
    
    return true;
}

bool crypto_generate_keypair(uint8_t* public_key, uint8_t* private_key) {
    furi_assert(public_key);
    furi_assert(private_key);
    
    if(!crypto_initialized) {
        FURI_LOG_E(TAG, "Crypto not initialized");
        return false;
    }
    
    FURI_LOG_D(TAG, "Generating Curve25519 keypair");
    
    mbedtls_ecdh_context ctx;
    mbedtls_ecdh_init(&ctx);
    
    // Set up for Curve25519
    int ret = mbedtls_ecp_group_load(&ctx.grp, MBEDTLS_ECP_DP_CURVE25519);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to load Curve25519: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Generate keypair
    ret = mbedtls_ecdh_gen_public(
        &ctx.grp,
        &ctx.d,
        &ctx.Q,
        mbedtls_ctr_drbg_random,
        &ctr_drbg);
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to generate keypair: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Export private key
    ret = mbedtls_mpi_write_binary(&ctx.d, private_key, CRYPTO_KEY_SIZE);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to export private key: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    // Export public key (X coordinate for Curve25519)
    ret = mbedtls_mpi_write_binary(&ctx.Q.X, public_key, CRYPTO_KEY_SIZE);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to export public key: -0x%04x", -ret);
        mbedtls_ecdh_free(&ctx);
        return false;
    }
    
    mbedtls_ecdh_free(&ctx);
    
    return true;
}

bool crypto_hash(const uint8_t* data, size_t data_len, uint8_t* hash) {
    furi_assert(data);
    furi_assert(hash);
    
    if(!crypto_initialized) {
        FURI_LOG_E(TAG, "Crypto not initialized");
        return false;
    }
    
    FURI_LOG_D(TAG, "Computing SHA-256 hash of %zu bytes", data_len);
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    
    // Start SHA-256 hashing (0 = SHA-256, 1 = SHA-224)
    int ret = mbedtls_sha256_starts(&ctx, 0);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to start SHA-256: -0x%04x", -ret);
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    // Update hash with data
    ret = mbedtls_sha256_update(&ctx, data, data_len);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to update SHA-256: -0x%04x", -ret);
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    // Finalize hash
    ret = mbedtls_sha256_finish(&ctx, hash);
    if(ret != 0) {
        FURI_LOG_E(TAG, "Failed to finish SHA-256: -0x%04x", -ret);
        mbedtls_sha256_free(&ctx);
        return false;
    }
    
    mbedtls_sha256_free(&ctx);
    
    return true;
}
