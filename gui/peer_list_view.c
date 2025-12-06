#include "views.h"
#include <gui/canvas.h>
#include <furi.h>

#define TAG "PeerListView"
#define MAX_VISIBLE_PEERS 5

typedef struct {
    char peer_names[16][32];
    size_t peer_count;
    size_t selected_index;
    size_t scroll_offset;
} PeerListViewModel;

static void peer_list_view_draw_callback(Canvas* canvas, void* model) {
    PeerListViewModel* m = model;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    
    // Draw title
    canvas_draw_str(canvas, 0, 10, "Peers");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    if(m->peer_count == 0) {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No peers found");
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Scanning...");
        return;
    }
    
    // Draw peer list
    size_t visible_count = MAX_VISIBLE_PEERS;
    if(m->peer_count < visible_count) {
        visible_count = m->peer_count;
    }
    
    for(size_t i = 0; i < visible_count; i++) {
        size_t peer_idx = m->scroll_offset + i;
        if(peer_idx >= m->peer_count) break;
        
        int y = 22 + (i * 10);
        
        // Highlight selected peer
        if(peer_idx == m->selected_index) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        
        canvas_draw_str(canvas, 2, y, m->peer_names[peer_idx]);
        
        if(peer_idx == m->selected_index) {
            canvas_set_color(canvas, ColorBlack);
        }
    }
    
    // Draw scroll indicator
    if(m->peer_count > MAX_VISIBLE_PEERS) {
        char scroll_info[16];
        snprintf(scroll_info, sizeof(scroll_info), "%zu/%zu", 
                 m->selected_index + 1, m->peer_count);
        canvas_draw_str_aligned(canvas, 128, 63, AlignRight, AlignBottom, scroll_info);
    }
}

static bool peer_list_view_input_callback(InputEvent* event, void* context) {
    View* view = context;
    
    if(event->type == InputTypeShort) {
        switch(event->key) {
            case InputKeyUp:
                with_view_model(
                    view,
                    PeerListViewModel* model,
                    {
                        if(model->selected_index > 0) {
                            model->selected_index--;
                            // Adjust scroll if needed
                            if(model->selected_index < model->scroll_offset) {
                                model->scroll_offset = model->selected_index;
                            }
                        }
                    },
                    true);
                return true;
                
            case InputKeyDown:
                with_view_model(
                    view,
                    PeerListViewModel* model,
                    {
                        if(model->selected_index < model->peer_count - 1) {
                            model->selected_index++;
                            // Adjust scroll if needed
                            if(model->selected_index >= model->scroll_offset + MAX_VISIBLE_PEERS) {
                                model->scroll_offset = model->selected_index - MAX_VISIBLE_PEERS + 1;
                            }
                        }
                    },
                    true);
                return true;
                
            case InputKeyOk:
                // Select peer for DM (TODO: implement)
                return true;
                
            case InputKeyBack:
                return false;
                
            default:
                break;
        }
    }
    
    return false;
}

View* peer_list_view_alloc(void) {
    View* view = view_alloc();
    
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(PeerListViewModel));
    view_set_draw_callback(view, peer_list_view_draw_callback);
    view_set_input_callback(view, peer_list_view_input_callback);
    view_set_context(view, view);
    
    with_view_model(
        view,
        PeerListViewModel* model,
        {
            model->peer_count = 0;
            model->selected_index = 0;
            model->scroll_offset = 0;
        },
        false);
    
    return view;
}

void peer_list_view_free(View* view) {
    view_free(view);
}

void peer_list_view_set_peers(View* view, const char** peer_names, size_t count) {
    with_view_model(
        view,
        PeerListViewModel* model,
        {
            model->peer_count = count < 16 ? count : 16;
            for(size_t i = 0; i < model->peer_count; i++) {
                strncpy(model->peer_names[i], peer_names[i], sizeof(model->peer_names[i]) - 1);
                model->peer_names[i][sizeof(model->peer_names[i]) - 1] = '\0';
            }
            model->selected_index = 0;
            model->scroll_offset = 0;
        },
        true);
}
