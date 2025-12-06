#include "views.h"
#include <gui/canvas.h>
#include <furi.h>

#define TAG "SettingsView"

typedef struct {
    char username[32];
    char fingerprint[65];  // Hex string of 32-byte hash
    bool show_stats;
    size_t messages_sent;
    size_t messages_received;
} SettingsViewModel;

static void settings_view_draw_callback(Canvas* canvas, void* model) {
    SettingsViewModel* m = model;
    
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    
    // Draw title
    canvas_draw_str(canvas, 0, 10, "Settings");
    canvas_draw_line(canvas, 0, 12, 128, 12);
    
    // Draw username
    char username_line[48];
    snprintf(username_line, sizeof(username_line), "User: %s", m->username);
    canvas_draw_str(canvas, 2, 24, username_line);
    
    // Draw fingerprint (first 16 chars)
    char fp_line[24];
    snprintf(fp_line, sizeof(fp_line), "FP: %.16s...", m->fingerprint);
    canvas_draw_str(canvas, 2, 36, fp_line);
    
    // Draw statistics
    if(m->show_stats) {
        char stats_line[32];
        snprintf(stats_line, sizeof(stats_line), "Sent: %zu", m->messages_sent);
        canvas_draw_str(canvas, 2, 48, stats_line);
        
        snprintf(stats_line, sizeof(stats_line), "Rcvd: %zu", m->messages_received);
        canvas_draw_str(canvas, 2, 58, stats_line);
    }
}

static bool settings_view_input_callback(InputEvent* event, void* context) {
    UNUSED(context);
    
    if(event->type == InputTypeShort) {
        switch(event->key) {
            case InputKeyOk:
                // Edit username (TODO: implement)
                return true;
                
            case InputKeyBack:
                return false;
                
            default:
                break;
        }
    }
    
    return false;
}

View* settings_view_alloc(void) {
    View* view = view_alloc();
    
    view_allocate_model(view, ViewModelTypeLockFree, sizeof(SettingsViewModel));
    view_set_draw_callback(view, settings_view_draw_callback);
    view_set_input_callback(view, settings_view_input_callback);
    view_set_context(view, view);
    
    with_view_model(
        view,
        SettingsViewModel* model,
        {
            snprintf(model->username, sizeof(model->username), "User");
            memset(model->fingerprint, '0', sizeof(model->fingerprint) - 1);
            model->fingerprint[sizeof(model->fingerprint) - 1] = '\0';
            model->show_stats = true;
            model->messages_sent = 0;
            model->messages_received = 0;
        },
        false);
    
    return view;
}

void settings_view_free(View* view) {
    view_free(view);
}
