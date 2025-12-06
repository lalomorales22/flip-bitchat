#pragma once

#include <furi.h>
#include <furi_hal.h>

// App configuration
#define FLIP_BITCHAT_VERSION "0.1.0"
#define MAX_USERNAME_LENGTH 32
#define MAX_MESSAGE_LENGTH 256
#define MAX_PEERS 16
#define BLE_MESH_TTL 7

// View IDs
typedef enum {
    FlipBitChatViewChat,
    FlipBitChatViewPeerList,
    FlipBitChatViewSettings,
    FlipBitChatViewTextInput,
} FlipBitChatView;

// Scene IDs
typedef enum {
    FlipBitChatSceneChat,
    FlipBitChatScenePeerList,
    FlipBitChatSceneSettings,
    FlipBitChatSceneTextInput,
} FlipBitChatScene;
