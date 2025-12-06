#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/scene_manager.h>
#include <storage/storage.h>

#include "flip_bitchat.h"
#include "ble/ble_manager.h"
#include "gui/views.h"

#define TAG "FlipBitChat"

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    TextInput* text_input;
    
    // BitChat components
    BLEManager* ble_manager;
    
    // App state
    char username[32];
    bool is_initialized;
} FlipBitChatApp;

static FlipBitChatApp* flip_bitchat_app_alloc() {
    FlipBitChatApp* app = malloc(sizeof(FlipBitChatApp));
    
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    
    // Initialize username
    snprintf(app->username, sizeof(app->username), "User_%lu", furi_get_tick());
    app->is_initialized = false;
    
    FURI_LOG_I(TAG, "BitChat app allocated");
    
    return app;
}

static void flip_bitchat_app_free(FlipBitChatApp* app) {
    furi_assert(app);
    
    // Free BLE manager
    if(app->ble_manager) {
        ble_manager_free(app->ble_manager);
    }
    
    // Free GUI components
    view_dispatcher_free(app->view_dispatcher);
    
    // Close GUI record
    furi_record_close(RECORD_GUI);
    
    free(app);
    
    FURI_LOG_I(TAG, "BitChat app freed");
}

static bool flip_bitchat_back_event_callback(void* context) {
    furi_assert(context);
    FlipBitChatApp* app = context;
    
    return scene_manager_handle_back_event(app->scene_manager);
}

int32_t flip_bitchat_app(void* p) {
    UNUSED(p);
    
    FURI_LOG_I(TAG, "Starting BitChat for Flipper Zero");
    
    FlipBitChatApp* app = flip_bitchat_app_alloc();
    
    // Initialize BLE manager
    app->ble_manager = ble_manager_alloc();
    
    // Set up view dispatcher
    view_dispatcher_enable_queue(app->view_dispatcher);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, flip_bitchat_back_event_callback);
    
    // TODO: Initialize scenes and views
    // TODO: Start BLE advertising
    
    FURI_LOG_I(TAG, "BitChat initialized, entering main loop");
    
    // Run view dispatcher
    view_dispatcher_run(app->view_dispatcher);
    
    // Cleanup
    flip_bitchat_app_free(app);
    
    FURI_LOG_I(TAG, "BitChat app exited");
    
    return 0;
}
