#include "message_retry.h"
#include "packet.h"
#include <furi.h>
#include <string.h>

#define TAG "MessageRetry"

struct MessageRetryService {
    PendingMessage messages[MAX_PENDING_MESSAGES];
    size_t message_count;
    FuriMutex* mutex;
    
    // Callback for actually sending messages
    bool (*send_callback)(const uint8_t* data, size_t len, void* context);
    void* send_context;
};

MessageRetryService* message_retry_service_alloc(void) {
    MessageRetryService* service = malloc(sizeof(MessageRetryService));
    
    service->message_count = 0;
    service->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    service->send_callback = NULL;
    service->send_context = NULL;
    
    FURI_LOG_I(TAG, "Message retry service allocated");
    
    return service;
}

void message_retry_service_free(MessageRetryService* service) {
    furi_assert(service);
    
    furi_mutex_acquire(service->mutex, FuriWaitForever);
    
    // Free all pending packets
    for(size_t i = 0; i < service->message_count; i++) {
        if(service->messages[i].packet) {
            packet_free(service->messages[i].packet);
        }
    }
    
    furi_mutex_release(service->mutex);
    furi_mutex_free(service->mutex);
    
    free(service);
    
    FURI_LOG_I(TAG, "Message retry service freed");
}

const uint8_t* message_retry_service_queue(
    MessageRetryService* service,
    const BitchatPacket* packet,
    bool needs_ack) {
    
    furi_assert(service);
    furi_assert(packet);
    
    furi_mutex_acquire(service->mutex, FuriWaitForever);
    
    if(service->message_count >= MAX_PENDING_MESSAGES) {
        FURI_LOG_W(TAG, "Message queue full");
        furi_mutex_release(service->mutex);
        return NULL;
    }
    
    // Find free slot
    size_t slot = service->message_count;
    service->message_count++;
    
    // Generate message ID from timestamp and sender ID
    uint64_t timestamp = furi_get_tick();
    memcpy(service->messages[slot].message_id, &timestamp, 8);
    
    // Copy packet
    service->messages[slot].packet = packet_create(
        packet->header.type,
        packet->header.ttl,
        packet->sender_id,
        (packet->header.flags & PACKET_FLAG_HAS_RECIPIENT) ? packet->recipient_id : NULL,
        packet->payload,
        packet->header.payload_len);
    
    service->messages[slot].state = MessageStateQueued;
    service->messages[slot].sent_time = 0;
    service->messages[slot].retry_count = 0;
    service->messages[slot].needs_ack = needs_ack;
    
    FURI_LOG_D(TAG, "Message queued (slot %zu, needs_ack=%d)", slot, needs_ack);
    
    const uint8_t* id = service->messages[slot].message_id;
    
    furi_mutex_release(service->mutex);
    
    return id;
}

void message_retry_service_ack(MessageRetryService* service, const uint8_t* message_id) {
    furi_assert(service);
    furi_assert(message_id);
    
    furi_mutex_acquire(service->mutex, FuriWaitForever);
    
    // Find message by ID
    for(size_t i = 0; i < service->message_count; i++) {
        if(memcmp(service->messages[i].message_id, message_id, 8) == 0) {
            FURI_LOG_D(TAG, "Message acknowledged (slot %zu)", i);
            service->messages[i].state = MessageStateDelivered;
            
            // Free packet
            if(service->messages[i].packet) {
                packet_free(service->messages[i].packet);
                service->messages[i].packet = NULL;
            }
            
            // Remove from queue by shifting remaining messages
            for(size_t j = i; j < service->message_count - 1; j++) {
                service->messages[j] = service->messages[j + 1];
            }
            service->message_count--;
            
            break;
        }
    }
    
    furi_mutex_release(service->mutex);
}

void message_retry_service_set_send_callback(
    MessageRetryService* service,
    bool (*callback)(const uint8_t* data, size_t len, void* context),
    void* context) {
    
    furi_assert(service);
    
    furi_mutex_acquire(service->mutex, FuriWaitForever);
    
    service->send_callback = callback;
    service->send_context = context;
    
    furi_mutex_release(service->mutex);
}

void message_retry_service_process(MessageRetryService* service) {
    furi_assert(service);
    
    if(!service->send_callback) {
        return;
    }
    
    furi_mutex_acquire(service->mutex, FuriWaitForever);
    
    uint32_t now = furi_get_tick();
    
    for(size_t i = 0; i < service->message_count; i++) {
        PendingMessage* msg = &service->messages[i];
        
        if(msg->state == MessageStateFailed) {
            // Remove failed messages
            if(msg->packet) {
                packet_free(msg->packet);
                msg->packet = NULL;
            }
            
            // Shift remaining messages
            for(size_t j = i; j < service->message_count - 1; j++) {
                service->messages[j] = service->messages[j + 1];
            }
            service->message_count--;
            i--;
            continue;
        }
        
        // Check if message needs to be sent/retried
        if(msg->state == MessageStateQueued ||
           (msg->state == MessageStateWaitingAck &&
            (now - msg->sent_time) > MESSAGE_RETRY_TIMEOUT_MS)) {
            
            // Check retry limit
            if(msg->retry_count >= MESSAGE_MAX_RETRIES) {
                FURI_LOG_W(TAG, "Message retry limit reached (slot %zu)", i);
                msg->state = MessageStateFailed;
                continue;
            }
            
            // Serialize and send packet
            uint8_t buffer[512];
            size_t len = packet_serialize(msg->packet, buffer, sizeof(buffer));
            
            if(len > 0) {
                if(service->send_callback(buffer, len, service->send_context)) {
                    msg->state = msg->needs_ack ? MessageStateWaitingAck : MessageStateDelivered;
                    msg->sent_time = now;
                    msg->retry_count++;
                    
                    FURI_LOG_D(TAG, "Message sent (slot %zu, retry %d)", i, msg->retry_count);
                    
                    // If no ack needed, mark as complete
                    if(!msg->needs_ack) {
                        if(msg->packet) {
                            packet_free(msg->packet);
                            msg->packet = NULL;
                        }
                        
                        // Shift remaining messages
                        for(size_t j = i; j < service->message_count - 1; j++) {
                            service->messages[j] = service->messages[j + 1];
                        }
                        service->message_count--;
                        i--;
                    }
                } else {
                    FURI_LOG_E(TAG, "Failed to send message (slot %zu)", i);
                }
            }
        }
    }
    
    furi_mutex_release(service->mutex);
}

size_t message_retry_service_get_pending_count(MessageRetryService* service) {
    furi_assert(service);
    
    furi_mutex_acquire(service->mutex, FuriWaitForever);
    size_t count = service->message_count;
    furi_mutex_release(service->mutex);
    
    return count;
}
