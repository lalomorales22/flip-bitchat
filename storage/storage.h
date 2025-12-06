#pragma once

#include <furi.h>
#include <storage/storage.h>

#define STORAGE_TAG "BitChatStorage"
#define STORAGE_DIR "/ext/apps_data/bitchat"
#define MESSAGES_FILE STORAGE_DIR "/messages.txt"
#define SETTINGS_FILE STORAGE_DIR "/settings.txt"
#define MAX_STORED_MESSAGES 64

typedef struct {
    uint64_t timestamp;
    char sender[32];
    char content[256];
} StoredMessage;

typedef struct {
    char username[32];
    uint8_t static_private_key[32];
    uint8_t static_public_key[32];
} StoredSettings;

/**
 * @brief Initialize storage (create directory if needed)
 * @return true if successful
 */
bool storage_init(void);

/**
 * @brief Save a message to storage
 * @param msg Message to save
 * @return true if successful
 */
bool storage_save_message(const StoredMessage* msg);

/**
 * @brief Load messages from storage
 * @param messages Array to store messages
 * @param max_messages Maximum number of messages to load
 * @return Number of messages loaded
 */
size_t storage_load_messages(StoredMessage* messages, size_t max_messages);

/**
 * @brief Clear all stored messages
 * @return true if successful
 */
bool storage_clear_messages(void);

/**
 * @brief Save settings to storage
 * @param settings Settings to save
 * @return true if successful
 */
bool storage_save_settings(const StoredSettings* settings);

/**
 * @brief Load settings from storage
 * @param settings Settings structure to fill
 * @return true if successful
 */
bool storage_load_settings(StoredSettings* settings);
