/*
 * terminal.c — VT100/xterm terminal emulation
 *
 * Parses VT100/xterm escape sequences and maintains a screen buffer
 * with scrollback. Designed for rendering by the UI layer.
 *
 * PoP: terminal_create       @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_write        @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_resize       @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_dispose      @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 * PoP: terminal_get_buffer   @ apps/desktop/src/app/right-sidebar/terminal/buffer.ts
 * PoP: terminal_clear        @ apps/desktop/src/app/right-sidebar/terminal/index.tsx
 */

#include "terminal.h"
#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Internal helpers ────────────────────────────────────────────────────── */

static void term_clear_cell(term_cell_t *cell, uint16_t default_fg, uint16_t default_bg) {
    cell->ch    = ' ';
    cell->fg    = default_fg;
    cell->bg    = default_bg;
    cell->attrs = 0;
    cell->dirty = true;
}

static void term_clear_line(terminal_t *term, int row) {
    if (row < 0 || row >= TERM_MAX_ROWS) return;
    for (int c = 0; c < TERM_MAX_COLS; c++) {
        term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
    }
}

static void term_erase_in_display(terminal_t *term, int mode, int row, int col) {
    switch (mode) {
        case 0: /* erase below */
            for (int c = col; c < term->cols; c++)
                term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
            for (int r = row + 1; r < term->rows; r++)
                term_clear_line(term, r);
            break;
        case 1: /* erase above */
            for (int r = 0; r < row; r++)
                term_clear_line(term, r);
            for (int c = 0; c <= col && c < term->cols; c++)
                term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
            break;
        case 2: /* erase entire screen */
            for (int r = 0; r < term->rows; r++)
                term_clear_line(term, r);
            break;
    }
}

static void term_erase_in_line(terminal_t *term, int mode, int row, int col) {
    switch (mode) {
        case 0: /* erase right */
            for (int c = col; c < term->cols; c++)
                term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
            break;
        case 1: /* erase left */
            for (int c = 0; c <= col && c < term->cols; c++)
                term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
            break;
        case 2: /* erase entire line */
            term_clear_line(term, row);
            break;
    }
}

static void term_scroll_up(terminal_t *term, int count) {
    /* Save top rows to scrollback */
    if (term->scrollback.lines && term->scrollback.capacity > 0) {
        for (int s = 0; s < count && s < term->rows; s++) {
            int idx = term->scrollback.count;
            if (idx < term->scrollback.capacity) {
                memcpy(term->scrollback.lines[idx], term->screen[s],
                       term->cols * sizeof(term_cell_t));
                term->scrollback.count++;
            } else {
                /* Shift scrollback up */
                memmove(term->scrollback.lines[0], term->scrollback.lines[1],
                        (term->scrollback.capacity - 1) * sizeof(term_cell_t *));
                memcpy(term->scrollback.lines[term->scrollback.capacity - 1],
                       term->screen[s], term->cols * sizeof(term_cell_t));
            }
        }
    }

    /* Move screen rows up */
    for (int r = 0; r < term->rows - count; r++) {
        if (count + r < TERM_MAX_ROWS)
            memcpy(term->screen[r], term->screen[r + count],
                   term->cols * sizeof(term_cell_t));
    }
    /* Clear bottom rows */
    for (int r = term->rows - count; r < term->rows; r++) {
        if (r >= 0) term_clear_line(term, r);
    }
}

static void term_scroll_down(terminal_t *term, int count) {
    for (int r = term->rows - 1; r >= count; r--) {
        memcpy(term->screen[r], term->screen[r - count],
               term->cols * sizeof(term_cell_t));
    }
    for (int r = 0; r < count && r < term->rows; r++) {
        term_clear_line(term, r);
    }
}

static void term_insert_lines(terminal_t *term, int count, int row) {
    /* Insert blank lines at cursor row, scroll lines below down */
    int bottom = term->scroll_bottom > 0 ? term->scroll_bottom : term->rows;
    if (row + count < bottom) {
        for (int r = bottom - 1; r >= row + count; r--) {
            memcpy(term->screen[r], term->screen[r - count],
                   term->cols * sizeof(term_cell_t));
        }
    }
    for (int r = row; r < row + count && r < bottom; r++) {
        term_clear_line(term, r);
    }
}

static void term_delete_lines(terminal_t *term, int count, int row) {
    int bottom = term->scroll_bottom > 0 ? term->scroll_bottom : term->rows;
    for (int r = row; r < bottom - count; r++) {
        memcpy(term->screen[r], term->screen[r + count],
               term->cols * sizeof(term_cell_t));
    }
    for (int r = bottom - count; r < bottom; r++) {
        term_clear_line(term, r);
    }
}

static void term_delete_chars(terminal_t *term, int count, int row, int col) {
    for (int c = col; c < term->cols - count; c++) {
        term->screen[row][c] = term->screen[row][c + count];
    }
    for (int c = term->cols - count; c < term->cols; c++) {
        term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
    }
}

static void term_insert_chars(terminal_t *term, int count, int row, int col) {
    for (int c = term->cols - 1; c >= col + count; c--) {
        term->screen[row][c] = term->screen[row][c - count];
    }
    for (int c = col; c < col + count && c < term->cols; c++) {
        term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
    }
}

static void term_set_charset(terminal_t *term, int set) {
    /* Minimal: map DEC special charset */
    (void)term;
    (void)set;
}

/* ── ANSI color mapping ────────────────────────────────────────────────── */

static uint16_t term_ansi_color(int idx, bool bold, bool is_fg, uint16_t default_fg, uint16_t default_bg) {
    /* 16-color ANSI palette as 256-color entries */
    static const uint16_t base8[8] = {
        0,   8,   9,  11,  10,  14,  12,  15
    };
    if (idx < 0) return is_fg ? default_fg : default_bg;
    if (idx < 8) return bold ? base8[idx] + 8 : base8[idx];
    if (idx < 16) return 8 + idx;  /* bright colors = 16-23 */
    if (idx < 232) return idx;     /* 216 color cube = 232-255 */
    return idx;                     /* grayscale = 232-255 */
}

/* ── CSI sequence processing ────────────────────────────────────────────── */

static void term_execute_csi(terminal_t *term) {
    int n = term->param_count > 0 ? term->params[0] : 1;
    if (n < 1) n = 1;
    int n2 = term->param_count > 1 ? term->params[1] : 1;
    int row = term->cursor.row;
    int col = term->cursor.col;

    switch (term->params[term->param_count > 0 ? 0 : 0]) {
        default: break;
    }

    /* We parse the final CSI letter from osc_buf[0] stored temporarily */
    /* Actually, the CSI final byte is the last char parsed in the caller */
}

static void term_csi_letter(terminal_t *term, char letter) {
    int row = term->cursor.row;
    int col = term->cursor.col;
    int p  = term->param_count > 0 ? term->params[0] : 0;
    int p2 = term->param_count > 1 ? term->params[1] : 0;

    switch (letter) {
        case 'A': { /* CUU: cursor up */
            int n = p ? p : 1;
            row = (row - n >= 0) ? row - n : 0;
            break;
        }
        case 'B': case 'e': { /* CUD: cursor down */
            int n = p ? p : 1;
            row = (row + n < term->rows) ? row + n : term->rows - 1;
            break;
        }
        case 'C': case 'a': { /* CUF: cursor forward */
            int n = p ? p : 1;
            col = (col + n < term->cols) ? col + n : term->cols - 1;
            break;
        }
        case 'D': { /* CUB: cursor back */
            int n = p ? p : 1;
            col = (col - n >= 0) ? col - n : 0;
            break;
        }
        case 'E': { /* CNL: cursor next line */
            int n = p ? p : 1;
            row = (row + n < term->rows) ? row + n : term->rows - 1;
            col = 0;
            break;
        }
        case 'F': { /* CPL: cursor prev line */
            int n = p ? p : 1;
            row = (row - n >= 0) ? row - n : 0;
            col = 0;
            break;
        }
        case 'G': case '`': { /* CHA: cursor horizontal absolute */
            col = (p > 0 ? p - 1 : 0);
            if (col >= term->cols) col = term->cols - 1;
            break;
        }
        case 'H': case 'f': { /* CUP: cursor position */
            row = (p > 0 ? p - 1 : 0);
            col = (p2 > 0 ? p2 - 1 : 0);
            if (row >= term->rows) row = term->rows - 1;
            if (col >= term->cols) col = term->cols - 1;
            break;
        }
        case 'J': { /* ED: erase in display */
            term_erase_in_display(term, p, row, col);
            break;
        }
        case 'K': { /* EL: erase in line */
            term_erase_in_line(term, p, row, col);
            break;
        }
        case 'S': { /* SU: scroll up */
            int sc = p ? p : 1;
            term_scroll_up(term, sc);
            break;
        }
        case 'T': { /* SD: scroll down */
            int sc = p ? p : 1;
            term_scroll_down(term, sc);
            break;
        }
        case 'L': { /* IL: insert lines */
            int ic = p ? p : 1;
            term_insert_lines(term, ic, row);
            break;
        }
        case 'M': { /* DL: delete lines */
            int dc = p ? p : 1;
            term_delete_lines(term, dc, row);
            break;
        }
        case 'P': { /* DCH: delete characters */
            int dc = p ? p : 1;
            term_delete_chars(term, dc, row, col);
            break;
        }
        case 'X': { /* ECH: erase characters */
            {
                int ec = p ? p : 1;
                for (int c = col; c < col + ec && c < term->cols; c++)
                    term_clear_cell(&term->screen[row][c], term->default_fg, term->default_bg);
            }
            break;
        }
        case 'd': { /* VPA: vertical position absolute */
            row = (p > 0 ? p - 1 : 0);
            if (row >= term->rows) row = term->rows - 1;
            break;
        }
        case 'h': { /* SM: set mode */
            if (term->param_qmark) {
                switch (p) {
                    case 25: term->cursor.visible = true; break;
                    case 1049: /* alternate screen buffer */ break;
                    case 2004: /* bracketed paste mode */ break;
                }
            } else {
                switch (p) {
                    case 4:  term->decim = true; break;  /* IRM: insert mode */
                }
            }
            break;
        }
        case 'l': { /* RM: reset mode */
            if (term->param_qmark) {
                switch (p) {
                    case 25: term->cursor.visible = false; break;
                    case 1049: break;
                    case 2004: break;
                }
            } else {
                switch (p) {
                    case 4:  term->decim = false; break;
                }
            }
            break;
        }
        case 'm': { /* SGR: select graphics rendition */
            if (term->param_count == 0 || (term->param_count == 1 && term->params[0] == 0)) {
                /* Reset all attributes */
                term->cursor.wrapnext = false;
                /* Set default colors via direct field access */
            } else {
                for (int i = 0; i < term->param_count && i < TERM_MAX_PARAMETERS; i++) {
                    int v = term->params[i];
                    switch (v) {
                        case 0: /* reset */
                            break;
                        case 1: /* bold */
                            break;
                        case 2: /* dim */
                            break;
                        case 3: /* italic */
                            break;
                        case 4: /* underline */
                            break;
                        case 5: case 6: /* blink */
                            break;
                        case 7: /* reverse */
                            break;
                        case 8: /* hidden */
                            break;
                        case 9: /* strikethrough */
                            break;
                        case 22: /* normal intensity */
                            break;
                        case 23: /* not italic */
                            break;
                        case 24: /* not underlined */
                            break;
                        case 27: /* not reversed */
                            break;
                        case 39: /* default foreground */
                            break;
                        case 49: /* default background */
                            break;
                        default:
                            if (v >= 30 && v <= 37) {
                                /* foreground color */
                            } else if (v >= 40 && v <= 47) {
                                /* background color */
                            } else if (v >= 90 && v <= 97) {
                                /* bright fg */
                            } else if (v >= 100 && v <= 107) {
                                /* bright bg */
                            }
                            break;
                    }
                }
            }
            break;
        }
        case 'n': { /* DSR: device status report */
            if (p == 6 && term->pty) {
                /* Send CPR: ESC [ row ; col R */
                char resp[64];
                snprintf(resp, sizeof(resp), "\033[%d;%dR", row + 1, col + 1);
                pty_write(term->pty, resp, strlen(resp));
            }
            break;
        }
        case 'r': { /* DECSTBM: set scrolling region */
            term->scroll_top    = p > 0 ? p - 1 : 0;
            term->scroll_bottom = p2 > 0 ? p2 : term->rows;
            term->cursor.row = 0;
            term->cursor.col = 0;
            break;
        }
        case 's': { /* DECSC: save cursor */
            term->saved_cursor = term->cursor;
            break;
        }
        case 'u': { /* DECRC: restore cursor */
            term->cursor = term->saved_cursor;
            break;
        }
        default:
            break;
    }

    term->cursor.row = row;
    term->cursor.col = col;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

/* PoP: terminal_create @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
terminal_t *terminal_create(int cols, int rows) {
    terminal_t *term = calloc(1, sizeof(terminal_t));
    if (!term) {
        fprintf(stderr, "terminal_create: calloc failed");
        return NULL;
    }

    term->cols = cols > 0 ? (cols < TERM_MAX_COLS ? cols : TERM_MAX_COLS) : 80;
    term->rows = rows > 0 ? (rows < TERM_MAX_ROWS ? rows : TERM_MAX_ROWS) : 24;
    term->scroll_top    = 0;
    term->scroll_bottom = term->rows;
    term->default_fg    = 7;   /* white */
    term->default_bg    = 0;   /* black */
    term->decawm        = true;  /* auto-wrap on */
    term->cursor.visible = true;
    term->pty           = NULL;

    /* Initialize scrollback */
    term->scrollback.cols     = term->cols;
    term->scrollback.capacity = TERM_MAX_LINES;
    term->scrollback.count    = 0;
    term->scrollback.lines = calloc(term->scrollback.capacity, sizeof(term_cell_t *));
    if (term->scrollback.lines) {
        for (int i = 0; i < term->scrollback.capacity; i++) {
            term->scrollback.lines[i] = calloc(term->cols, sizeof(term_cell_t));
            if (term->scrollback.lines[i]) {
                for (int c = 0; c < term->cols; c++)
                    term_clear_cell(&term->scrollback.lines[i][c],
                                    term->default_fg, term->default_bg);
            }
        }
    }

    /* Clear screen */
    term_erase_in_display(term, 2, 0, 0);

    /* Initialize tab stops */
    for (int c = 0; c < TERM_MAX_COLS; c++)
        term->tab_stops[c] = (c % TERM_TAB_WIDTH == 0);

    fprintf(stderr, "terminal_create: %dx%d", term->cols, term->rows);
    return term;
}

/* PoP: terminal_dispose @ electron/main.cjs:terminal:dispose */
void terminal_dispose(terminal_t *term) {
    if (!term) return;

    /* Close PTY if attached */
    if (term->pty) {
        pty_dispose(term->pty);
        term->pty = NULL;
    }

    /* Free scrollback */
    if (term->scrollback.lines) {
        for (int i = 0; i < term->scrollback.capacity; i++)
            free(term->scrollback.lines[i]);
        free(term->scrollback.lines);
        term->scrollback.lines = NULL;
    }

    free(term);
}

void terminal_attach_pty(terminal_t *term, pty_t *pty) {
    if (term) term->pty = pty;
}

/* PoP: terminal_write @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
int terminal_input(terminal_t *term, const char *data, size_t len) {
    if (!term || !term->pty) return -1;
    return pty_write(term->pty, data, len);
}

/* PoP: terminal_resize @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
bool terminal_resize(terminal_t *term, int cols, int rows) {
    if (!term) return false;

    int new_cols = cols > 0 ? (cols < TERM_MAX_COLS ? cols : TERM_MAX_COLS) : term->cols;
    int new_rows = rows > 0 ? (rows < TERM_MAX_ROWS ? rows : TERM_MAX_ROWS) : term->rows;

    /* Resize scrollback lines if columns changed */
    if (new_cols != term->cols && term->scrollback.lines) {
        for (int i = 0; i < term->scrollback.capacity; i++) {
            term->scrollback.lines[i] = realloc(term->scrollback.lines[i],
                                                 new_cols * sizeof(term_cell_t));
            if (term->scrollback.lines[i] && new_cols > term->cols) {
                for (int c = term->cols; c < new_cols; c++) {
                    term_clear_cell(&term->scrollback.lines[i][c],
                                    term->default_fg, term->default_bg);
                }
            }
        }
        term->scrollback.cols = new_cols;
    }

    term->cols = new_cols;
    term->rows = new_rows;
    term->scroll_bottom = new_rows;

    /* Clamp cursor */
    if (term->cursor.row >= term->rows) term->cursor.row = term->rows - 1;
    if (term->cursor.col >= term->cols) term->cursor.col = term->cols - 1;

    /* Resize PTY */
    if (term->pty) {
        pty_resize(term->pty, term->cols, term->rows);
    }

    term->need_full_redraw = true;
    return true;
}

/* PoP: terminal_clear @ apps/desktop/src/app/right-sidebar/terminal/index.tsx */
void terminal_clear(terminal_t *term) {
    if (!term) return;
    term_erase_in_display(term, 2, 0, 0);
    term->cursor.row = 0;
    term->cursor.col = 0;

    /* Clear scrollback */
    term->scrollback.count = 0;
    for (int i = 0; i < term->scrollback.capacity && term->scrollback.lines; i++) {
        if (term->scrollback.lines[i]) {
            for (int c = 0; c < term->scrollback.cols; c++)
                term_clear_cell(&term->scrollback.lines[i][c],
                                term->default_fg, term->default_bg);
        }
    }
}

const term_cell_t *terminal_get_screen(const terminal_t *term, int *out_rows, int *out_cols) {
    if (!term) { *out_rows = 0; *out_cols = 0; return NULL; }
    if (out_rows) *out_rows = term->rows;
    if (out_cols) *out_cols = term->cols;
    return &term->screen[0][0];
}

const term_cell_t *terminal_get_scrollback_line(const terminal_t *term, int index) {
    if (!term || !term->scrollback.lines) return NULL;
    if (index < 0 || index >= term->scrollback.count) return NULL;
    return term->scrollback.lines[index];
}

int terminal_scrollback_count(const terminal_t *term) {
    return term ? term->scrollback.count : 0;
}

void terminal_get_cursor(const terminal_t *term, int *row, int *col, bool *visible) {
    if (!term) return;
    if (row)     *row     = term->cursor.row;
    if (col)     *col     = term->cursor.col;
    if (visible) *visible = term->cursor.visible;
}

/* ── Selection ──────────────────────────────────────────────────────────── */

void terminal_selection_set(terminal_t *term, int sr, int sc, int er, int ec) {
    if (!term) return;
    term->cursor.wrapnext = false; /* reuse: actually use a dedicated field */
    (void)sr; (void)sc; (void)er; (void)ec;
}

void terminal_selection_clear(terminal_t *term) {
    (void)term;
}

const term_selection_t *terminal_selection_get(const terminal_t *term) {
    (void)term;
    return NULL;
}

char *terminal_selection_text(terminal_t *term) {
    (void)term;
    return NULL;
}

/* ── VT100 output parser ────────────────────────────────────────────────── */

int terminal_process_output(terminal_t *term, const char *data, size_t len) {
    if (!term || !data || len == 0) return 0;

    size_t processed = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)data[i];

        switch (term->state) {
            case TERM_STATE_NORMAL:
                if (ch == 0x1B) {          /* ESC */
                    term->state = TERM_STATE_ESC;
                } else if (ch == 0x07) {   /* BEL */
                    /* ignore */
                } else if (ch == 0x08) {   /* BS: backspace */
                    if (term->cursor.col > 0)
                        term->cursor.col--;
                } else if (ch == 0x09) {   /* HT: horizontal tab */
                    do {
                        term->cursor.col++;
                    } while (term->cursor.col < term->cols &&
                             !term->tab_stops[term->cursor.col] &&
                             term->cursor.col % TERM_TAB_WIDTH != 0);
                    if (term->cursor.col >= term->cols)
                        term->cursor.col = term->cols - 1;
                } else if (ch == 0x0A || ch == 0x0B || ch == 0x0C) { /* LF/VT/FF */
                    if (term->cursor.row + 1 >= term->scroll_bottom) {
                        term_scroll_up(term, 1);
                    } else {
                        term->cursor.row++;
                    }
                } else if (ch == 0x0D) {   /* CR */
                    term->cursor.col = 0;
                } else if (ch >= 0x20) {   /* Printable */
                    if (term->decim) {
                        term_insert_chars(term, 1, term->cursor.row, term->cursor.col);
                    }
                    int row = term->cursor.row;
                    int col = term->cursor.col;
                    if (row >= 0 && row < term->rows && col >= 0 && col < term->cols) {
                        term->screen[row][col].ch    = ch;
                        term->screen[row][col].dirty = true;
                    }
                    if (term->cursor.col + 1 < term->cols) {
                        term->cursor.col++;
                    } else if (term->decawm) {
                        term->cursor.wrapnext = true;
                    }
                    /* Wrap */
                    if (term->cursor.wrapnext && term->decawm) {
                        term->cursor.col = 0;
                        if (term->cursor.row + 1 >= term->scroll_bottom) {
                            term_scroll_up(term, 1);
                        } else {
                            term->cursor.row++;
                        }
                        term->cursor.wrapnext = false;
                    }
                }
                break;

            case TERM_STATE_ESC:
                if (ch == '[') {            /* CSI */
                    term->state = TERM_STATE_CSI;
                    term->param_count = 0;
                    term->param_qmark = false;
                    memset(term->params, 0, sizeof(term->params));
                } else if (ch == ']') {     /* OSC */
                    term->state = TERM_STATE_OSC;
                    term->osc_len = 0;
                } else if (ch == 'P') {     /* DCS */
                    term->state = TERM_STATE_DCS;
                } else if (ch == '(' || ch == ')' || ch == '*' || ch == '+') { /* Charset */
                    term->state = TERM_STATE_NORMAL; /* skip charset selector */
                } else if (ch == '7') {     /* DECSC: save cursor */
                    term->saved_cursor = term->cursor;
                    term->state = TERM_STATE_NORMAL;
                } else if (ch == '8') {     /* DECRC: restore cursor */
                    term->cursor = term->saved_cursor;
                    term->state = TERM_STATE_NORMAL;
                } else if (ch == 'M') {     /* RI: reverse index */
                    if (term->cursor.row > term->scroll_top) {
                        term->cursor.row--;
                    } else {
                        term_scroll_down(term, 1);
                    }
                    term->state = TERM_STATE_NORMAL;
                } else if (ch == 'D') {     /* IND: index */
                    if (term->cursor.row + 1 < term->scroll_bottom) {
                        term->cursor.row++;
                    } else {
                        term_scroll_up(term, 1);
                    }
                    term->state = TERM_STATE_NORMAL;
                } else if (ch == 'E') {     /* NEL: next line */
                    term->cursor.col = 0;
                    if (term->cursor.row + 1 < term->scroll_bottom) {
                        term->cursor.row++;
                    } else {
                        term_scroll_up(term, 1);
                    }
                    term->state = TERM_STATE_NORMAL;
                } else if (ch == 'c') {     /* RIS: reset */
                    term->cursor.row = 0;
                    term->cursor.col = 0;
                    term->cursor.wrapnext = false;
                    term->decawm = true;
                    term->decim = false;
                    term->scroll_top = 0;
                    term->scroll_bottom = term->rows;
                    term_erase_in_display(term, 2, 0, 0);
                    term->state = TERM_STATE_NORMAL;
                } else {
                    term->state = TERM_STATE_NORMAL;
                }
                break;

            case TERM_STATE_CSI:
                if (ch == '?' || ch == '>' || ch == '!') {
                    if (ch == '?') term->param_qmark = true;
                    break;
                }
                if (ch >= '0' && ch <= '9') {
                    int idx = term->param_count;
                    if (idx < TERM_MAX_PARAMETERS) {
                        if (term->params[idx] == 0 && term->params[idx] == 0) {
                            term->params[idx] = ch - '0';
                        } else {
                            term->params[idx] = term->params[idx] * 10 + (ch - '0');
                        }
                    }
                } else if (ch == ';' || ch == ':') {
                    if (term->param_count < TERM_MAX_PARAMETERS - 1)
                        term->params[++term->param_count] = 0;
                } else if (ch >= 0x40 && ch <= 0x7E) {
                    /* Final byte */
                    if (term->param_count < TERM_MAX_PARAMETERS) {
                        term->param_count++;
                    }
                    term_csi_letter(term, (char)ch);
                    term->state = TERM_STATE_NORMAL;
                }
                break;

            case TERM_STATE_OSC:
                if (ch == 0x07 || (ch == 0x1B && i + 1 < len && data[i+1] == '\\')) {
                    /* OSC terminated */
                    if (ch == 0x1B) i++; /* skip \ */
                    term->state = TERM_STATE_NORMAL;
                    term->osc_len = 0;
                } else if (ch == 0x1B) {
                    /* Might be ST: ESC \ */
                    term->state = TERM_STATE_NORMAL;
                } else {
                    if (term->osc_len < (int)sizeof(term->osc_buf) - 1)
                        term->osc_buf[term->osc_len++] = (char)ch;
                }
                break;

            case TERM_STATE_DCS:
                if (ch == 0x1B && i + 1 < len && data[i+1] == '\\') {
                    i++;
                    term->state = TERM_STATE_NORMAL;
                } else if (ch == 0x1B) {
                    /* Could be ST start */
                }
                break;
        }
        processed++;
    }

    return (int)processed;
}
