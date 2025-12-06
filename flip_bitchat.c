#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/scene_manager.h>
#include <storage/storage.h>

#include "flip_bitchat.h"
#include "ble/ble_manager.h"
#include "crypto/crypto.h"
#include "crypto/noise.h"
#include "gui/views.h"
#include "protocol/message_retry.h"
#include "protocol/fragmenter.h"
#include "storage/storage.h"

#define TAG "FlipBitChat"

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    
    // Views
    View* chat_view;
    View* peer_list_view;
    View* settings_view;
    TextInput* text_input;
    
    // BitChat components
    BLEManager* ble_manager;
    MessageRetryService* retry_service;
    MessageFragmenter* fragmenter;
    
    // Crypto
    uint8_t static_private_key[32];
    uint8_t static_public_key[32];
    NoiseHandshakeContext noise_ctx;
    
    // App state
    char username[32];
    char input_buffer[256];
    bool is_initialized;
    
    // Message counter for statistics
    size_t messages_sent;
    size_t messages_received;
} FlipBitChatApp;

// Callback for text input completion
static void text_input_callback(void* context) {
    FlipBitChatApp* app = context;
    
    // Create and send message
    FURI_LOG_I(TAG, "Sending message: %s", app->input_buffer);
    
    // TODO: Create packet and queue for sending
    // For now, just add to chat view
    chat_view_add_message(app->chat_view, app->username, app->input_buffer);
    
    app->messages_sent++;
    
    // Return to chat view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipBitChatViewChat);
}

// Callback for BLE messages
static void ble_message_callback(
    void* context,
    const uint8_t* data,
    size_t size,
    const uint8_t* peer_id) {
    
    FlipBitChatApp* app = context;
    UNUSED(peer_id);
    
    FURI_LOG_D(TAG, "Received BLE message: %zu bytes", size);
    
    // TODO: Process received packet
    // For now, just increment counter
    app->messages_received++;
}

static FlipBitChatApp* flip_bitchat_app_alloc() {
    FlipBitChatApp* app = malloc(sizeof(FlipBitChatApp));
    
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    
    // Allocate views
    app->chat_view = chat_view_alloc();
    app->peer_list_view = peer_list_view_alloc();
    app->settings_view = settings_view_alloc();
    app->text_input = text_input_alloc();
    
    // Add views to dispatcher
    view_dispatcher_add_view(app->view_dispatcher, FlipBitChatViewChat, app->chat_view);
    view_dispatcher_add_view(app->view_dispatcher, FlipBitChatViewPeerList, app->peer_list_view);
    view_dispatcher_add_view(app->view_dispatcher, FlipBitChatViewSettings, app->settings_view);
    view_dispatcher_add_view(app->view_dispatcher, FlipBitChatViewTextInput, text_input_get_view(app->text_input));
    
    // Initialize crypto
    crypto_init();
    
    // Load or generate keys
    StoredSettings settings;
    if(storage_load_settings(&settings)) {
        memcpy(app->static_private_key, settings.static_private_key, 32);
        memcpy(app->static_public_key, settings.static_public_key, 32);
        strncpy(app->username, settings.username, sizeof(app->username) - 1);
        app->username[sizeof(app->username) - 1] = '\0';
    } else {
        // Generate new keys
        crypto_generate_keypair(app->static_public_key, app->static_private_key);
        snprintf(app->username, sizeof(app->username), "User_%lu", furi_get_tick());
        
        // Save settings
        memcpy(settings.static_private_key, app->static_private_key, 32);
        memcpy(settings.static_public_key, app->static_public_key, 32);
        strncpy(settings.username, app->username, sizeof(settings.username));
        storage_save_settings(&settings);
    }
    
    app->is_initialized = false;
    app->messages_sent = 0;
    app->messages_received = 0;
    
    FURI_LOG_I(TAG, "BitChat app allocated (user: %s)", app->username);
    
    return app;
}

static void flip_bitchat_app_free(FlipBitChatApp* app) {
    furi_assert(app);
    
    // Free protocol components
    if(app->fragmenter) {
        message_fragmenter_free(app->fragmenter);
    }
    if(app->retry_service) {
        message_retry_service_free(app->retry_service);
    }
    
    // Free BLE manager
    if(app->ble_manager) {
        ble_manager_free(app->ble_manager);
    }
    
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, FlipBitChatViewChat);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBitChatViewPeerList);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBitChatViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, FlipBitChatViewTextInput);
    
    chat_view_free(app->chat_view);
    peer_list_view_free(app->peer_list_view);
    settings_view_free(app->settings_view);
    text_input_free(app->text_input);
    
    // Free GUI components
    view_dispatcher_free(app->view_dispatcher);
    
    // Deinitialize crypto
    crypto_deinit();
    
    // Close GUI record
    furi_record_close(RECORD_GUI);
    
    free(app);
    
    FURI_LOG_I(TAG, "BitChat app freed");
}

static bool flip_bitchat_back_event_callback(void* context) {
    furi_assert(context);
    FlipBitChatApp* app = context;
    
    // Return to chat view on back press
    uint32_t current_view = view_dispatcher_get_current_view(app->view_dispatcher);
    if(current_view == FlipBitChatViewChat) {
        // Exit app
        return false;
    } else {
        // Return to chat view
        view_dispatcher_switch_to_view(app->view_dispatcher, FlipBitChatViewChat);
        return true;
    }
}

int32_t flip_bitchat_app(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "Starting BitChat for Flipper Zero");
    
    FlipBitChatApp* app = flip_bitchat_app_alloc();
    
    // Initialize storage
    storage_init();
    
    // Initialize BLE manager
    app->ble_manager = ble_manager_alloc();
    ble_manager_set_message_callback(app->ble_manager, ble_message_callback, app);
    
    // Initialize protocol services
    app->retry_service = message_retry_service_alloc();
    app->fragmenter = message_fragmenter_alloc();
    
    // Set up view dispatcher
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flip_bitchat_back_event_callback);
    
    // Configure text input
    text_input_set_header_text(app->text_input, "Enter message:");
    text_input_set_result_callback(
        app->text_input,
        text_input_callback,
        app,
        app->input_buffer,
        sizeof(app->input_buffer),
        true);
    
    // Start with chat view
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipBitChatViewChat);
    
    // Start BLE
    ble_manager_start_advertising(app->ble_manager, app->username);
    ble_manager_start_scan(app->ble_manager);
    
    FURI_LOG_I(TAG, "BitChat initialized, entering main loop");
    
    // Run view dispatcher
    view_dispatcher_run(app->view_dispatcher);
    
    // Cleanup
    flip_bitchat_app_free(app);
    
    FURI_LOG_I(TAG, "BitChat app exited");
    
    return 0;
}
