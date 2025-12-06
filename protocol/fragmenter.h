#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "packet.h"

#define FRAGMENT_MAX_SIZE 256
#define FRAGMENT_TIMEOUT_MS 30000
#define MAX_FRAGMENTS_PER_MESSAGE 16

typedef struct {
    uint8_t message_id[8];
    uint8_t* fragments[MAX_FRAGMENTS_PER_MESSAGE];
    size_t fragment_sizes[MAX_FRAGMENTS_PER_MESSAGE];
    bool fragment_received[MAX_FRAGMENTS_PER_MESSAGE];
    size_t total_fragments;
    size_t received_count;
    uint32_t first_fragment_time;
} FragmentAssembly;

typedef struct MessageFragmenter MessageFragmenter;

/**
 * @brief Allocate message fragmenter
 * @return Fragmenter instance
 */
MessageFragmenter* message_fragmenter_alloc(void);

/**
 * @brief Free message fragmenter
 * @param fragmenter Fragmenter instance
 */
void message_fragmenter_free(MessageFragmenter* fragmenter);

/**
 * @brief Fragment a large message into packets
 * @param fragmenter Fragmenter instance
 * @param data Data to fragment
 * @param data_len Length of data
 * @param sender_id Sender ID
 * @param recipient_id Recipient ID (NULL for broadcast)
 * @param packets Array to store created packets
 * @param max_packets Maximum number of packets
 * @return Number of fragment packets created
 */
size_t message_fragmenter_create_fragments(
    MessageFragmenter* fragmenter,
    const uint8_t* data,
    size_t data_len,
    const uint8_t* sender_id,
    const uint8_t* recipient_id,
    BitchatPacket** packets,
    size_t max_packets);

/**
 * @brief Process a fragment packet
 * @param fragmenter Fragmenter instance
 * @param packet Fragment packet
 * @param assembled_data Buffer for assembled data (if complete)
 * @param assembled_len Length of assembled data buffer
 * @param written Number of bytes written (if complete)
 * @return true if message is complete and assembled
 */
bool message_fragmenter_process_fragment(
    MessageFragmenter* fragmenter,
    const BitchatPacket* packet,
    uint8_t* assembled_data,
    size_t assembled_len,
    size_t* written);

/**
 * @brief Clean up timed-out fragment assemblies
 * @param fragmenter Fragmenter instance
 */
void message_fragmenter_cleanup(MessageFragmenter* fragmenter);
