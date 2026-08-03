/*
 * pet_ui.c — Pet UI Rendering
 *
 * Handles rendering and animation of the desktop pet.
 */

#define _GNU_SOURCE
#include "pet_ui.h"
#include "app_state_internal.h"
#include "gui_core.h"
#include "pet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ══════════════════════════════════════════════════════════════════════ */

static void draw_pet_unicode(app_state_t *app, int x, int y, int w, int h) {
    gc_theme_t *t = app_get_theme(app);
    gc_font_t *font = gc_get_font(app_get_window(app));
    
    /* Simple ASCII pet based on state */
    const char *pet_frames[] = {
        "  /\\_/\\  ",
        " ( o.o ) ",
        "  > ^ <  ",
        "  /\\_/\\  ",
        " ( -.- ) ",
        "  >   <  ",
    };
    
    int frame = app_pet_frame(app) % 6;
    gc_draw_text(app_get_window(app), font, pet_frames[frame], x, y, t->accent);
}

static void draw_pet_spritesheet(app_state_t *app, int x, int y, int w, int h) {
    /* Placeholder for spritesheet rendering */
    (void)app; (void)x; (void)y; (void)w; (void)h;
    /* Would load and render actual pet sprites */
}

/* ══════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════ */

void pet_ui_init(app_state_t *app) {
    if (!app) return;
    
    /* Initialize pet system */
    if (!slermes_initialized()) slermes_init();
    
    pet_config_t cfg = {0};
    cfg.enabled = true;
    cfg.scale = 0.33f;
    cfg.unicode_cols = 24;
    pet_init(&cfg);
    
    /* Populate the in-app gallery from the real pet store (~/.slermes/pets/),
     * so the gallery lists actual installed pets instead of nothing. */
    pet_installed_t pets[16];
    int n = pet_installed_pets(pets, 16);
    for (int i = 0; i < n && i < 16; i++) {
        snprintf(app->pet_names[i], sizeof(app->pet_names[i]), "%s",
                 pets[i].display_name[0] ? pets[i].display_name : pets[i].slug);
    }
    app->pet_count = n > 16 ? 16 : n;
    if (app->pet_count > 0 && app->pet_selected >= app->pet_count)
        app->pet_selected = 0;
    
    app->pet_active = true;
    app->pet_type = 0;
    app->pet_frame = 0;
    app->pet_frame_tick = 0;
    app->pet_scale = 0.33f;
}

void pet_ui_draw(app_state_t *app) {
    if (!app || !app_pet_active(app) || !app_get_window(app)) return;
    
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);
    
    /* Calculate pet position */
    float scale = app_pet_scale(app);
    int pet_w = (int)(64 * scale);
    int pet_h = (int)(64 * scale);
    
    /* Position near bottom of sidebar or in chat area */
    int x = (int)app_pet_x(app);
    int y = (int)app_pet_y(app);
    
    /* Clamp to window */
    int win_w = gc_window_w(win);
    int win_h = gc_window_h(win);
    if (x + pet_w > win_w) x = win_w - pet_w;
    if (y + pet_h > win_h - STATUSBAR_H) y = win_h - STATUSBAR_H - pet_h;
    if (x < 0) x = 0;
    if (y < TITLEBAR_H) y = TITLEBAR_H;
    
    /* Update position */
    app->pet_x = x;
    app->pet_y = y;
    
    /* Draw pet based on render mode */
    pet_render_mode_t mode = pet_detect_terminal_graphics();
    if (mode == PET_MODE_UNICODE || mode == PET_MODE_AUTO) {
        draw_pet_unicode(app, x, y, pet_w, pet_h);
    } else {
        draw_pet_spritesheet(app, x, y, pet_w, pet_h);
    }
    
    /* Draw gallery if open */
    if (app_pet_show_gallery(app)) {
        gc_font_t *font_small = gc_get_font_small(win);
        int gal_x = app_sidebar_w(app) + 20;
        int gal_y = TITLEBAR_H + 50;
        int gal_w = app_chat_w(app) - 40;
        int gal_h = win_h - TITLEBAR_H - STATUSBAR_H - 100;
        
        gc_rect_t gal_bg = {gal_x, gal_y, gal_w, gal_h};
        gc_fill_round_rect(win, gal_bg, 8, t->bg_card);
        gc_draw_rect(win, gal_bg, 1, t->border);
        gc_draw_text(win, font_small, "Pet Gallery (Press ESC to close)", gal_x + 10, gal_y + 10, t->text);
        
        /* List installed pets */
        for (int i = 0; i < app_pet_count(app); i++) {
            gc_draw_text(win, font_small, app_pet_name(app, i), gal_x + 10, gal_y + 30 + i * 20, 
                         i == app_pet_selected(app) ? t->accent : t->text);
        }
    }
}

void pet_ui_update_animation(app_state_t *app) {
    if (!app || !app_pet_active(app)) return;
    
    app->pet_frame_tick++;
    if (app->pet_frame_tick >= 10) { /* ~60fps / 10 = 6fps */
        app->pet_frame_tick = 0;
        app->pet_frame = (app->pet_frame + 1) % 6;
    }
    
    /* Apply physics for floating animation */
    app->pet_vx *= 0.99f;
    app->pet_vy *= 0.99f;
    app->pet_x += app->pet_vx;
    app->pet_y += app->pet_vy;
    
    /* Bounce off edges */
    gc_window_t *win = app_get_window(app);
    if (win) {
        int win_w = gc_window_w(win);
        int win_h = gc_window_h(win);
        int pet_w = (int)(64 * app->pet_scale);
        int pet_h = (int)(64 * app->pet_scale);
        
        if (app->pet_x <= 0 || app->pet_x + pet_w >= win_w) {
            app->pet_vx = -app->pet_vx;
            app->pet_x = app->pet_x < 0 ? 0 : win_w - pet_w;
        }
        if (app->pet_y <= TITLEBAR_H || app->pet_y + pet_h >= win_h - STATUSBAR_H) {
            app->pet_vy = -app->pet_vy;
            app->pet_y = app->pet_y < TITLEBAR_H ? TITLEBAR_H : win_h - STATUSBAR_H - pet_h;
        }
    }
}

int pet_ui_derive_state(app_state_t *app) {
    if (!app) return PET_STATE_IDLE;
    
    bool busy = app_api_busy(app);
    bool awaiting = app_composer_focused(app) && app_composer_buf(app)[0] != '\0';
    bool error = false; /* Would check for error state */
    bool celebrate = false;
    bool just_completed = false;
    bool tool_running = false; /* Would check for running tools */
    bool reasoning = false; /* Would check for reasoning state */
    
    pet_state_t st = pet_state_derive(busy, awaiting, error, celebrate,
                                      just_completed, tool_running, reasoning);
    /* Push into the pet system so pet_info_json / pet_cells_json / TUI RPC
     * reflect the same state the GUI derives. */
    pet_update_state(busy, awaiting, error, celebrate,
                     just_completed, tool_running, reasoning);
    return st;
}

bool pet_ui_handle_click(app_state_t *app, int mx, int my) {
    if (!app || !app_pet_active(app)) return false;
    
    gc_window_t *win = app_get_window(app);
    if (!win) return false;
    
    /* Gallery open: clicks select a pet row; clicks outside close it. */
    if (app_pet_show_gallery(app)) {
        int gal_x = app_sidebar_w(app) + 20;
        int gal_y = TITLEBAR_H + 50;
        int gal_w = app_chat_w(app) - 40;
        int gal_h = gc_window_h(win) - TITLEBAR_H - STATUSBAR_H - 100;
        if (mx >= gal_x && mx < gal_x + gal_w && my >= gal_y && my < gal_y + gal_h) {
            int row = (my - gal_y - 30) / 20;
            if (row >= 0 && row < app->pet_count) {
                app->pet_selected = row;
                if (app->pet_count > 0) {
                    /* Select the pet in the pet system by slug. */
                    pet_installed_t pets[16];
                    int n = pet_installed_pets(pets, 16);
                    if (row < n) pet_select(pets[row].slug);
                }
            }
            return true;
        }
        /* Click outside the gallery closes it. */
        app_set_pet_show_gallery(app, false);
        return true;
    }
    
    float scale = app_pet_scale(app);
    int pet_w = (int)(64 * scale);
    int pet_h = (int)(64 * scale);
    int x = (int)app_pet_x(app);
    int y = (int)app_pet_y(app);
    
    if (mx >= x && mx < x + pet_w && my >= y && my < y + pet_h) {
        /* Clicked on pet - toggle gallery or change pet */
        app_set_pet_show_gallery(app, !app_pet_show_gallery(app));
        /* Apply small impulse */
        app->pet_vx += (float)((rand() % 200) - 100) / 100.0f;
        app->pet_vy += (float)((rand() % 200) - 100) / 100.0f;
        return true;
    }
    
    return false;
}