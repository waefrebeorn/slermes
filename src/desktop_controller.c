/*
 * desktop_controller.c — Desktop Controller Implementation
 *
 * Handles boot sequence, gateway connection, and pane management.
 */

#define _GNU_SOURCE
#include "desktop_controller.h"
#include "app_state_internal.h"
#include "gui_core.h"
#include "hud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════
 * Internal State
 * ══════════════════════════════════════════════════════════════════════ */

static struct {
    boot_state_t boot;
    char status_message[256];
    char gateway_url[256];
    bool gateway_connecting;
    bool gateway_ready;
    int active_sessions;
    char active_profile[64];
} desktop_controller = {0};

/* ══════════════════════════════════════════════════════════════════════
 * Implementation
 * ══════════════════════════════════════════════════════════════════════ */

void desktop_controller_init(void) {
    memset(&desktop_controller, 0, sizeof(desktop_controller));
    desktop_controller.boot = BOOT_INIT;
    snprintf(desktop_controller.status_message, sizeof(desktop_controller.status_message), "Initializing...");
    desktop_controller.gateway_connecting = false;
    desktop_controller.gateway_ready = false;
    hud_init();
}

void desktop_controller_set_boot_state(boot_state_t state, const char *msg) {
    desktop_controller.boot = state;
    if (msg) snprintf(desktop_controller.status_message, sizeof(desktop_controller.status_message), "%s", msg);
    
    gc_theme_t *t = &gc_theme_dark; /* Will be updated from app */
    switch (state) {
        case BOOT_CONNECTING:
            hud_push("Gateway", "Connecting...", GC_HEX(0xf59e0b), 10);
            break;
        case BOOT_READY:
            hud_push("Gateway", "Connected", GC_HEX(0x22c55e), 10);
            break;
        case BOOT_ERROR:
            hud_push("Gateway", "Error", GC_HEX(0xef4444), 30);
            break;
        default: break;
    }
}

boot_state_t desktop_controller_get_boot_state(void) {
    return desktop_controller.boot;
}

void desktop_controller_draw_boot_overlay(app_state_t *app) {
    if (!app || !app_get_window(app)) return;
    
    gc_window_t *win = app_get_window(app);
    gc_theme_t *t = app_get_theme(app);
    
    if (desktop_controller.boot == BOOT_READY) return;
    if (desktop_controller.boot == BOOT_INIT) return;
    
    int w = gc_window_w(win), h = gc_window_h(win);
    
    /* Semi-transparent overlay */
    gc_rect_t bg = {0, 0, w, h};
    gc_fill_rect(win, bg, GC_RGBA(10, 10, 15, 245));
    
    /* Center panel */
    int pw = 360, ph = 120;
    int px = (w - pw) / 2, py = (h - ph) / 2;
    gc_rect_t panel = {px, py, pw, ph};
    gc_fill_round_rect(win, panel, 12, t->bg_secondary);
    gc_draw_rect(win, panel, 1, t->border);
    
    gc_font_t *font = gc_get_font(win);
    gc_font_t *font_small = gc_get_font_small(win);
    int fh = gc_font_height(font);
    
    /* Status icon */
    const char *icon;
    gc_color_t icon_color;
    switch (desktop_controller.boot) {
        case BOOT_CONNECTING: icon = "\xe2\x96\x90"; icon_color = GC_HEX(0xf59e0b); break;  // ◐
        case BOOT_ERROR: icon = "\xe2\x9c\x95"; icon_color = GC_HEX(0xef4444); break;      // ✕
        default: icon = "\xe2\x97\x8f"; icon_color = t->text_dim; break;                     // ●
    }
    gc_draw_text(win, font, icon, px + pw / 2 - 12, py + 20, icon_color);
    
    /* Status message */
    gc_draw_text(win, font_small, desktop_controller.status_message,
                 px + 20, py + 20 + fh + 8, t->text_secondary);
}

bool desktop_controller_is_ready(void) {
    return desktop_controller.boot == BOOT_READY;
}

void desktop_controller_set_gateway_url(const char *url) {
    if (url) snprintf(desktop_controller.gateway_url, sizeof(desktop_controller.gateway_url), "%s", url);
}

void desktop_controller_set_gateway_connected(bool connected) {
    desktop_controller.gateway_ready = connected;
    desktop_controller.gateway_connecting = !connected;
    if (connected) {
        desktop_controller_set_boot_state(BOOT_READY, "Gateway connected");
    } else {
        desktop_controller_set_boot_state(BOOT_CONNECTING, "Reconnecting...");
    }
}

void desktop_controller_set_active_profile(const char *profile) {
    if (profile) snprintf(desktop_controller.active_profile, sizeof(desktop_controller.active_profile), "%s", profile);
}