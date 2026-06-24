/*
 * window_compositor.h — Multi-window management for C11 desktop app
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

#ifndef WINDOW_COMPOSITOR_H
#define WINDOW_COMPOSITOR_H

#include "window.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define COMPOSITOR_MAX_WINDOWS 64
#define COMPOSITOR_MAX_TITLE  256

/* ── Window types ───────────────────────────────────────────────────────── */
typedef enum {
    WIN_TYPE_MAIN = 0,
    WIN_TYPE_SESSION,       /* pop-out session window */
    WIN_TYPE_DIALOG,
    WIN_TYPE_SETTINGS,
} win_type_t;

/* ── Window entry in compositor registry ────────────────────────────────── */
typedef struct {
    window_t   *window;             /* owned by compositor */
    win_type_t  type;
    char        session_id[64];     /* associated session, if any */
    char        title[COMPOSITOR_MAX_TITLE];
    int         z_order;            /* stacking order */
    bool        visible;
    bool        minimized;
    bool        maximized;
} compositor_win_entry_t;

/* ── Compositor handle ──────────────────────────────────────────────────── */
typedef struct {
    compositor_win_entry_t entries[COMPOSITOR_MAX_WINDOWS];
    int count;
    int active_idx;                 /* -1 if none */
} compositor_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: compositor_create @ electron/session-windows.cjs */
compositor_t *compositor_create(void);

void compositor_destroy(compositor_t *comp);

/* ── Window Management ───────────────────────────────────────────────────── */

/* PoP: compositor_open_window @ electron/session-windows.cjs */
/* Create and register a new window. */
int compositor_open_window(compositor_t *comp, win_type_t type,
                           const char *title, int w, int h,
                           const char *session_id);

/* PoP: compositor_close_window @ electron/session-windows.cjs */
/* Close and unregister a window by index. */
bool compositor_close_window(compositor_t *comp, int idx);

/* PoP: compositor_focus_window @ electron/session-windows.cjs */
/* Bring a window to front and make it active. */
bool compositor_focus_window(compositor_t *comp, int idx);

/* PoP: compositor_get_active @ electron/session-windows.cjs */
/* Get the currently active window entry (NULL if none). */
const compositor_win_entry_t *compositor_get_active(const compositor_t *comp);

/* PoP: compositor_list_windows @ electron/session-windows.cjs */
/* Get all window entries (returns count). */
int compositor_list_windows(const compositor_t *comp,
                            const compositor_win_entry_t **out,
                            int max_count);

/* Find window index by session ID. Returns -1 if not found. */
int compositor_find_by_session(const compositor_t *comp, const char *session_id);

/* Find window index by window_t pointer. Returns -1 if not found. */
int compositor_find_by_window(const compositor_t *comp, const window_t *w);

/* ── Window state ────────────────────────────────────────────────────────── */

bool compositor_set_minimized(compositor_t *comp, int idx, bool minimized);
bool compositor_set_maximized(compositor_t *comp, int idx, bool maximized);
bool compositor_set_title(compositor_t *comp, int idx, const char *title);
bool compositor_set_z_order(compositor_t *comp, int idx, int z);

/* ── Iteration ───────────────────────────────────────────────────────────── */
/* Call a callback for each window. Stops if callback returns false. */
void compositor_foreach(compositor_t *comp,
                        bool (*fn)(const compositor_win_entry_t *entry, void *ctx),
                        void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* WINDOW_COMPOSITOR_H */
