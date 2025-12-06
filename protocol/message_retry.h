#pragma once

#include <furi.h>
#include "protocol/packet.h"

#define MAX_PENDING_MESSAGES 16
#define MESSAGE_RETRY_TIMEOUT_MS 5000
#define MESSAGE_MAX_RETRIES 3

typedef enum {
    MessageStateQueued,
    MessageStateSending,
    MessageStateWaitingAck,
    MessageStateDelivered,
    MessageStateFailed,
} MessageState;

typedef struct {
    uint8_t message_id[8];
    BitchatPacket* packet;
    MessageState state;
    uint32_t sent_time;
    uint8_t retry_count;
    bool needs_ack;
} PendingMessage;

typedef struct MessageRetryService MessageRetryService;

/**
 * @brief Allocate message retry service
 * @return Service instance
 */
MessageRetryService* message_retry_service_alloc(void);

/**
 * @brief Free message retry service
 * @param service Service instance
 */
void message_retry_service_free(MessageRetryService* service);

/**
 * @brief Queue a message for sending with retry
 * @param service Service instance
 * @param packet Packet to send (will be copied)
 * @param needs_ack Whether message needs acknowledgment
 * @return Message ID, or NULL if queue is full
 */
const uint8_t* message_retry_service_queue(
    MessageRetryService* service,
    const BitchatPacket* packet,
    bool needs_ack);

/**
 * @brief Mark a message as acknowledged
 * @param service Service instance
 * @param message_id Message ID
 */
void message_retry_service_ack(MessageRetryService* service, const uint8_t* message_id);

/**
 * @brief Process pending messages (call periodically)
 * @param service Service instance
 */
void message_retry_service_process(MessageRetryService* service);

/**
 * @brief Set callback for sending messages
 * @param service Service instance
 * @param callback Send callback function
 * @param context Context to pass to callback
 */
void message_retry_service_set_send_callback(
    MessageRetryService* service,
    bool (*callback)(const uint8_t* data, size_t len, void* context),
    void* context);

/**
 * @brief Get number of pending messages
 * @param service Service instance
 * @return Number of pending messages
 */
size_t message_retry_service_get_pending_count(MessageRetryService* service);
