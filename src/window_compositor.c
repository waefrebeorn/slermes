/*
 * window_compositor.c — Multi-window management for C11 desktop app
 *
 * Manages a registry of windows (session windows, pop-outs, dialogs).
 * Provides open/focus/close/reorder operations.
 *
 * PoP: compositor_create         @ electron/session-windows.cjs
 * PoP: compositor_open_window   @ electron/session-windows.cjs
 * PoP: compositor_close_window  @ electron/session-windows.cjs
 * PoP: compositor_focus_window  @ electron/session-windows.cjs
 * PoP: compositor_list_windows  @ electron/session-windows.cjs
 * PoP: compositor_get_active    @ electron/session-windows.cjs
 */

#include "window_compositor.h"
#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: compositor_create @ electron/session-windows.cjs */
compositor_t *compositor_create(void) {
    compositor_t *comp = calloc(1, sizeof(compositor_t));
    if (!comp) {
        fprintf(stderr, "compositor_create: calloc failed");
        return NULL;
    }

    comp->count = 0;
    comp->active_idx = -1;

    fprintf(stderr, "compositor_create: allocated %d slots", COMPOSITOR_MAX_WINDOWS);
    return comp;
}

void compositor_destroy(compositor_t *comp) {
    if (!comp) return;

    /* Close all windows */
    for (int i = comp->count - 1; i >= 0; i--) {
        if (comp->entries[i].window) {
            window_destroy(comp->entries[i].window);
            comp->entries[i].window = NULL;
        }
    }

    free(comp);
}

/* PoP: compositor_open_window @ electron/session-windows.cjs */
int compositor_open_window(compositor_t *comp, win_type_t type,
                           const char *title, int w, int h,
                           const char *session_id) {
    if (!comp) return -1;

    if (comp->count >= COMPOSITOR_MAX_WINDOWS) {
        fprintf(stderr, "compositor_open_window: max windows (%d) reached", COMPOSITOR_MAX_WINDOWS);
        return -1;
    }

    char win_title[COMPOSITOR_MAX_TITLE];
    if (title && *title) {
        strncpy(win_title, title, COMPOSITOR_MAX_TITLE - 1);
    } else {
        switch (type) {
            case WIN_TYPE_MAIN:     snprintf(win_title, sizeof(win_title), "Slermes Agent"); break;
            case WIN_TYPE_SESSION:  snprintf(win_title, sizeof(win_title), "Session"); break;
            case WIN_TYPE_DIALOG:   snprintf(win_title, sizeof(win_title), "Dialog"); break;
            case WIN_TYPE_SETTINGS: snprintf(win_title, sizeof(win_title), "Settings"); break;
            default:                snprintf(win_title, sizeof(win_title), "Window"); break;
        }
    }
    win_title[COMPOSITOR_MAX_TITLE - 1] = '\0';

    window_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.title = win_title;
    cfg.width = w > 0 ? w : 1280;
    cfg.height = h > 0 ? h : 900;
    cfg.resizable = true;
    cfg.centered = true;

    window_t *win = window_create(&cfg);
    if (!win) {
        fprintf(stderr, "compositor_open_window: window_create failed for '%s'", win_title);
        return -1;
    }

    int idx = comp->count;
    compositor_win_entry_t *entry = &comp->entries[idx];
    memset(entry, 0, sizeof(*entry));
    entry->window      = win;
    entry->type        = type;
    entry->z_order     = idx;
    entry->visible     = true;
    entry->minimized   = false;
    entry->maximized   = false;
    strncpy(entry->title, win_title, COMPOSITOR_MAX_TITLE - 1);
    if (session_id)
        strncpy(entry->session_id, session_id, sizeof(entry->session_id) - 1);

    comp->count++;
    comp->active_idx = idx;

    fprintf(stderr, "compositor_open_window: '%s' type=%d idx=%d", win_title, type, idx);
    return idx;
}

/* PoP: compositor_close_window @ electron/session-windows.cjs */
bool compositor_close_window(compositor_t *comp, int idx) {
    if (!comp || idx < 0 || idx >= comp->count) return false;

    if (comp->entries[idx].window) {
        window_destroy(comp->entries[idx].window);
    }

    /* Shift remaining entries down */
    for (int i = idx; i < comp->count - 1; i++) {
        comp->entries[i] = comp->entries[i + 1];
    }
    comp->count--;

    /* Update active index */
    if (comp->active_idx == idx) {
        comp->active_idx = (comp->count > 0) ? 0 : -1;
    } else if (comp->active_idx > idx) {
        comp->active_idx--;
    }

    fprintf(stderr, "compositor_close_window: idx=%d remaining=%d", idx, comp->count);
    return true;
}

/* PoP: compositor_focus_window @ electron/session-windows.cjs */
bool compositor_focus_window(compositor_t *comp, int idx) {
    if (!comp || idx < 0 || idx >= comp->count) return false;

    compositor_win_entry_t *entry = &comp->entries[idx];
    if (entry->window && !entry->minimized) {
        window_focus(entry->window);
    }

    entry->visible = true;
    entry->z_order = comp->count; /* raise to top */

    /* Decrement z-order of others slightly */
    for (int i = 0; i < comp->count; i++) {
        if (i != idx && comp->entries[i].z_order > 0)
            comp->entries[i].z_order--;
    }

    comp->active_idx = idx;
    return true;
}

/* PoP: compositor_get_active @ electron/session-windows.cjs */
const compositor_win_entry_t *compositor_get_active(const compositor_t *comp) {
    if (!comp || comp->active_idx < 0 || comp->active_idx >= comp->count)
        return NULL;
    return &comp->entries[comp->active_idx];
}

/* PoP: compositor_list_windows @ electron/session-windows.cjs */
int compositor_list_windows(const compositor_t *comp,
                            const compositor_win_entry_t **out,
                            int max_count) {
    if (!comp || !out || max_count <= 0) return 0;

    int count = comp->count < max_count ? comp->count : max_count;
    *out = comp->entries;
    return count;
}

int compositor_find_by_session(const compositor_t *comp, const char *session_id) {
    if (!comp || !session_id) return -1;
    for (int i = 0; i < comp->count; i++) {
        if (strcmp(comp->entries[i].session_id, session_id) == 0)
            return i;
    }
    return -1;
}

int compositor_find_by_window(const compositor_t *comp, const window_t *w) {
    if (!comp || !w) return -1;
    for (int i = 0; i < comp->count; i++) {
        if (comp->entries[i].window == w)
            return i;
    }
    return -1;
}

bool compositor_set_minimized(compositor_t *comp, int idx, bool minimized) {
    if (!comp || idx < 0 || idx >= comp->count) return false;
    comp->entries[idx].minimized = minimized;
    if (comp->entries[idx].window) {
        if (minimized) window_minimize(comp->entries[idx].window);
        else window_restore(comp->entries[idx].window);
    }
    return true;
}

bool compositor_set_maximized(compositor_t *comp, int idx, bool maximized) {
    if (!comp || idx < 0 || idx >= comp->count) return false;
    comp->entries[idx].maximized = maximized;
    if (comp->entries[idx].window) {
        if (maximized) window_maximize(comp->entries[idx].window);
        else window_restore(comp->entries[idx].window);
    }
    return true;
}

bool compositor_set_title(compositor_t *comp, int idx, const char *title) {
    if (!comp || idx < 0 || idx >= comp->count || !title) return false;
    strncpy(comp->entries[idx].title, title, COMPOSITOR_MAX_TITLE - 1);
    if (comp->entries[idx].window)
        window_set_title(comp->entries[idx].window, title);
    return true;
}

bool compositor_set_z_order(compositor_t *comp, int idx, int z) {
    if (!comp || idx < 0 || idx >= comp->count) return false;
    comp->entries[idx].z_order = z;
    return true;
}

void compositor_foreach(compositor_t *comp,
                        bool (*fn)(const compositor_win_entry_t *entry, void *ctx),
                        void *ctx) {
    if (!comp || !fn) return;
    for (int i = 0; i < comp->count; i++) {
        if (!fn(&comp->entries[i], ctx))
            break;
    }
}
