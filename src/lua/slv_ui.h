/*
 * slv_ui.h -- a Lua-driven terminal UI host built on slerm-Lua.
 *
 * The direction from the WWW/Wua work: curses (and SDL, and C#) are replaced
 * by Lua as the UI substrate. This module is the seed of that - it gives a
 * Lua chunk a `term` table so a script can draw, instead of C linking against
 * ncurses (which cannot build on Windows at all: no ncurses import libs and
 * no mkfifo in the MinGW headers).
 *
 * The screen is an in-memory cell buffer owned by the host, NOT by Lua. Lua
 * can only touch it through the `term` table, so a script cannot corrupt the
 * host's state and the host can render or assert on the buffer afterwards.
 *
 * Lua surface:
 *   term.width, term.height         -> integers
 *   term.clear()                    wipe the buffer
 *   term.put(x, y, s)               write a string at (x, y), clipped
 *   term.get(x, y)                  -> the character at (x, y)
 *   term.fill(ch)                   fill the whole buffer with one character
 *   term.refresh()                  no-op here; the host renders on demand
 *
 * C11 only. Depends on src/lv/slv.h and nothing else.
 */
#ifndef SLV_UI_H
#define SLV_UI_H

#include <stddef.h>
#include <stdio.h>

/* slv.h lives in src/lv/, a sibling of this header's src/lua/. The relative
 * include keeps this module self-contained without adding -I src/lv to the
 * build. */
#include "../lv/slv.h"

typedef struct slv_ui slv_ui;

/* Create a UI state with a w x h character buffer and a fresh Lua state that
 * has the standard libraries open and `term` registered. NULL on OOM. */
slv_ui *slv_ui_new(int w, int h);

void slv_ui_free(slv_ui *ui);

/* Run a Lua chunk. Returns NULL on success, or a static error string. */
const char *slv_ui_run(slv_ui *ui, const char *src, const char *chunkname);

/* Buffer accessors (for embedding and tests). */
int         slv_ui_width(const slv_ui *ui);
int         slv_ui_height(const slv_ui *ui);
char        slv_ui_cell(const slv_ui *ui, int x, int y);
/* The row starting at y: exactly slv_ui_width() chars, NOT NUL-terminated.
 * Use slv_ui_cell() for indexed access. */
const char *slv_ui_line(const slv_ui *ui, int y);

/* Render the buffer to a stream, one line at a time, no trailing spaces. */
void slv_ui_dump(const slv_ui *ui, FILE *out);

/* The underlying Lua state, for hosts that want to register more globals. */
slv *slv_ui_state(slv_ui *ui);

#endif /* SLV_UI_H */
