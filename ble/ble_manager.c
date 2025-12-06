#include "ble_manager.h"
#include <furi.h>
#include <furi_hal_bt.h>

#define TAG "BLEManager"

struct BLEManager {
    BLEState state;
    BLEMessageCallback message_callback;
    void* callback_context;
    
    // Peer tracking
    BLEPeer peers[16];
    size_t peer_count;
    
    FuriMutex* mutex;
};

BLEManager* ble_manager_alloc(void) {
    BLEManager* manager = malloc(sizeof(BLEManager));
    
    manager->state = BLEStateIdle;
    manager->message_callback = NULL;
    manager->callback_context = NULL;
    manager->peer_count = 0;
    
    manager->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    
    FURI_LOG_I(TAG, "BLE Manager allocated");
    
    return manager;
}

void ble_manager_free(BLEManager* manager) {
    furi_assert(manager);
    
    // Stop any active operations
    if(manager->state == BLEStateAdvertising) {
        ble_manager_stop_advertising(manager);
    }
    if(manager->state == BLEStateScanning) {
        ble_manager_stop_scan(manager);
    }
    
    furi_mutex_free(manager->mutex);
    free(manager);
    
    FURI_LOG_I(TAG, "BLE Manager freed");
}

bool ble_manager_start_advertising(BLEManager* manager, const char* device_name) {
    furi_assert(manager);
    furi_assert(device_name);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Starting BLE advertising as '%s'", device_name);
    
    // TODO: Implement actual BLE advertising using furi_hal_bt APIs
    // This is a placeholder implementation
    // Real implementation would use:
    // - furi_hal_bt_start_advertising() to broadcast presence
    // - Custom GATT service/characteristics for BitChat protocol
    
    manager->state = BLEStateAdvertising;
    
    furi_mutex_release(manager->mutex);
    
    return true;
}

void ble_manager_stop_advertising(BLEManager* manager) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Stopping BLE advertising");
    
    // TODO: Stop BLE advertising
    
    manager->state = BLEStateIdle;
    
    furi_mutex_release(manager->mutex);
}

bool ble_manager_start_scan(BLEManager* manager) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Starting BLE scan");
    
    // TODO: Implement BLE scanning using furi_hal_bt APIs
    // Real implementation would use:
    // - furi_hal_bt_start_scan() to discover peers
    // - Filter for BitChat service UUID
    // - Add discovered peers to peers array
    
    manager->state = BLEStateScanning;
    
    furi_mutex_release(manager->mutex);
    
    return true;
}

void ble_manager_stop_scan(BLEManager* manager) {
    furi_assert(manager);
    
    furi_mutex_acquire(manager->mutex, FuriWaitForever);
    
    FURI_LOG_I(TAG, "Stopping BLE scan");
    
    // TODO: Stop BLE scanning
    
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
    
    FURI_LOG_D(TAG, "Sending %zu bytes to peer", size);
    
    // TODO: Implement actual BLE send using GATT characteristics
    // Real implementation would:
    // - Find peer connection by peer_id
    // - Write data to TX characteristic
    // - Handle fragmentation if needed
    
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
    
    // TODO: Broadcast to all connected peers
    // Loop through peers array and send to each
    
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
