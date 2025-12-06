#include "views.h"
#include <gui/canvas.h>
#include <furi.h>

#define TAG "ChatView"
#define MAX_MESSAGES 32
#define MAX_MESSAGE_LENGTH 64

typedef struct {
    char messages[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
    size_t message_count;
    size_t scroll_offset;
} ChatViewModel;

static void chat_view_draw_callback(Canvas* canvas, void* model) {
    ChatViewModel* m = model;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    
    // Draw title
    canvas_draw_str(canvas, 0, 10, "BitChat");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    // Draw messages (max 5 visible lines on 128x64 display)
    size_t visible_lines = 5;
    size_t start_idx = m->scroll_offset;
    
    for(size_t i = 0; i < visible_lines && (start_idx + i) < m->message_count; i++) {
        canvas_draw_str(canvas, 2, 22 + (i * 10), m->messages[start_idx + i]);
    }
    
    // Draw scroll indicator
    if(m->message_count > visible_lines) {
        char scroll_info[16];
        snprintf(scroll_info, sizeof(scroll_info), "[%zu/%zu]", 
                 m->scroll_offset + 1, m->message_count - visible_lines + 1);
        canvas_draw_str_aligned(canvas, 128, 63, AlignRight, AlignBottom, scroll_info);
    }
}

static bool chat_view_input_callback(InputEvent* event, void* context) {
    View* view = context;
    
    if(event->type == InputTypeShort) {
        switch(event->key) {
            case InputKeyUp:
                // Scroll up
                with_view_model(
                    view,
                    ChatViewModel* model,
                    {
                        if(model->scroll_offset > 0) {
                            model->scroll_offset--;
                        }
                    },
                    true);
                return true;
            case InputKeyDown:
                // Scroll down
                with_view_model(
                    view,
                    ChatViewModel* model,
                    {
                        size_t visible_lines = 5;
                        if(model->message_count > visible_lines &&
                           model->scroll_offset < model->message_count - visible_lines) {
                            model->scroll_offset++;
                        }
                    },
                    true);
                return true;
            case InputKeyOk:
                // Open message compose (TODO: implement)
                return true;
            case InputKeyBack:
                return false;
            default:
                break;
        }
    }
    
    return false;
}

View* chat_view_alloc(void) {
    View* view = view_alloc();
    
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(ChatViewModel));
    view_set_draw_callback(view, chat_view_draw_callback);
    view_set_input_callback(view, chat_view_input_callback);
    view_set_context(view, view); // Set view as its own context for input callback
    
    with_view_model(
        view,
        ChatViewModel* model,
        {
            model->message_count = 0;
            model->scroll_offset = 0;
        },
        false);
    
    return view;
}

void chat_view_free(View* view) {
    view_free(view);
}

void chat_view_add_message(View* view, const char* sender, const char* message) {
    with_view_model(
        view,
        ChatViewModel* model,
        {
            if(model->message_count < MAX_MESSAGES) {
                snprintf(
                    model->messages[model->message_count],
                    MAX_MESSAGE_LENGTH,
                    "%s: %s",
                    sender,
                    message);
                model->message_count++;
            } else {
                // Shift messages up and add new one at the end
                for(size_t i = 0; i < MAX_MESSAGES - 1; i++) {
                    memcpy(
                        model->messages[i],
                        model->messages[i + 1],
                        MAX_MESSAGE_LENGTH);
                }
                snprintf(
                    model->messages[MAX_MESSAGES - 1],
                    MAX_MESSAGE_LENGTH,
                    "%s: %s",
                    sender,
                    message);
            }
        },
        true);
}

void chat_view_clear(View* view) {
    with_view_model(
        view,
        ChatViewModel* model,
        {
            model->message_count = 0;
            model->scroll_offset = 0;
        },
        true);
}
