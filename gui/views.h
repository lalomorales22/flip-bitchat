#pragma once

#include <gui/view.h>
#include <gui/view_dispatcher.h>

// Chat view functions
View* chat_view_alloc(void);
void chat_view_free(View* view);
void chat_view_add_message(View* view, const char* sender, const char* message);
void chat_view_clear(View* view);

// Peer list view functions
View* peer_list_view_alloc(void);
void peer_list_view_free(View* view);
void peer_list_view_set_peers(View* view, const char** peer_names, size_t count);

// Settings view functions
View* settings_view_alloc(void);
void settings_view_free(View* view);
