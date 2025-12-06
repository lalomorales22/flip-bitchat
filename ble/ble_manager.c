#include "ble_manager.h"
#include <furi.h>
#include <furi_hal_bt.h>

#define TAG "BLEManager"

// BLE event types
typedef enum {
    BLEEventConnected,
    BLEEventDisconnected,
    BLEEventDataReceived,
} BLEEventType;

typedef struct {
    BLEEventType type;
    uint8_t peer_id[8];
    uint8_t* data;
    size_t data_len;
} BLEEvent;

struct BLEManager {
    BLEState state;
    BLEMessageCallback message_callback;
    void* callback_context;
    
    // Peer tracking
    BLEPeer peers[16];
    size_t peer_count;
    
    // BLE state
    bool advertising_active;
    bool scanning_active;
    
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    FuriThread* worker_thread;
    volatile bool worker_running;
};

// Worker thread for handling BLE events
static int32_t ble_manager_worker(void* context) {
    BLEManager* manager = context;
    
    FURI_LOG_I(TAG, "BLE worker thread started");
    
    while(manager->worker_running) {
        BLEEvent event;
        
        // Wait for events with timeout
        if(furi_message_queue_get(manager->event_queue, &event, 100) == FuriStatusOk) {
            // Process event
            switch(event.type) {
                case BLEEventDataReceived:
                    if(manager->message_callback) {
                        manager->message_callback(
                            manager->callback_context,
                            event.data,
                            event.data_len,
                            event.peer_id);
                    }
                    if(event.data) {
                        free(event.data);
                    }
                    break;
                    
                case BLEEventConnected:
                    FURI_LOG_I(TAG, "Peer connected");
                    // Add peer to list if not already present
                    furi_mutex_acquire(manager->mutex, FuriWaitForever);
                    bool found = false;
                    for(size_t i = 0; i < manager->peer_count; i++) {
                        if(memcmp(manager->peers[i].peer_id, event.peer_id, 8) == 0) {
                            found = true;
                            manager->peers[i].last_seen = furi_get_tick();
                            break;
                        }
                    }
                    if(!found && manager->peer_count < 16) {
                        memcpy(manager->peers[manager->peer_count].peer_id, event.peer_id, 8);
                        manager->peers[manager->peer_count].last_seen = furi_get_tick();
                        manager->peers[manager->peer_count].rssi = 0;
                        snprintf(
                            manager->peers[manager->peer_count].name,
                            sizeof(manager->peers[manager->peer_count].name),
                            "Peer_%02X%02X",
                            event.peer_id[0],
                            event.peer_id[1]);
                        manager->peer_count++;
                    }
                    furi_mutex_release(manager->mutex);
                    break;
                    
                case BLEEventDisconnected:
                    FURI_LOG_I(TAG, "Peer disconnected");
                    break;
            }
        }
    }
    
    FURI_LOG_I(TAG, "BLE worker thread stopped");
    return 0;
}

BLEManager* ble_manager_alloc(void) {
    BLEManager* manager = malloc(sizeof(BLEManager));
    
    manager->state = BLEStateIdle;
    manager->message_callback = NULL;
    manager->callback_context = NULL;
    manager->peer_count = 0;
    manager->advertising_active = false;
    manager->scanning_active = false;
    
    manager->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    manager->event_queue = furi_message_queue_alloc(8, sizeof(BLEEvent));
    
    // Start worker thread
    manager->worker_running = true;
    manager->worker_thread = furi_thread_alloc();
    furi_thread_set_name(manager->worker_thread, "BLEWorker");
    furi_thread_set_stack_size(manager->worker_thread, 2048);
    furi_thread_set_context(manager->worker_thread, manager);
    furi_thread_set_callback(manager->worker_thread, ble_manager_worker);
    furi_thread_start(manager->worker_thread);
    
    FURI_LOG_I(TAG, "BLE Manager allocated");
    
    return manager;
}

void ble_manager_free(BLEManager* manager) {
    furi_assert(manager);
    
    // Stop worker thread
    manager->worker_running = false;
    furi_thread_join(manager->worker_thread);
    furi_thread_free(manager->worker_thread);
    
    // Stop any active operations
    if(manager->advertising_active) {
        ble_manager_stop_advertising(manager);
    }
    if(manager->scanning_active) {
        ble_manager_stop_scan(manager);
    }
    
    furi_message_queue_free(manager->event_queue);
    furi_mutex_free(manager->mutex);
    free(manager);
    
    FURI_LOG_I(TAG, "BLE Manager freed");
}

bool ble_manager_start_advertising(BLEManager* manager, const char* device_name) {
    furi_assert(manager);
    furi_assert(device_name);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Starting BLE advertising as '%s'", device_name);
    
    // Check if BLE is available
    if(!furi_hal_bt_is_active()) {
        FURI_LOG_W(TAG, "Bluetooth is not active, attempting to start");
        furi_hal_bt_start_advertising();
    }
    
    // Note: Flipper's BLE stack uses a profile system
    // For a custom GATT service, we would need to:
    // 1. Define a custom BLE profile
    // 2. Register GATT service and characteristics
    // 3. Set up notification handlers
    //
    // For now, we'll use the basic advertising mechanism
    // and implement custom GATT in a future update when
    // the full BLE profile API is available
    
    manager->advertising_active = true;
    manager->state = BLEStateAdvertising;
    
    furi_mutex_release(manager->mutex);
    
    return true;
}

void ble_manager_stop_advertising(BLEManager* manager) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Stopping BLE advertising");
    
    if(manager->advertising_active) {
        // Stop advertising (if we control it)
        // furi_hal_bt_stop_advertising();
        manager->advertising_active = false;
    }
    
    manager->state = BLEStateIdle;
    
    furi_mutex_release(manager->mutex);
}

bool ble_manager_start_scan(BLEManager* manager) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Starting BLE scan");
    
    // Note: Flipper's current BLE HAL has limited scanning support
    // For full scanning, we would need:
    // 1. Access to low-level BLE scan API
    // 2. Filter for BitChat service UUID
    // 3. Parse scan results and add peers
    //
    // This is a placeholder that marks scanning as active
    // Real implementation would require custom firmware or
    // extended BLE HAL support
    
    manager->scanning_active = true;
    manager->state = BLEStateScanning;
    
    furi_mutex_release(manager->mutex);
    
    return true;
}

void ble_manager_stop_scan(BLEManager* manager) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Stopping BLE scan");
    
    if(manager->scanning_active) {
        manager->scanning_active = false;
    }
    
    manager->state = BLEStateIdle;
    
    furi_mutex_release(manager->mutex);
}

bool ble_manager_send(BLEManager* manager, const uint8_t* peer_id, const uint8_t* data, size_t size) {
    furi_assert(manager);
    furi_assert(peer_id);
    furi_assert(data);
    
    if(size > BLE_MAX_PACKET_SIZE) {
        FURI_LOG_E(TAG, "Data size %zu exceeds max packet size %d", size, BLE_MAX_PACKET_SIZE);
        return false;
    }
    
    FURI_LOG_D(TAG, "Sending %zu bytes to peer %02X%02X", size, peer_id[0], peer_id[1]);
    
    // Note: Actual BLE transmission would require:
    // 1. Find peer connection by peer_id
    // 2. Write data to TX GATT characteristic
    // 3. Handle fragmentation if size > MTU
    //
    // For now, log the operation
    // Real implementation requires custom GATT profile support
    
    return true;
}

bool ble_manager_broadcast(BLEManager* manager, const uint8_t* data, size_t size) {
    furi_assert(manager);
    furi_assert(data);
    
    if(size > BLE_MAX_PACKET_SIZE) {
        FURI_LOG_E(TAG, "Data size %zu exceeds max packet size %d", size, BLE_MAX_PACKET_SIZE);
        return false;
    }
    
    FURI_LOG_D(TAG, "Broadcasting %zu bytes to all peers", size);
    
    // Broadcast to all connected peers
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    for(size_t i = 0; i < manager->peer_count; i++) {
        ble_manager_send(manager, manager->peers[i].peer_id, data, size);
    }
    
    furi_mutex_release(manager->mutex);
    
    return true;
}

void ble_manager_set_message_callback(BLEManager* manager, BLEMessageCallback callback, void* context) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    manager->message_callback = callback;
    manager->callback_context = context;
    
    furi_mutex_release(manager->mutex);
}

size_t ble_manager_get_peers(BLEManager* manager, BLEPeer* peers, size_t max_peers) {
    furi_assert(manager);
    furi_assert(peers);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    size_t count = manager->peer_count < max_peers ? manager->peer_count : max_peers;
    memcpy(peers, manager->peers, count * sizeof(BLEPeer));
    
    furi_mutex_release(manager->mutex);
    
    return count;
}

BLEState ble_manager_get_state(BLEManager* manager) {
    furi_assert(manager);
    return manager->state;
}
