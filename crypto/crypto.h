#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Key sizes
#define CRYPTO_KEY_SIZE 32
#define CRYPTO_NONCE_SIZE 12  // ChaCha20-Poly1305 uses 12-byte nonces
#define CRYPTO_MAC_SIZE 16

/**
 * @brief Initialize crypto system
 * @return true if successful
 */
bool crypto_init(void);

/**
 * @brief Deinitialize crypto system
 */
void crypto_deinit(void);

/**
 * @brief Generate a random key
 * @param key Buffer to store generated key
 * @param key_size Size of key to generate
 * @return true if successful
 */
bool crypto_generate_key(uint8_t* key, size_t key_size);

/**
 * @brief Encrypt data using ChaCha20-Poly1305
 * @param plaintext Plaintext data
 * @param plaintext_len Length of plaintext
 * @param key Encryption key
 * @param nonce Nonce/IV
 * @param ciphertext Buffer for encrypted data
 * @param ciphertext_len Size of ciphertext buffer
 * @return true if successful
 */
bool crypto_encrypt(
    const uint8_t* plaintext,
    size_t plaintext_len,
    const uint8_t* key,
    const uint8_t* nonce,
    uint8_t* ciphertext,
    size_t* ciphertext_len);

/**
 * @brief Decrypt data using ChaCha20-Poly1305
 * @param ciphertext Encrypted data
 * @param ciphertext_len Length of ciphertext
 * @param key Decryption key
 * @param nonce Nonce/IV
 * @param plaintext Buffer for decrypted data
 * @param plaintext_len Size of plaintext buffer
 * @return true if successful
 */
bool crypto_decrypt(
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* key,
    const uint8_t* nonce,
    uint8_t* plaintext,
    size_t* plaintext_len);

/**
 * @brief Perform Curve25519 key exchange
 * @param private_key Our private key
 * @param public_key Their public key
 * @param shared_secret Buffer for shared secret
 * @return true if successful
 */
bool crypto_key_exchange(
    const uint8_t* private_key,
    const uint8_t* public_key,
    uint8_t* shared_secret);

/**
 * @brief Generate Curve25519 key pair
 * @param public_key Buffer for public key
 * @param private_key Buffer for private key
 * @return true if successful
 */
bool crypto_generate_keypair(uint8_t* public_key, uint8_t* private_key);

/**
 * @brief Compute SHA-256 hash
 * @param data Data to hash
 * @param data_len Length of data
 * @param hash Buffer for hash output (32 bytes)
 * @return true if successful
 */
bool crypto_hash(const uint8_t* data, size_t data_len, uint8_t* hash);
