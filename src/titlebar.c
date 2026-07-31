/*
 * titlebar.c — Titlebar and Statusbar Rendering
 *
 * Handles drawing and interaction for the titlebar and statusbar.
 */

#define _GNU_SOURCE
#include "titlebar.h"
#include "app_state_internal.h"
#include "gui_core.h"
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════
 * Titlebar Drawing
 * ══════════════════════════════════════════════════════════════════════ */

void titlebar_draw(app_state_t *app) {
    if (!app || !app_get_window(app)) return;
    
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);
    int w = gc_window_w(win);
    
    /* Titlebar background */
    gc_rect_t tb_bg = {0, 0, w, TITLEBAR_H};
    gc_fill_rect(win, tb_bg, t->bg_secondary);
    gc_draw_hline(win, 0, TITLEBAR_H - 1, w, t->border);
    
    /* App title */
    gc_font_t *font_bold = gc_get_font_bold(win);
    int fh = gc_font_height(font_bold);
    gc_draw_text(win, font_bold, "Hermes Slermes", 16, (TITLEBAR_H - fh) / 2, t->text);
    
    /* Current session/view title */
    gc_font_t *font = gc_get_font(win);
    int sidebar_w = app_sidebar_w(app);
    int chat_w = w - sidebar_w;
    int chat_x = sidebar_w;
    
    if (app_current_view_name(app)[0]) {
        gc_draw_text(win, font, app_current_view_name(app), chat_x + 20, (TITLEBAR_H - gc_font_height(font)) / 2, t->text_secondary);
    }
    
    /* Titlebar tools (right side) */
    const char *tool_chars[] = {"\xf0\x9f\x94\x8a", "\xe2\x8c\xa8", "\xe2\x9a\x99", "\xe2\x96\xa0"};
    const char *tool_tooltips[] = {"Search", "Theme", "Haptics", "Settings"};
    
    int tx = w - 16 - 20;
    for (int i = 3; i >= 0; i--) {
        gc_rect_t tool_bg = {tx, 8, 20, 20};
        bool hov = false; /* hover tracking would be in event handling */
        gc_fill_round_rect(win, tool_bg, 4, hov ? GC_RGBA(255,255,255,30) : GC_RGBA(0,0,0,0));
        gc_draw_text(win, font, tool_chars[i], tx + 2, 8 + (20 - gc_font_height(font)) / 2, t->text_secondary);
        tx -= 24;
    }
}

void statusbar_draw(app_state_t *app) {
    if (!app || !app_get_window(app)) return;
    
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);
    int w = gc_window_w(win);
    int h = gc_window_h(win);
    
    /* Statusbar background */
    gc_rect_t sb_bg = {0, h - STATUSBAR_H, w, STATUSBAR_H};
    gc_fill_rect(win, sb_bg, t->bg_secondary);
    gc_draw_hline(win, 0, h - STATUSBAR_H, w, t->border);
    
    gc_font_t *font_small = gc_get_font_small(win);
    int sfh = gc_font_height(font_small);
    int y = h - STATUSBAR_H + (STATUSBAR_H - sfh) / 2;
    
    /* Left side: model pill info */
    app_session_entry_t *s = app_get_session(app, app_selected_session(app));
    if (s && s->model[0]) {
        char model_info[64];
        snprintf(model_info, sizeof(model_info), "Model: %s", s->model);
        gc_draw_text(win, font_small, model_info, 16, y, t->text_secondary);
    }
    
    /* Center: API status */
    if (app_api_busy(app)) {
        gc_draw_text(win, font_small, app_api_status(app), w / 2 - 50, y, t->warn);
    } else if (app_api_status(app)[0]) {
        gc_draw_text(win, font_small, app_api_status(app), w / 2 - 50, y, t->success);
    }
    
    /* Right side: stats */
    char stats[128];
    snprintf(stats, sizeof(stats), "Sessions: %d | Messages: %d", app_total_sessions(app), app_total_messages(app));
    gc_draw_text(win, font_small, stats, w - gc_text_width(font_small, stats) - 16, y, t->text_dim);
}

int titlebar_handle_click(app_state_t *app, int mx, int my) {
    if (!app) return TITLEBAR_HIT_NONE;
    
    gc_window_t *win = app_get_window(app);
    int w = gc_window_w(win);
    
    if (my >= TITLEBAR_H) return TITLEBAR_HIT_NONE;
    
    /* Title area */
    if (mx >= 16 && mx < w - 100) {
        return TITLEBAR_HIT_TITLE;
    }
    
    /* Tools on the right */
    int tx = w - 16 - 20;
    for (int i = 3; i >= 0; i--) {
        if (mx >= tx && mx < tx + 20 && my >= 8 && my < 28) {
            switch (i) {
                case TOOL_THEME:
                    app_toggle_theme(app);
                    break;
                case TOOL_SIDEBAR:
                    app_toggle_sidebar(app);
                    break;
                case TOOL_HAPTICS:
                    app->haptics_muted = !app->haptics_muted;
                    break;
                case TOOL_SETTINGS:
                    /* Open settings */
                    break;
            }
            return TITLEBAR_HIT_TOOL;
        }
        tx -= 24;
    }
    
    return TITLEBAR_HIT_NONE;
}

void titlebar_handle_hover(app_state_t *app, int mx, int my) {
    /* Update hover state for tool buttons */
    (void)app; (void)mx; (void)my;
}

bool statusbar_handle_click(app_state_t *app, int mx, int my) {
    if (!app) return false;
    
    gc_window_t *win = app_get_window(app);
    int w = gc_window_w(win);
    int h = gc_window_h(win);
    
    if (my < h - STATUSBAR_H) return false;
    
    /* Could handle clicks on model pill, status, etc. */
    return false;
}

/* Helper function to get total sessions (defined in session_db.h) */
int app_total_sessions(app_state_t *app) {
    return app ? app->total_sessions : 0;
}

int app_total_messages(app_state_t *app) {
    return app ? app->total_messages : 0;
}
