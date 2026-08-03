/*
 * session_switcher.c — Ctrl+Tab session switcher HUD (v484 parity)
 *
 * Floating HUD listing the most recent sessions (the in-memory list is
 * already ordered started_at DESC). Ctrl+Tab / Ctrl+Shift+Tab cycle the
 * selection, 1-9 jump to a slot, Enter switches, Esc closes. Self-contained
 * module over the opaque app_state API — no internals.
 */

#include "session_switcher.h"
#include "app_state.h"
#include "gui_core.h"
#include "session_db.h"

#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#define SWITCHER_MAX_SLOTS 9

static struct {
    bool visible;
    int  selected;   /* slot index 0..SWITCHER_MAX_SLOTS-1 */
} g_sw = {false, 0};

/* ── lifecycle ──────────────────────────────────────────────────────── */

void session_switcher_open(app_state_t *app) {
    if (!app) return;
    if (app_session_count(app) == 0) return;
    g_sw.visible = true;
    g_sw.selected = 0;
}

void session_switcher_close(app_state_t *app) {
    (void)app;
    g_sw.visible = false;
}

void session_switcher_toggle(app_state_t *app) {
    if (g_sw.visible) session_switcher_close(app);
    else session_switcher_open(app);
}

bool session_switcher_visible(app_state_t *app) {
    (void)app;
    return g_sw.visible;
}

/* ── selection ──────────────────────────────────────────────────────── */

void session_switcher_cycle(app_state_t *app, int dir) {
    if (!app || !g_sw.visible) return;
    int n = app_session_count(app);
    if (n == 0) return;
    int max_slot = n < SWITCHER_MAX_SLOTS ? n : SWITCHER_MAX_SLOTS;
    g_sw.selected += dir;
    if (g_sw.selected < 0) g_sw.selected = max_slot - 1;
    if (g_sw.selected >= max_slot) g_sw.selected = 0;
}

void session_switcher_select(app_state_t *app, int slot) {
    if (!app || !g_sw.visible) return;
    int n = app_session_count(app);
    if (n == 0) return;
    if (slot < 0 || slot >= SWITCHER_MAX_SLOTS) return;
    if (slot >= n) return;
    /* Commit: switch to the session at that slot. */
    app_set_selected_session(app, slot);
    app_set_current_view(app, 0);
    app_set_current_view_name(app, "Chat");
    app_set_chat_scroll(app, 0);
    session_db_load_messages(app, slot);
    g_sw.visible = false;
}

/* ── drawing ────────────────────────────────────────────────────────── */

void session_switcher_draw(app_state_t *app) {
    if (!app || !g_sw.visible) return;
    gc_window_t *win = app_get_window(app);
    if (!win) return;
    const gc_theme_t *t = app_get_theme(app);

    int w = gc_window_w(win);
    int h = gc_window_h(win);
    int n = app_session_count(app);
    if (n == 0) { g_sw.visible = false; return; }

    int max_slot = n < SWITCHER_MAX_SLOTS ? n : SWITCHER_MAX_SLOTS;
    if (g_sw.selected >= max_slot) g_sw.selected = max_slot - 1;

    /* HUD geometry: centered, 320px wide, 30px rows. */
    int hud_w = 340;
    int row_h = 30;
    int hud_h = 34 + row_h * max_slot + 10;
    int ox = (w - hud_w) / 2;
    int oy = (h - hud_h) / 3;

    /* Backdrop + surface. */
    gc_rect_t bg = {0, 0, w, h};
    gc_fill_rect(win, bg, GC_RGBA(0, 0, 0, 200));
    gc_rect_t surf = {ox, oy, hud_w, hud_h};
    gc_fill_round_rect(win, surf, 10, t->bg_secondary);
    gc_draw_rect(win, surf, 1, t->border);

    gc_font_t *font = gc_get_font(win);
    gc_font_t *small = gc_get_font_small(win);

    gc_draw_text(win, small, "Switch Session  (Ctrl+Tab / 1-9 / Enter / Esc)",
                 ox + 14, oy + 8, t->text_secondary);

    for (int i = 0; i < max_slot; i++) {
        int ry = oy + 30 + i * row_h;
        app_session_entry_t *s = app_get_session(app, i);
        if (!s) continue;
        bool sel = (i == g_sw.selected);
        if (sel) {
            gc_rect_t row = {ox + 8, ry, hud_w - 16, row_h - 4};
            gc_fill_round_rect(win, row, 6, GC_RGBA(0, 83, 253, 60));
        }
        char label[320];
        if (s->title[0] && strcmp(s->title, s->id) != 0)
            snprintf(label, sizeof(label), "  %d  %s", i + 1, s->title);
        else
            snprintf(label, sizeof(label), "  %d  (untitled)  %s", i + 1, s->id);
        gc_draw_text(win, font, label, ox + 12, ry + 6,
                     sel ? t->text : t->text_secondary);
        if (s->model[0]) {
            gc_draw_text(win, small, s->model, ox + hud_w - 140, ry + 8, t->text_dim);
        }
    }
}

/* ── input ──────────────────────────────────────────────────────────── */

bool session_switcher_handle_key(app_state_t *app, int key, int mod) {
    if (!app || !g_sw.visible) return false;

    switch (key) {
        case SDLK_ESCAPE:
            g_sw.visible = false;
            return true;
        case SDLK_TAB:
            /* Ctrl+Shift+Tab cycles backward. */
            session_switcher_cycle(app, (mod & KMOD_SHIFT) ? -1 : 1);
            return true;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            session_switcher_select(app, g_sw.selected);
            return true;
        case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
        case SDLK_6: case SDLK_7: case SDLK_8: case SDLK_9:
            session_switcher_select(app, key - SDLK_1);
            return true;
        case SDLK_UP:
            session_switcher_cycle(app, -1);
            return true;
        case SDLK_DOWN:
            session_switcher_cycle(app, 1);
            return true;
        default:
            break;
    }
    return false;
}
