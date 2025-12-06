#include "packet.h"
#include "../flip_bitchat.h"
#include <furi.h>
#include <string.h>
#include <stdlib.h>

#define TAG "Protocol"

BitchatPacket* packet_create(
    PacketType type,
    uint8_t ttl,
    const uint8_t* sender_id,
    const uint8_t* recipient_id,
    const uint8_t* payload,
    size_t payload_len) {
    
    BitchatPacket* packet = malloc(sizeof(BitchatPacket));
    if(!packet) return NULL;
    
    // Set header
    packet->header.version = BITCHAT_PROTOCOL_VERSION;
    packet->header.type = type;
    packet->header.ttl = ttl;
    packet->header.timestamp = furi_get_tick();
    packet->header.payload_len = payload_len;
    packet->header.flags = 0;
    
    // Set sender ID
    memcpy(packet->sender_id, sender_id, 8);
    
    // Set recipient ID if provided
    if(recipient_id) {
        memcpy(packet->recipient_id, recipient_id, 8);
        packet->header.flags |= PACKET_FLAG_HAS_RECIPIENT;
    } else {
        // Broadcast ID
        memset(packet->recipient_id, 0xFF, 8);
    }
    
    // Copy payload
    if(payload_len > 0) {
        packet->payload = malloc(payload_len);
        if(!packet->payload) {
            // Allocation failed - clean up and return NULL
            free(packet);
            return NULL;
        }
        memcpy(packet->payload, payload, payload_len);
    } else {
        packet->payload = NULL;
    }
    
    return packet;
}

void packet_free(BitchatPacket* packet) {
    if(packet) {
        if(packet->payload) {
            free(packet->payload);
        }
        free(packet);
    }
}

size_t packet_serialize(const BitchatPacket* packet, uint8_t* buffer, size_t buffer_size) {
    if(!packet || !buffer) return 0;
    
    // Calculate required size
    size_t required_size = sizeof(BitchatPacketHeader) + 8; // Header + sender ID
    
    if(packet->header.flags & PACKET_FLAG_HAS_RECIPIENT) {
        required_size += 8; // Recipient ID
    }
    
    required_size += packet->header.payload_len;
    
    if(packet->header.flags & PACKET_FLAG_HAS_SIGNATURE) {
        required_size += 64; // Signature
    }
    
    if(buffer_size < required_size) {
        FURI_LOG_E(TAG, "Buffer too small: need %zu, have %zu", required_size, buffer_size);
        return 0;
    }
    
    size_t offset = 0;
    
    // Write header
    memcpy(buffer + offset, &packet->header, sizeof(BitchatPacketHeader));
    offset += sizeof(BitchatPacketHeader);
    
    // Write sender ID
    memcpy(buffer + offset, packet->sender_id, 8);
    offset += 8;
    
    // Write recipient ID if present
    if(packet->header.flags & PACKET_FLAG_HAS_RECIPIENT) {
        memcpy(buffer + offset, packet->recipient_id, 8);
        offset += 8;
    }
    
    // Write payload
    if(packet->header.payload_len > 0 && packet->payload) {
        memcpy(buffer + offset, packet->payload, packet->header.payload_len);
        offset += packet->header.payload_len;
    }
    
    // Write signature if present
    if(packet->header.flags & PACKET_FLAG_HAS_SIGNATURE) {
        memcpy(buffer + offset, packet->signature, 64);
        offset += 64;
    }
    
    return offset;
}

BitchatPacket* packet_deserialize(const uint8_t* buffer, size_t buffer_size) {
    if(!buffer || buffer_size < sizeof(BitchatPacketHeader) + 8) {
        FURI_LOG_E(TAG, "Buffer too small for packet");
        return NULL;
    }
    
    size_t offset = 0;
    
    // Read header
    BitchatPacketHeader header;
    memcpy(&header, buffer + offset, sizeof(BitchatPacketHeader));
    offset += sizeof(BitchatPacketHeader);
    
    // Read sender ID
    uint8_t sender_id[8];
    memcpy(sender_id, buffer + offset, 8);
    offset += 8;
    
    // Read recipient ID if present
    uint8_t* recipient_id = NULL;
    uint8_t recipient_id_buf[8];
    if(header.flags & PACKET_FLAG_HAS_RECIPIENT) {
        memcpy(recipient_id_buf, buffer + offset, 8);
        recipient_id = recipient_id_buf;
        offset += 8;
    }
    
    // Read payload
    const uint8_t* payload = NULL;
    if(header.payload_len > 0) {
        payload = buffer + offset;
        offset += header.payload_len;
    }
    
    // Create packet
    BitchatPacket* packet = packet_create(
        header.type,
        header.ttl,
        sender_id,
        recipient_id,
        payload,
        header.payload_len);
    
    if(packet) {
        packet->header.timestamp = header.timestamp;
        packet->header.flags = header.flags;
    }
    
    return packet;
}

BitchatPacket* packet_create_message(
    const char* sender,
    const char* content,
    const uint8_t* sender_id,
    const uint8_t* recipient_id) {
    
    // Create message structure
    BitchatMessage msg = {0};
    msg.timestamp = furi_get_tick();
    snprintf(msg.id, sizeof(msg.id), "%lu", msg.timestamp); // Simple ID
    snprintf(msg.sender, sizeof(msg.sender), "%s", sender);
    snprintf(msg.content, sizeof(msg.content), "%s", content);
    msg.is_relay = false;
    msg.is_private = (recipient_id != NULL);
    
    // Serialize message to payload
    // For simplicity, just pack the essential fields
    uint8_t payload[512];
    size_t payload_len = 0;
    
    int written = snprintf(
        (char*)(payload + payload_len),
        sizeof(payload) - payload_len,
        "%s|%s",
        msg.sender,
        msg.content);
    
    // Check for truncation
    if(written > 0 && (size_t)written < sizeof(payload) - payload_len) {
        payload_len += written;
    } else {
        // Message too large or error
        FURI_LOG_E(TAG, "Message serialization failed or truncated");
        return NULL;
    }
    
    // Create packet
    return packet_create(
        PacketTypeMessage,
        BLE_MESH_TTL,
        sender_id,
        recipient_id,
        payload,
        payload_len);
}

bool packet_extract_message(const BitchatPacket* packet, BitchatMessage* message) {
    if(!packet || !message || packet->header.type != PacketTypeMessage) {
        return false;
    }
    
    if(!packet->payload || packet->header.payload_len == 0) {
        return false;
    }
    
    // Create a null-terminated copy of the payload for safe string operations
    char* payload_str = malloc(packet->header.payload_len + 1);
    if(!payload_str) {
        return false;
    }
    memcpy(payload_str, packet->payload, packet->header.payload_len);
    payload_str[packet->header.payload_len] = '\0';
    
    // Parse payload (simple format: "sender|content")
    char* separator = strchr(payload_str, '|');
    
    if(!separator) {
        free(payload_str);
        return false;
    }
    
    size_t sender_len = separator - payload_str;
    if(sender_len >= sizeof(message->sender)) {
        sender_len = sizeof(message->sender) - 1;
    }
    
    memcpy(message->sender, payload_str, sender_len);
    message->sender[sender_len] = '\0';
    
    strncpy(message->content, separator + 1, sizeof(message->content) - 1);
    message->content[sizeof(message->content) - 1] = '\0';
    
    message->timestamp = packet->header.timestamp;
    message->is_private = (packet->header.flags & PACKET_FLAG_HAS_RECIPIENT) != 0;
    
    free(payload_str);
    return true;
}

BitchatPacket* packet_create_delivery_ack(
    const uint8_t* ack_for_id,
    const uint8_t* sender_id,
    const uint8_t* recipient_id) {
    
    furi_assert(ack_for_id);
    furi_assert(sender_id);
    
    // Payload is just the message ID being acknowledged
    return packet_create(
        PacketTypeDeliveryAck,
        BLE_MESH_TTL,
        sender_id,
        recipient_id,
        ack_for_id,
        8);
}

BitchatPacket* packet_create_read_receipt(
    const uint8_t* read_id,
    const uint8_t* sender_id,
    const uint8_t* recipient_id) {
    
    furi_assert(read_id);
    furi_assert(sender_id);
    
    // Payload is just the message ID being marked as read
    return packet_create(
        PacketTypeReadReceipt,
        BLE_MESH_TTL,
        sender_id,
        recipient_id,
        read_id,
        8);
}

bool packet_extract_ack_id(const BitchatPacket* packet, uint8_t* ack_id) {
    if(!packet || !ack_id) {
        return false;
    }
    
    if(packet->header.type != PacketTypeDeliveryAck &&
       packet->header.type != PacketTypeReadReceipt) {
        return false;
    }
    
    if(!packet->payload || packet->header.payload_len < 8) {
        return false;
    }
    
    memcpy(ack_id, packet->payload, 8);
    return true;
}
