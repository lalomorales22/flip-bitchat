#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Noise protocol XX pattern handshake states
typedef enum {
    NoiseStateInit,
    NoiseStateHandshakeMsg1,
    NoiseStateHandshakeMsg2,
    NoiseStateHandshakeMsg3,
    NoiseStateTransport,
} NoiseState;

// Noise handshake context
typedef struct {
    NoiseState state;
    bool is_initiator;
    
    // Keys
    uint8_t local_static_private[32];
    uint8_t local_static_public[32];
    uint8_t local_ephemeral_private[32];
    uint8_t local_ephemeral_public[32];
    
    uint8_t remote_static_public[32];
    uint8_t remote_ephemeral_public[32];
    
    // Session keys (derived after handshake)
    uint8_t tx_key[32];
    uint8_t rx_key[32];
    
    // Chaining key for key derivation
    uint8_t chaining_key[32];
    uint8_t hash[32];
    
    uint64_t tx_nonce;
    uint64_t rx_nonce;
} NoiseHandshakeContext;

/**
 * @brief Initialize a Noise handshake context
 * @param ctx Context to initialize
 * @param is_initiator True if we're initiating the handshake
 * @param static_private Our static private key
 * @param static_public Our static public key
 * @return true if successful
 */
bool noise_init(
    NoiseHandshakeContext* ctx,
    bool is_initiator,
    const uint8_t* static_private,
    const uint8_t* static_public);

/**
 * @brief Create first handshake message (-> e)
 * @param ctx Handshake context
 * @param output Output buffer
 * @param output_len Size of output buffer
 * @param written Number of bytes written
 * @return true if successful
 */
bool noise_handshake_msg1(
    NoiseHandshakeContext* ctx,
    uint8_t* output,
    size_t output_len,
    size_t* written);

/**
 * @brief Process first handshake message and create response (<- e, ee, s, es)
 * @param ctx Handshake context
 * @param input Input message
 * @param input_len Length of input
 * @param output Output buffer
 * @param output_len Size of output buffer
 * @param written Number of bytes written
 * @return true if successful
 */
bool noise_handshake_msg2(
    NoiseHandshakeContext* ctx,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t output_len,
    size_t* written);

/**
 * @brief Process second handshake message and create final message (-> s, se)
 * @param ctx Handshake context
 * @param input Input message
 * @param input_len Length of input
 * @param output Output buffer
 * @param output_len Size of output buffer
 * @param written Number of bytes written
 * @return true if successful
 */
bool noise_handshake_msg3(
    NoiseHandshakeContext* ctx,
    const uint8_t* input,
    size_t input_len,
    uint8_t* output,
    size_t output_len,
    size_t* written);

/**
 * @brief Process final handshake message
 * @param ctx Handshake context
 * @param input Input message
 * @param input_len Length of input
 * @return true if successful
 */
bool noise_handshake_complete(
    NoiseHandshakeContext* ctx,
    const uint8_t* input,
    size_t input_len);

/**
 * @brief Encrypt data using transport keys
 * @param ctx Handshake context (must be in transport state)
 * @param plaintext Plaintext data
 * @param plaintext_len Length of plaintext
 * @param ciphertext Output buffer
 * @param ciphertext_len Size of ciphertext buffer
 * @param written Number of bytes written
 * @return true if successful
 */
bool noise_encrypt(
    NoiseHandshakeContext* ctx,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    size_t ciphertext_len,
    size_t* written);

/**
 * @brief Decrypt data using transport keys
 * @param ctx Handshake context (must be in transport state)
 * @param ciphertext Ciphertext data
 * @param ciphertext_len Length of ciphertext
 * @param plaintext Output buffer
 * @param plaintext_len Size of plaintext buffer
 * @param written Number of bytes written
 * @return true if successful
 */
bool noise_decrypt(
    NoiseHandshakeContext* ctx,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    uint8_t* plaintext,
    size_t plaintext_len,
    size_t* written);
