/*
 * session_switcher.h — Ctrl+Tab session switcher HUD (v484 parity)
 *
 * Floating HUD listing the most recent sessions; Ctrl+Tab cycles the
 * selection, 1-9 jumps to a slot, Enter switches, Esc closes. Mirrors the
 * Electron/desktop session-switcher behavior documented in the desktop
 * parity spec.
 */

#ifndef SLERMES_SESSION_SWITCHER_H
#define SLERMES_SESSION_SWITCHER_H

#include <stdbool.h>

typedef struct app_state app_state_t;

/* Open/close the switcher HUD. */
void session_switcher_toggle(app_state_t *app);
void session_switcher_open(app_state_t *app);
void session_switcher_close(app_state_t *app);
bool session_switcher_visible(app_state_t *app);

/* Cycle selection forward/backward (Ctrl+Tab / Ctrl+Shift+Tab). */
void session_switcher_cycle(app_state_t *app, int dir);

/* Select slot index 0-8 (1-9 keys). */
void session_switcher_select(app_state_t *app, int slot);

/* Draw the HUD (call in the frame loop after the main panels). */
void session_switcher_draw(app_state_t *app);

/* Handle a key when the HUD is open. Returns true if consumed. */
bool session_switcher_handle_key(app_state_t *app, int key, int mod);

#endif /* SLERMES_SESSION_SWITCHER_H */
