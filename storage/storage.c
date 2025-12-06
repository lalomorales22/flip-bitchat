#include "storage.h"
#include <furi.h>
#include <storage/storage.h>

bool storage_init(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    
    // Create directory if it doesn't exist
    storage_common_mkdir(storage, STORAGE_DIR);
    
    furi_record_close(RECORD_STORAGE);
    
    FURI_LOG_I(STORAGE_TAG, "Storage initialized");
    return true;
}

bool storage_save_message(const StoredMessage* msg) {
    furi_assert(msg);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    // Open file in append mode
    bool success = storage_file_open(
        file,
        MESSAGES_FILE,
        FSAM_WRITE,
        FSOM_OPEN_APPEND);
    
    if(!success) {
        // Try creating the file
        success = storage_file_open(
            file,
            MESSAGES_FILE,
            FSAM_WRITE,
            FSOM_CREATE_ALWAYS);
    }
    
    if(success) {
        // Write message in simple format: timestamp|sender|content\n
        FuriString* line = furi_string_alloc();
        furi_string_printf(
            line,
            "%llu|%s|%s\n",
            msg->timestamp,
            msg->sender,
            msg->content);
        
        storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
        
        furi_string_free(line);
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    return success;
}

size_t storage_load_messages(StoredMessage* messages, size_t max_messages) {
    furi_assert(messages);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    size_t count = 0;
    
    if(storage_file_open(file, MESSAGES_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        
        while(count < max_messages && storage_file_read(file, line, 1024)) {
            // Parse line: timestamp|sender|content
            const char* str = furi_string_get_cstr(line);
            
            // Find first separator
            const char* sep1 = strchr(str, '|');
            if(!sep1) continue;
            
            // Find second separator
            const char* sep2 = strchr(sep1 + 1, '|');
            if(!sep2) continue;
            
            // Parse timestamp
            messages[count].timestamp = strtoull(str, NULL, 10);
            
            // Parse sender
            size_t sender_len = sep2 - sep1 - 1;
            if(sender_len >= sizeof(messages[count].sender)) {
                sender_len = sizeof(messages[count].sender) - 1;
            }
            memcpy(messages[count].sender, sep1 + 1, sender_len);
            messages[count].sender[sender_len] = '\0';
            
            // Parse content (remove newline)
            strncpy(messages[count].content, sep2 + 1, sizeof(messages[count].content) - 1);
            messages[count].content[sizeof(messages[count].content) - 1] = '\0';
            
            // Remove trailing newline
            size_t len = strlen(messages[count].content);
            if(len > 0 && messages[count].content[len - 1] == '\n') {
                messages[count].content[len - 1] = '\0';
            }
            
            count++;
            furi_string_reset(line);
        }
        
        furi_string_free(line);
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    FURI_LOG_I(STORAGE_TAG, "Loaded %zu messages", count);
    
    return count;
}

bool storage_clear_messages(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    
    bool success = storage_common_remove(storage, MESSAGES_FILE);
    
    furi_record_close(RECORD_STORAGE);
    
    FURI_LOG_I(STORAGE_TAG, "Messages cleared");
    
    return success;
}

bool storage_save_settings(const StoredSettings* settings) {
    furi_assert(settings);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    bool success = storage_file_open(
        file,
        SETTINGS_FILE,
        FSAM_WRITE,
        FSOM_CREATE_ALWAYS);
    
    if(success) {
        // Write username
        storage_file_write(file, settings->username, sizeof(settings->username));
        
        // Write keys
        storage_file_write(file, settings->static_private_key, 32);
        storage_file_write(file, settings->static_public_key, 32);
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    FURI_LOG_I(STORAGE_TAG, "Settings saved");
    
    return success;
}

bool storage_load_settings(StoredSettings* settings) {
    furi_assert(settings);
    
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    bool success = false;
    
    if(storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Read username
        if(storage_file_read(file, settings->username, sizeof(settings->username)) == sizeof(settings->username)) {
            // Read keys
            if(storage_file_read(file, settings->static_private_key, 32) == 32) {
                if(storage_file_read(file, settings->static_public_key, 32) == 32) {
                    success = true;
                }
            }
        }
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    
    if(success) {
        FURI_LOG_I(STORAGE_TAG, "Settings loaded");
    } else {
        FURI_LOG_W(STORAGE_TAG, "Settings not found or invalid");
    }
    
    return success;
}
