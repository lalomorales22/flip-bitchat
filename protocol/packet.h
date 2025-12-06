#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Protocol version
#define BITCHAT_PROTOCOL_VERSION 1

// Packet types (from BitChat whitepaper)
typedef enum {
    PacketTypeMessage = 0x01,
    PacketTypeDeliveryAck = 0x02,
    PacketTypeReadReceipt = 0x03,
    PacketTypeNoiseHandshakeInit = 0x04,
    PacketTypeNoiseHandshakeResp = 0x05,
    PacketTypeNoiseHandshakeFinal = 0x06,
    PacketTypePeerAnnounce = 0x07,
    PacketTypeFragmentStart = 0x08,
    PacketTypeFragmentContinue = 0x09,
    PacketTypeFragmentEnd = 0x0A,
} PacketType;

// Packet flags
#define PACKET_FLAG_HAS_RECIPIENT 0x01
#define PACKET_FLAG_HAS_SIGNATURE 0x02
#define PACKET_FLAG_IS_COMPRESSED 0x04

// BitChat packet header (13 bytes fixed)
typedef struct __attribute__((packed)) {
    uint8_t version;        // Protocol version
    uint8_t type;          // Message type
    uint8_t ttl;           // Time-to-live for mesh routing
    uint64_t timestamp;    // Millisecond timestamp
    uint8_t flags;         // Bitmask for optional fields
    uint16_t payload_len;  // Length of payload
} BitchatPacketHeader;

// Full BitChat packet structure
typedef struct {
    BitchatPacketHeader header;
    uint8_t sender_id[8];        // 8-byte truncated peer ID
    uint8_t recipient_id[8];     // Optional, present if HAS_RECIPIENT flag
    uint8_t* payload;            // Variable length payload
    uint8_t signature[64];       // Optional Ed25519 signature
} BitchatPacket;

// BitChat message (payload for PacketTypeMessage)
typedef struct {
    uint64_t timestamp;
    char id[37];              // UUID string
    char sender[32];          // Sender nickname
    char content[256];        // Message content
    char original_sender[32]; // Optional, for relayed messages
    char recipient[32];       // Optional, for private messages
    bool is_relay;
    bool is_private;
} BitchatMessage;

/**
 * @brief Create a new BitChat packet
 * @param type Packet type
 * @param ttl Time-to-live
 * @param sender_id Sender peer ID
 * @param recipient_id Recipient peer ID (NULL for broadcast)
 * @param payload Payload data
 * @param payload_len Payload length
 * @return Allocated packet or NULL on error
 */
BitchatPacket* packet_create(
    PacketType type,
    uint8_t ttl,
    const uint8_t* sender_id,
    const uint8_t* recipient_id,
    const uint8_t* payload,
    size_t payload_len);

/**
 * @brief Free a BitChat packet
 * @param packet Packet to free
 */
void packet_free(BitchatPacket* packet);

/**
 * @brief Serialize a packet to bytes
 * @param packet Packet to serialize
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or 0 on error
 */
size_t packet_serialize(const BitchatPacket* packet, uint8_t* buffer, size_t buffer_size);

/**
 * @brief Deserialize bytes to a packet
 * @param buffer Input buffer
 * @param buffer_size Size of input buffer
 * @return Allocated packet or NULL on error
 */
BitchatPacket* packet_deserialize(const uint8_t* buffer, size_t buffer_size);

/**
 * @brief Create a message packet
 * @param sender Sender nickname
 * @param content Message content
 * @param sender_id Sender peer ID
 * @param recipient_id Optional recipient ID (NULL for broadcast)
 * @return Allocated packet or NULL on error
 */
BitchatPacket* packet_create_message(
    const char* sender,
    const char* content,
    const uint8_t* sender_id,
    const uint8_t* recipient_id);

/**
 * @brief Create a delivery acknowledgment packet
 * @param ack_for_id Message ID being acknowledged
 * @param sender_id Our peer ID
 * @param recipient_id Recipient ID
 * @return Allocated packet or NULL on error
 */
BitchatPacket* packet_create_delivery_ack(
    const uint8_t* ack_for_id,
    const uint8_t* sender_id,
    const uint8_t* recipient_id);

/**
 * @brief Create a read receipt packet
 * @param read_id Message ID being marked as read
 * @param sender_id Our peer ID
 * @param recipient_id Recipient ID
 * @return Allocated packet or NULL on error
 */
BitchatPacket* packet_create_read_receipt(
    const uint8_t* read_id,
    const uint8_t* sender_id,
    const uint8_t* recipient_id);

/**
 * @brief Extract acknowledgment ID from packet
 * @param packet Packet to extract from
 * @param ack_id Buffer for acknowledgment ID (8 bytes)
 * @return true if successful
 */
bool packet_extract_ack_id(const BitchatPacket* packet, uint8_t* ack_id);
