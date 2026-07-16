/*
 * terminal.h — VT100/xterm terminal emulation
 *
 * Minimal VT100/xterm terminal emulator for the C11 desktop app.
 * Parses escape sequences, maintains a screen buffer, and provides
 * scrollback. Designed to be rendered via the UI layer.
 *
 * Replaces @xterm/xterm TypeScript terminal.
 *
 * PoP: terminal_create       @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_write        @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_resize       @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_dispose      @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_get_buffer   @ apps/desktop/src/app/right-sidebar/terminal/buffer.ts
 * PoP: terminal_clear        @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 */

#ifndef TERMINAL_H
#define TERMINAL_H

#include "slermes_pty.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define TERM_MAX_COLS   320
#define TERM_MAX_ROWS   128
#define TERM_MAX_LINES  (TERM_MAX_ROWS * 3)  /* visible + scrollback */
#define TERM_TAB_WIDTH  8
#define TERM_MAX_PARAMETERS 16

/* ── Color / Attribute definitions ─────────────────────────────────────── */
typedef enum {
    TERM_DEFAULT_FG  = 39,
    TERM_DEFAULT_BG  = 49,
} term_default_color_t;

typedef enum {
    TERM_ATTR_BOLD      = (1 << 0),
    TERM_ATTR_DIM       = (1 << 1),
    TERM_ATTR_ITALIC    = (1 << 2),
    TERM_ATTR_UNDERLINE = (1 << 3),
    TERM_ATTR_BLINK     = (1 << 4),
    TERM_ATTR_REVERSE   = (1 << 5),
    TERM_ATTR_HIDDEN    = (1 << 6),
    TERM_ATTR_STRIKE    = (1 << 7),
} term_attr_t;

/* A single screen cell */
typedef struct {
    uint32_t ch;           /* Unicode codepoint */
    uint16_t fg;           /* foreground color (ANSI 0-255 or true-color) */
    uint16_t bg;           /* background color */
    uint8_t  attrs;        /* term_attr_t bitmask */
    bool     dirty;        /* needs redraw */
} term_cell_t;

/* ── Scrollback buffer ─────────────────────────────────────────────────── */
typedef struct {
    term_cell_t **lines;   /* array of row pointers */
    int          cols;     /* columns per row */
    int          count;    /* number of rows in buffer */
    int          capacity; /* max rows */
} term_scrollback_t;

/* ── Cursor state ──────────────────────────────────────────────────────── */
typedef struct {
    int  row, col;
    bool visible;
    bool wrapnext;         /* wrap pending (DECAWM) */
    uint16_t fg_save, bg_save;
    uint8_t  attrs_save;
} term_cursor_t;

/* ── Parser state machine ──────────────────────────────────────────────── */
typedef enum {
    TERM_STATE_NORMAL = 0,
    TERM_STATE_ESC,        /* after ESC */
    TERM_STATE_CSI,        /* after CSI (ESC [) */
    TERM_STATE_OSC,        /* after OSC (ESC ]) */
    TERM_STATE_DCS,        /* after DCS (ESC P) */
} term_parser_state_t;

/* ── Terminal handle ──────────────────────────────────────────────────── */
typedef struct {
    /* Screen buffer */
    term_cell_t    screen[TERM_MAX_ROWS][TERM_MAX_COLS];
    int            rows;           /* visible rows */
    int            cols;           /* visible columns */
    int            scroll_top;     /* DECSTBM top */
    int            scroll_bottom;  /* DECSTBM bottom */

    /* Scrollback */
    term_scrollback_t scrollback;

    /* Cursor */
    term_cursor_t  cursor;
    term_cursor_t  saved_cursor;   /* for DECSC/DECRC */

    /* Default colors */
    uint16_t       default_fg;
    uint16_t       default_bg;
    uint8_t        default_attrs;

    /* Parser state */
    term_parser_state_t state;
    int            params[TERM_MAX_PARAMETERS];
    int            param_count;
    int            param_accum;
    bool           param_qmark;    /* CSI ? prefix */
    char           osc_buf[1024];
    int            osc_len;

    /* PTY integration */
    pty_t         *pty;

    /* Tab stops */
    bool           tab_stops[TERM_MAX_COLS];

    /* DEC private modes */
    bool           decawm;    /* auto-wrap */
    bool           decim;     /* insert mode */
    bool           decckm;    /* cursor keys */
    bool           decrccm;   /* reverse cursor */

    /* Reflow tracking */
    bool           need_full_redraw;
} terminal_t;

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

/* PoP: terminal_create @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
/* Create a new terminal with given dimensions. */
terminal_t *terminal_create(int cols, int rows);

/* PoP: terminal_dispose @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
/* Free terminal resources. */
void terminal_dispose(terminal_t *term);

/* PTY attachment */
void terminal_attach_pty(terminal_t *term, pty_t *pty);

/* ── I/O ────────────────────────────────────────────────────────────────── */

/* PoP: terminal_write @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
/* Process input from user (keystrokes) — writes to attached PTY. */
int terminal_input(terminal_t *term, const char *data, size_t len);

/* PoP: terminal_resize @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
/* Resize the terminal, also resizes the attached PTY. */
bool terminal_resize(terminal_t *term, int cols, int rows);

/* Process output from PTY — parses VT100 escape sequences into screen buffer.
 * Call this after pty_read() to feed PTY output into the terminal. */
int terminal_process_output(terminal_t *term, const char *data, size_t len);

/* PoP: terminal_clear @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
/* Clear the terminal screen and scrollback. */
void terminal_clear(terminal_t *term);

/* ── Screen Access ──────────────────────────────────────────────────────── */

/* PoP: terminal_get_buffer @ apps/desktop/src/app/right-sidebar/terminal/buffer.ts */
/* Get a pointer to the screen buffer for rendering. */
const term_cell_t *terminal_get_screen(const terminal_t *term, int *out_rows, int *out_cols);

/* Get scrollback line. Returns NULL if index out of bounds. */
const term_cell_t *terminal_get_scrollback_line(const terminal_t *term, int index);

/* Get scrollback line count. */
int terminal_scrollback_count(const terminal_t *term);

/* Get cursor position. */
void terminal_get_cursor(const terminal_t *term, int *row, int *col, bool *visible);

/* ── Selection ──────────────────────────────────────────────────────────── */

typedef struct {
    int start_row, start_col;
    int end_row, end_col;
    bool active;
} term_selection_t;

void terminal_selection_set(terminal_t *term, int sr, int sc, int er, int ec);
void terminal_selection_clear(terminal_t *term);
const term_selection_t *terminal_selection_get(const terminal_t *term);

/* Get selected text (caller must free). Returns NULL if no selection. */
char *terminal_selection_text(terminal_t *term);

/* ── Helper: determine if cells have same attributes ─────────────────────── */
static inline bool term_cell_eq_attrs(const term_cell_t *a, const term_cell_t *b) {
    return a->fg == b->fg && a->bg == b->bg && a->attrs == b->attrs;
}

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_H */
