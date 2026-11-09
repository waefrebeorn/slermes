/*
 * tui_win.h - TUI window / render helpers.
 *
 * Module-internal header (tui_fullscreen.c <-> tui_win.c).
 */
#ifndef TUI_WIN_H
#define TUI_WIN_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "tui_state.h"   /* msg_role_t, display_line_t */

void handle_sigint(int sig);
void tui_create_windows(void);
void tui_fullscreen_print(const char *fmt, ...);
void tui_history_add(msg_role_t role, const char *text, bool bold);
void tui_redraw_history(void);
void tui_resize_panes(void);

#endif /* TUI_WIN_H */
