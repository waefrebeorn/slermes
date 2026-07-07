/*
 * hud.c — Floating HUD Implementation
 *
 * Transient status indicators displayed at top-right.
 */

#define _GNU_SOURCE
#include "hud.h"
#include "app_state_internal.h"
#include "gui_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ══════════════════════════════════════════════════════════════════════
 * Internal State
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct {
    char label[64];
    char value[128];
    gc_color_t color;
    time_t created_at;
    int  duration_sec;
} hud_item_t;

static struct {
    hud_item_t items[HUD_MAX_ITEMS];
    int count;
} floating_hud = {0};

/* ══════════════════════════════════════════════════════════════════════
 * Implementation
 * ══════════════════════════════════════════════════════════════════════ */

void hud_init(void) {
    memset(&floating_hud, 0, sizeof(floating_hud));
}

void hud_push(const char *label, const char *value, gc_color_t color, int duration_sec) {
    if (!label || !value) return;
    
    /* Shift existing items up if at capacity */
    if (floating_hud.count >= HUD_MAX_ITEMS) {
        memmove(&floating_hud.items[0], &floating_hud.items[1],
                (HUD_MAX_ITEMS - 1) * sizeof(hud_item_t));
        floating_hud.count = HUD_MAX_ITEMS - 1;
    }
    
    hud_item_t *item = &floating_hud.items[floating_hud.count++];
    snprintf(item->label, sizeof(item->label), "%s", label);
    snprintf(item->value, sizeof(item->value), "%s", value);
    item->color = color;
    item->created_at = time(NULL);
    item->duration_sec = duration_sec > 0 ? duration_sec : 5;
}

void hud_update(void) {
    time_t now = time(NULL);
    int write = 0;
    for (int i = 0; i < floating_hud.count; i++) {
        if (now - floating_hud.items[i].created_at < floating_hud.items[i].duration_sec) {
            floating_hud.items[write++] = floating_hud.items[i];
        }
    }
    floating_hud.count = write;
}

void hud_draw(app_state_t *app) {
    if (!app || !app_get_window(app)) return;
    
    hud_update();
    
    if (floating_hud.count == 0) return;
    
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);
    int w = gc_window_w(win);
    
    int item_w = 140;
    int item_h = 22;
    int padding = 8;
    int total_w = item_w + padding * 2;
    int total_h = floating_hud.count * (item_h + 4) + padding * 2;
    int ox = w - total_w - 12;
    int oy = TITLEBAR_H + 8;
    
    gc_rect_t bg = {ox, oy, total_w, total_h};
    gc_fill_rect(win, bg, GC_RGBA(0, 0, 0, 200));
    gc_fill_round_rect(win, bg, 8, t->bg_secondary);
    gc_draw_rect(win, bg, 1, t->border_subtle);
    
    gc_font_t *font_small = gc_get_font_small(win);
    int sfh = gc_font_height(font_small);
    
    for (int i = 0; i < floating_hud.count; i++) {
        int iy = oy + padding + i * (item_h + 4);
        gc_draw_text(win, font_small, floating_hud.items[i].label,
                     ox + padding, iy + 2, t->text_dim);
        gc_draw_text(win, font_small, floating_hud.items[i].value,
                     ox + padding, iy + sfh + 4, floating_hud.items[i].color);
    }
}