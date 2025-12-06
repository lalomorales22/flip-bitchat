#pragma once

#include <furi.h>
#include <furi_hal_bt.h>

// BLE Configuration
#define BLE_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // Nordic UART Service UUID
#define BLE_TX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // TX Characteristic
#define BLE_RX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // RX Characteristic

#define BLE_MAX_PACKET_SIZE 512
#define BLE_SCAN_INTERVAL_MS 1000

typedef struct BLEManager BLEManager;

typedef enum {
    BLEStateIdle,
    BLEStateAdvertising,
    BLEStateScanning,
    BLEStateConnected,
} BLEState;

typedef struct {
    uint8_t peer_id[8];
    char name[32];
    int8_t rssi;
    uint32_t last_seen;
} BLEPeer;

typedef void (*BLEMessageCallback)(void* context, const uint8_t* data, size_t size, const uint8_t* peer_id);

/**
 * @brief Allocate and initialize BLE manager
 * @return Pointer to BLEManager instance
 */
BLEManager* ble_manager_alloc(void);

/**
 * @brief Free BLE manager resources
 * @param manager Pointer to BLEManager instance
 */
void ble_manager_free(BLEManager* manager);

/**
 * @brief Start BLE advertising
 * @param manager Pointer to BLEManager instance
 * @param device_name Name to advertise
 * @return true if successful
 */
bool ble_manager_start_advertising(BLEManager* manager, const char* device_name);

/**
 * @brief Stop BLE advertising
 * @param manager Pointer to BLEManager instance
 */
void ble_manager_stop_advertising(BLEManager* manager);

/**
 * @brief Start scanning for peers
 * @param manager Pointer to BLEManager instance
 * @return true if successful
 */
bool ble_manager_start_scan(BLEManager* manager);

/**
 * @brief Stop scanning for peers
 * @param manager Pointer to BLEManager instance
 */
void ble_manager_stop_scan(BLEManager* manager);

/**
 * @brief Send data to a peer
 * @param manager Pointer to BLEManager instance
 * @param peer_id Peer identifier
 * @param data Data to send
 * @param size Size of data
 * @return true if successful
 */
bool ble_manager_send(BLEManager* manager, const uint8_t* peer_id, const uint8_t* data, size_t size);

/**
 * @brief Broadcast data to all peers
 * @param manager Pointer to BLEManager instance
 * @param data Data to broadcast
 * @param size Size of data
 * @return true if successful
 */
bool ble_manager_broadcast(BLEManager* manager, const uint8_t* data, size_t size);

/**
 * @brief Set callback for received messages
 * @param manager Pointer to BLEManager instance
 * @param callback Callback function
 * @param context Context to pass to callback
 */
void ble_manager_set_message_callback(BLEManager* manager, BLEMessageCallback callback, void* context);

/**
 * @brief Get list of discovered peers
 * @param manager Pointer to BLEManager instance
 * @param peers Array to store peers
 * @param max_peers Maximum number of peers to return
 * @return Number of peers found
 */
size_t ble_manager_get_peers(BLEManager* manager, BLEPeer* peers, size_t max_peers);

/**
 * @brief Get current BLE state
 * @param manager Pointer to BLEManager instance
 * @return Current BLE state
 */
BLEState ble_manager_get_state(BLEManager* manager);
