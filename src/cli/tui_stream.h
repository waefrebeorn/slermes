/*
 * tui_stream.h - TUI streaming-render helpers.
 *
 * Module-internal header (tui_fullscreen.c <-> tui_stream.c).
 */
#ifndef TUI_STREAM_H
#define TUI_STREAM_H

#include <stdbool.h>
#include "tui_state.h"
#include "tui_fullscreen.h"

void tui_fullscreen_stream_done(void);
void tui_fullscreen_stream_token(const char *token);
int tui_stream_cb(const char *token, void *userdata);

#endif /* TUI_STREAM_H */
