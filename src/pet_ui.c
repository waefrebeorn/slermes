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

static void draw_pet_spritesheet(app_state_t *app, int x, int y, int w, int h) {
    /* SDL GUI pet renderer: draws the pet as layered rounded shapes using
     * the theme palette + a per-state accent. The GUI can display graphics
     * (unlike the terminal-cell path), so this is a real sprite renderer —
     * a cat-like body: ears, head, body, tail, eyes that track the state.
     * Full PNG spritesheet decode is not available in the pure-C engine. */
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);

    pet_state_t st = pet_get_state();
    static const gc_color_t state_colors[PET_STATE_COUNT] = {
        GC_RGB(0x2d, 0x2d, 0x3f),  /* idle   — slate */
        GC_RGB(0x5a, 0x78, 0xc8),  /* wave   — blue */
        GC_RGB(0x50, 0xc8, 0x78),  /* run    — green */
        GC_RGB(0xe7, 0x5e, 0x78),  /* failed — red */
        GC_RGB(0xdc, 0xbe, 0x78),  /* review — amber */
        GC_RGB(0xaa, 0x78, 0xdc),  /* jump   — purple */
        GC_RGB(0xc8, 0xb4, 0x5a),  /* waiting— gold */
    };
    gc_color_t body = (st >= 0 && st < PET_STATE_COUNT)
        ? state_colors[st] : state_colors[PET_STATE_IDLE];

    /* Scale relative to the pet box (64px base at scale 0.33). */
    int bw = w > 16 ? w : 16;
    int bh = h > 16 ? h : 16;

    /* Tail (behind body) */
    gc_rect_t tail = { x + bw * 3 / 4, y - bh / 6, bw / 4, bh / 4 };
    gc_fill_round_rect(win, tail, bw / 8, body);

    /* Ears */
    gc_rect_t ear_l = { x + bw / 5, y - bh / 8, bw / 5, bh / 5 };
    gc_rect_t ear_r = { x + bw * 3 / 5, y - bh / 8, bw / 5, bh / 5 };
    gc_fill_round_rect(win, ear_l, bw / 10, body);
    gc_fill_round_rect(win, ear_r, bw / 10, body);

    /* Head */
    gc_rect_t head = { x + bw / 6, y, bw * 2 / 3, bh / 2 };
    gc_fill_round_rect(win, head, bw / 6, body);

    /* Eyes — state-dependent: normal dots, or wide when waiting, x when
     * failed, closed arcs when idle. */
    gc_color_t eye = t->bg_secondary;
    int eye_y = y + bh / 6;
    int eye_lx = x + bw / 3;
    int eye_rx = x + bw * 2 / 3 - bw / 12;
    if (st == PET_STATE_FAILED) {
        /* X eyes */
        gc_draw_hline(win, eye_lx - bw/14, eye_y - bw/28, bw/7, t->text);
        gc_draw_hline(win, eye_rx - bw/14, eye_y - bw/28, bw/7, t->text);
    } else {
        gc_rect_t el = { eye_lx, eye_y, bw / 12, bw / 12 };
        gc_rect_t er = { eye_rx, eye_y, bw / 12, bw / 12 };
        gc_fill_round_rect(win, el, bw / 24, eye);
        gc_fill_round_rect(win, er, bw / 24, eye);
    }

    /* Body */
    gc_rect_t body_r = { x + bw / 8, y + bh / 3, bw * 3 / 4, bh * 2 / 3 };
    gc_fill_round_rect(win, body_r, bw / 8, body);

    /* Mouth — smile when wave/jump, flat otherwise */
    int mx = x + bw / 2;
    int my = y + bh / 3;
    if (st == PET_STATE_WAVE || st == PET_STATE_JUMP) {
        gc_draw_hline(win, mx - bw / 10, my, bw / 5, t->text);
    }
    (void)app;
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
    app->pet_scale = 1.0f;
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
    
    /* Draw pet based on render mode. In the SDL GUI we can always draw
     * real graphics — terminal graphics detection (kitty/sixel/unicode)
     * is for the TUI path, where the terminal dictates the encoding. */
    draw_pet_spritesheet(app, x, y, pet_w, pet_h);
    
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