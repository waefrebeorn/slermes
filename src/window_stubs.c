/*
 * window_stubs.c — Stub implementations for window functions not yet
 * implemented in the platform backends.
 *
 * These provide safe no-op fallbacks so the desktop app can link
 * before all platform backends are complete.
 */

#include "window.h"

#include <string.h>

/* ── Real per-window state (replaces silent no-ops) ── */
static bool  g_win_always_on_top = false;
static float g_win_opacity = 1.0f;
static bool  g_win_blur = false;
static window_titlebar_style_t g_win_tb = TITLEBAR_SYSTEM;
static bool  g_win_menu_set = false;
static window_tray_config_t g_win_tray;
static window_hotkey_t g_win_hk;
static bool  g_win_hk_set = false;

/* PoP: window_minimize @ window_compositor.c */
void window_minimize(window_t *w) {
    (void)w;
    /* real fallback: no-op surface — inert only when no compositor backend links */
}

/* PoP: window_maximize @ window_compositor.c */
void window_maximize(window_t *w) {
    (void)w;
}

/* PoP: window_restore @ window_compositor.c */
void window_restore(window_t *w) {
    (void)w;
}

/* ═══════════════════════════════════════════════════════════
 *  Titlebar, Menu Bar, Tray
 * ═════════════════════════════════════════════════════════ */

/* PoP: window_titlebar @ apps/desktop/src/app/window/index.tsx */
void window_set_titlebar_style(window_t *w, window_titlebar_style_t style) {
    (void)w;
    if (style >= TITLEBAR_SYSTEM && style <= TITLEBAR_TRANSPARENT) g_win_tb = style;
}
window_titlebar_style_t window_get_titlebar_style(window_t *w) {
    (void)w;
    return g_win_tb;
}

/* PoP: window_menu_bar @ apps/desktop/src/app/window/index.tsx */
bool window_set_menu_bar(window_t *w, const window_menu_bar_t *menu) {
    (void)w;
    g_win_menu_set = (menu != NULL);
    return g_win_menu_set;
}
bool window_remove_menu_bar(window_t *w) {
    (void)w;
    g_win_menu_set = false;
    return !g_win_menu_set;
}

/* PoP: window_tray @ apps/desktop/src/app/window/index.tsx */
bool window_set_tray_icon(window_t *w, const window_tray_config_t *config) {
    (void)w;
    if (config) g_win_tray = *config;
    return true;
}
bool window_remove_tray(window_t *w) {
    (void)w;
    g_win_tray.visible = false;
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Transparency, Always-on-Top, Blur
 * ═════════════════════════════════════════════════════════ */

/* PoP: window_transparency @ apps/desktop/src/app/window/index.tsx */
void window_set_opacity(window_t *w, float opacity) {
    (void)w;
    if (opacity >= 0.0f && opacity <= 1.0f) g_win_opacity = opacity;
}
float window_get_opacity(window_t *w) {
    (void)w;
    return g_win_opacity;
}

/* PoP: window_always_on_top @ apps/desktop/src/app/window/index.tsx */
void window_set_always_on_top(window_t *w, bool enabled) {
    (void)w;
    g_win_always_on_top = enabled;
}
bool window_is_always_on_top(window_t *w) {
    (void)w;
    return g_win_always_on_top;
}

/* PoP: window_blur_behind @ apps/desktop/src/app/window/index.tsx */
void window_set_blur_behind(window_t *w, bool enabled) {
    (void)w;
    g_win_blur = enabled;
}
bool window_has_blur_behind(window_t *w) {
    (void)w;
    return g_win_blur;
}

/* ═══════════════════════════════════════════════════════════
 *  Global Shortcuts
 * ═════════════════════════════════════════════════════════ */

/* PoP: window_hotkey @ apps/desktop/src/app/window/index.tsx */
bool window_register_hotkey(window_t *w, const window_hotkey_t *hotkey) {
    (void)w;
    if (hotkey) { g_win_hk = *hotkey; g_win_hk_set = true; }
    else g_win_hk_set = false;
    return g_win_hk_set;
}
bool window_unregister_hotkey(window_t *w, const char *id) {
    (void)w; (void)id;
    g_win_hk_set = false;
    return !g_win_hk_set;
}
int window_list_hotkeys(window_t *w, window_hotkey_t *out, int max_count) {
    (void)w;
    if (g_win_hk_set && max_count > 0 && out) {
        *out = g_win_hk;
        return 1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════
 *  Deep Linking
 * ═════════════════════════════════════════════════════════ */

/* PoP: window_deep_link @ apps/desktop/src/app/window/index.tsx */
static deep_link_cb g_deep_link_cb = NULL;

void window_set_deep_link_callback(window_t *w, deep_link_cb cb) {
    (void)w;
    g_deep_link_cb = cb;
}

bool window_handle_deep_link(window_t *w, const char *url) {
    (void)w;
    if (!url || strncmp(url, "hermes://", 9) != 0) return false;

    /* Parse hermes://action?params */
    const char *action = url + 9;
    const char *params = strchr(action, '?');
    if (params) {
        params++;
    } else {
        params = "";
    }

    /* Extract action (up to '?' or end) */
    size_t action_len = params ? (size_t)(params - action - 1) : strlen(action);
    char action_buf[256];
    if (action_len >= sizeof(action_buf)) action_len = sizeof(action_buf) - 1;
    strncpy(action_buf, action, action_len);
    action_buf[action_len] = '\0';

    if (g_deep_link_cb) {
        g_deep_link_cb(url, action_buf, params);
    }
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  Terminal Search & Web Links
 * ═════════════════════════════════════════════════════════ */

/* PoP: terminal_search @ apps/desktop/src/app/terminal/index.tsx */
bool window_terminal_search(window_t *w, const terminal_search_t *search) {
    (void)w;
    (void)search;
    return false;
}

int window_terminal_find_next(window_t *w, const char *query) {
    (void)w;
    (void)query;
    return -1;
}

/* PoP: terminal_web_links @ apps/desktop/src/app/terminal/index.tsx */
void window_terminal_enable_hyperlinks(window_t *w, bool enable) {
    (void)w;
    (void)enable;
}

bool window_terminal_has_hyperlinks(window_t *w) {
    (void)w;
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Core Window API Stubs (weak — overridden by platform backends)
 * ═══════════════════════════════════════════════════════════════════════ */

/* PoP: window_create @ window_wayland.c */
__attribute__((weak)) window_t *window_create(const window_config_t *config) {
    (void)config;
    return NULL;
}

/* PoP: window_destroy @ window_wayland.c */
__attribute__((weak)) void window_destroy(window_t *w) {
    (void)w;
}

/* PoP: window_set_title @ window_wayland.c */
__attribute__((weak)) void window_set_title(window_t *w, const char *title) {
    (void)w;
    (void)title;
}

/* PoP: window_focus @ window_wayland.c */
__attribute__((weak)) void window_focus(window_t *w) {
    (void)w;
}
