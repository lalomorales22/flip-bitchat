#include "noise.h"
#include "crypto.h"
#include <furi.h>
#include <string.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>

#define TAG "Noise"

// Protocol name for Noise_XX_25519_ChaChaPoly_SHA256
static const char* PROTOCOL_NAME = "Noise_XX_25519_ChaChaPoly_SHA256";

// HKDF helper function using mbedtls
static bool hkdf(
    const uint8_t* chaining_key,
    const uint8_t* input_key_material,
    size_t ikm_len,
    uint8_t* output1,
    uint8_t* output2) {
    
    // Use mbedtls HKDF with SHA-256
    const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    // First output key
    int ret = mbedtls_hkdf(
        md,
        chaining_key,
        32,
        input_key_material ? input_key_material : (const uint8_t*)"",
        input_key_material ? ikm_len : 0,
        NULL,
        0,
        output1,
        32);
    
    if(ret != 0) {
        FURI_LOG_E(TAG, "HKDF failed for output1: -0x%04x", -ret);
        return false;
    }
    
    // Second output key if requested
    if(output2) {
        ret = mbedtls_hkdf(
            md,
            output1,
            32,
            input_key_material ? input_key_material : (const uint8_t*)"",
            input_key_material ? ikm_len : 0,
            NULL,
            0,
            output2,
            32);
        
        if(ret != 0) {
            FURI_LOG_E(TAG, "HKDF failed for output2: -0x%04x", -ret);
            return false;
        }
    }
    
    return true;
}

bool noise_init(
    NoiseHandshakeContext* ctx,
    bool is_initiator,
    const uint8_t* static_private,
    const uint8_t* static_public) {
    
    furi_assert(ctx);
    furi_assert(static_private);
    furi_assert(static_public);
    
    memset(ctx, 0, sizeof(NoiseHandshakeContext));
    
    ctx->state = NoiseStateInit;
    ctx->is_initiator = is_initiator;
    
    // Store static keys
    memcpy(ctx->local_static_private, static_private, 32);
    memcpy(ctx->local_static_public, static_public, 32);
    
    // Initialize chaining key and hash with protocol name
    crypto_hash((const uint8_t*)PROTOCOL_NAME, strlen(PROTOCOL_NAME), ctx->chaining_key);
    memcpy(ctx->hash, ctx->chaining_key, 32);
    
    ctx->tx_nonce = 0;
    ctx->rx_nonce = 0;
    
    FURI_LOG_I(TAG, "Noise handshake initialized (initiator=%d)", is_initiator);
    
    return true;
}

bool noise_handshake_msg1(
    NoiseHandshakeContext* ctx,
    uint8_t* output,
    size_t output_len,
    size_t* written) {
    
    furi_assert(ctx);
    furi_assert(output);
    furi_assert(written);
    
    if(ctx->state != NoiseStateInit || !ctx->is_initiator) {
        FURI_LOG_E(TAG, "Invalid state for msg1");
        return false;
    }
    
    if(output_len < 32) {
        FURI_LOG_E(TAG, "Output buffer too small");
        return false;
    }
    
    // Generate ephemeral keypair
    if(!crypto_generate_keypair(ctx->local_ephemeral_public, ctx->local_ephemeral_private)) {
        FURI_LOG_E(TAG, "Failed to generate ephemeral keypair");
        return false;
    }
    
    // Message: e (ephemeral public key)
    memcpy(output, ctx->local_ephemeral_public, 32);
    *written = 32;
    
    // Update hash: h = HASH(h || e)
    uint8_t temp[64];
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->local_ephemeral_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    ctx->state = NoiseStateHandshakeMsg1;
    
    FURI_LOG_D(TAG, "Created handshake msg1 (%zu bytes)", *written);
    
    return true;
}

bool noise_handshake_msg2(
    NoiseHandshakeContext* ctx,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t output_len,
    size_t* written) {
    
    furi_assert(ctx);
    furi_assert(input);
    furi_assert(output);
    furi_assert(written);
    
    if(ctx->state != NoiseStateInit || ctx->is_initiator) {
        FURI_LOG_E(TAG, "Invalid state for msg2");
        return false;
    }
    
    if(input_len < 32 || output_len < 96) {
        FURI_LOG_E(TAG, "Buffer size error");
        return false;
    }
    
    // Parse msg1: remote ephemeral public key
    memcpy(ctx->remote_ephemeral_public, input, 32);
    
    // Update hash
    uint8_t temp[64];
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->remote_ephemeral_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // Generate our ephemeral keypair
    if(!crypto_generate_keypair(ctx->local_ephemeral_public, ctx->local_ephemeral_private)) {
        FURI_LOG_E(TAG, "Failed to generate ephemeral keypair");
        return false;
    }
    
    // Message: e, ee, s, es
    size_t offset = 0;
    
    // e: our ephemeral public key
    memcpy(output + offset, ctx->local_ephemeral_public, 32);
    offset += 32;
    
    // Update hash
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->local_ephemeral_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // ee: ephemeral-ephemeral DH
    uint8_t ee_secret[32];
    crypto_key_exchange(ctx->local_ephemeral_private, ctx->remote_ephemeral_public, ee_secret);
    hkdf(ctx->chaining_key, ee_secret, 32, ctx->chaining_key, NULL);
    
    // s: our static public key (encrypted with current key)
    // Simplified: just append static public key
    memcpy(output + offset, ctx->local_static_public, 32);
    offset += 32;
    
    // Update hash
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->local_static_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // es: ephemeral-static DH
    uint8_t es_secret[32];
    crypto_key_exchange(ctx->local_ephemeral_private, ctx->remote_ephemeral_public, es_secret);
    hkdf(ctx->chaining_key, es_secret, 32, ctx->chaining_key, NULL);
    
    *written = offset;
    ctx->state = NoiseStateHandshakeMsg2;
    
    FURI_LOG_D(TAG, "Created handshake msg2 (%zu bytes)", *written);
    
    return true;
}

bool noise_handshake_msg3(
    NoiseHandshakeContext* ctx,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t output_len,
    size_t* written) {
    
    furi_assert(ctx);
    furi_assert(input);
    furi_assert(output);
    furi_assert(written);
    
    if(ctx->state != NoiseStateHandshakeMsg1 || !ctx->is_initiator) {
        FURI_LOG_E(TAG, "Invalid state for msg3");
        return false;
    }
    
    if(input_len < 64 || output_len < 32) {
        FURI_LOG_E(TAG, "Buffer size error");
        return false;
    }
    
    // Parse msg2
    size_t offset = 0;
    
    // e: remote ephemeral public key
    memcpy(ctx->remote_ephemeral_public, input + offset, 32);
    offset += 32;
    
    // Update hash
    uint8_t temp[64];
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->remote_ephemeral_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // ee: ephemeral-ephemeral DH
    uint8_t ee_secret[32];
    crypto_key_exchange(ctx->local_ephemeral_private, ctx->remote_ephemeral_public, ee_secret);
    hkdf(ctx->chaining_key, ee_secret, 32, ctx->chaining_key, NULL);
    
    // s: remote static public key
    memcpy(ctx->remote_static_public, input + offset, 32);
    offset += 32;
    
    // Update hash
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->remote_static_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // es: ephemeral-static DH
    uint8_t es_secret[32];
    crypto_key_exchange(ctx->local_ephemeral_private, ctx->remote_static_public, es_secret);
    hkdf(ctx->chaining_key, es_secret, 32, ctx->chaining_key, NULL);
    
    // Message: s, se
    offset = 0;
    
    // s: our static public key
    memcpy(output + offset, ctx->local_static_public, 32);
    offset += 32;
    
    // Update hash
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->local_static_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // se: static-ephemeral DH
    uint8_t se_secret[32];
    crypto_key_exchange(ctx->local_static_private, ctx->remote_ephemeral_public, se_secret);
    hkdf(ctx->chaining_key, se_secret, 32, ctx->chaining_key, NULL);
    
    // Derive transport keys
    hkdf(ctx->chaining_key, NULL, 0, ctx->tx_key, ctx->rx_key);
    
    *written = offset;
    ctx->state = NoiseStateTransport;
    
    FURI_LOG_I(TAG, "Handshake complete (initiator)");
    
    return true;
}

bool noise_handshake_complete(
    NoiseHandshakeContext* ctx,
    const uint8_t* input,
    size_t input_len) {
    
    furi_assert(ctx);
    furi_assert(input);
    
    if(ctx->state != NoiseStateHandshakeMsg2 || ctx->is_initiator) {
        FURI_LOG_E(TAG, "Invalid state for handshake complete");
        return false;
    }
    
    if(input_len < 32) {
        FURI_LOG_E(TAG, "Input too small");
        return false;
    }
    
    // Parse msg3
    // s: remote static public key
    memcpy(ctx->remote_static_public, input, 32);
    
    // Update hash
    uint8_t temp[64];
    memcpy(temp, ctx->hash, 32);
    memcpy(temp + 32, ctx->remote_static_public, 32);
    crypto_hash(temp, 64, ctx->hash);
    
    // se: static-ephemeral DH
    uint8_t se_secret[32];
    crypto_key_exchange(ctx->local_ephemeral_private, ctx->remote_static_public, se_secret);
    hkdf(ctx->chaining_key, se_secret, 32, ctx->chaining_key, NULL);
    
    // Derive transport keys (reversed for responder)
    hkdf(ctx->chaining_key, NULL, 0, ctx->rx_key, ctx->tx_key);
    
    ctx->state = NoiseStateTransport;
    
    FURI_LOG_I(TAG, "Handshake complete (responder)");
    
    return true;
}

bool noise_encrypt(
    NoiseHandshakeContext* ctx,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    size_t ciphertext_len,
    size_t* written) {
    
    furi_assert(ctx);
    furi_assert(plaintext);
    furi_assert(ciphertext);
    furi_assert(written);
    
    if(ctx->state != NoiseStateTransport) {
        FURI_LOG_E(TAG, "Not in transport state");
        return false;
    }
    
    // Create nonce from counter
    uint8_t nonce[12];
    memset(nonce, 0, 12);
    memcpy(nonce + 4, &ctx->tx_nonce, 8);
    
    // Encrypt
    if(!crypto_encrypt(plaintext, plaintext_len, ctx->tx_key, nonce, ciphertext, written)) {
        return false;
    }
    
    ctx->tx_nonce++;
    
    return true;
}

bool noise_decrypt(
    NoiseHandshakeContext* ctx,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* plaintext,
    size_t plaintext_len,
    size_t* written) {
    
    furi_assert(ctx);
    furi_assert(ciphertext);
    furi_assert(plaintext);
    furi_assert(written);
    
    if(ctx->state != NoiseStateTransport) {
        FURI_LOG_E(TAG, "Not in transport state");
        return false;
    }
    
    // Create nonce from counter
    uint8_t nonce[12];
    memset(nonce, 0, 12);
    memcpy(nonce + 4, &ctx->rx_nonce, 8);
    
    // Decrypt
    if(!crypto_decrypt(ciphertext, ciphertext_len, ctx->rx_key, nonce, plaintext, written)) {
        return false;
    }
    
    ctx->rx_nonce++;
    
    return true;
}
