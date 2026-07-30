/*
 * curses_widget.c — Curses interactive widget library.
 *
 * Port of Python hermes_cli/curses_ui.py — ncurses-driven checklists,
 * radiolists, picker menus, and confirmation dialogs.
 *
 * This file compiles in two modes:
 *   #ifdef HAS_NCURSES_TUI — full interactive ncurses widgets
 *   without HAS_NCURSES_TUI — numbered text fallbacks only (always portable)
 *
 * The full widgets are linked only in the `tui` build target.
 * The fallback functions are always available for the main `slermes` binary.
 */

#define _GNU_SOURCE
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#if defined(HAS_NCURSES_TUI)
#  include <curses.h>
#  include <signal.h>
#  include <sys/ioctl.h>
#  include <termios.h>
#endif

#ifdef HAS_NCURSES_TUI
/* ── Forward declarations (ncurses mode) ──────────────────── */
static bool _try_curses_init(void);
static void _init_colors(void);
static int  _filter_indices(const char **labels, int count, const char *query, int **out);
static cw_nav_action_t _decode_key(int key);
static void _flush_stdin(void);
static int  _reconcile_cursor(int *filtered, int filter_count, int cursor, int *cursor_pos);
static int  _move_cursor(int *filtered, int filter_count, int cursor_pos, int delta);
static int  _scroll_for_cursor(int scroll_offset, int cursor_pos, int visible_rows, int total_rows);
static int  _menu_event_loop(cw_menu_config_t *cfg, int cancel_value,
                             cw_key_handler_fn custom_keys, void *custom_ctx);
static void _draw_menu(cw_menu_config_t *cfg, int cursor, int scroll_offset,
                        int *filtered, int filter_count, const char *search_query);
static cw_selection_t _checklist_interactive(const char *title, char **items, int item_count,
                                    int *initial_selection, int initial_count,
                                    const char *status_text);
static int  _radiolist_interactive(const char *title, char **items, int item_count,
                                    int initial_selection, int cancel_value,
                                    const char *description, bool searchable);
static int  _picker_interactive(const char *title, char **items, int item_count,
                                 int default_index, bool searchable, const char *cancel_label);
static bool _confirm_interactive(const char *title, const char *message);
#endif

/* ════════════════════════════════════════════════════════════════
 *  Public API — conditions dispatch to ncurses interactive or
 *  fallthrough to numbered text fallback.
 * ════════════════════════════════════════════════════════════════ */

void cw_init_colors(void) {
#ifdef HAS_NCURSES_TUI
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CW_COLOR_SELECTED, COLOR_GREEN, -1);
        init_pair(CW_COLOR_HEADER,   COLOR_YELLOW, -1);
        if (COLORS > 8)
            init_pair(CW_COLOR_DIM, 8, -1);
        else
            init_pair(CW_COLOR_DIM, COLOR_WHITE, -1);
    }
#else
    /* No-op: no ncurses */
#endif
}

cw_nav_action_t cw_read_key(int key) {
#ifdef HAS_NCURSES_TUI
    return _decode_key(key);
#else
    (void)key;
    return CW_NAV_NONE;
#endif
}

int cw_run_menu(cw_menu_config_t *cfg, int cancel_value,
                cw_key_handler_fn custom_keys, void *custom_ctx)
{
#ifdef HAS_NCURSES_TUI
    return _menu_event_loop(cfg, cancel_value, custom_keys, custom_ctx);
#else
    (void)cfg;
    (void)cancel_value;
    (void)custom_keys;
    (void)custom_ctx;
    return cancel_value;
#endif
}

/* ── Widgets ────────────────────────────────────────────────── */

cw_selection_t cw_checklist(
    const char *title, char **items, int item_count,
    int *initial_selection, int initial_count, const char *status_text)
{
#ifdef HAS_NCURSES_TUI
    /* Try ncurses interactive first */
    cw_selection_t chosen = _checklist_interactive(title, items, item_count,
                                                     initial_selection, initial_count,
                                                     status_text);
    if (chosen.indices != NULL) return chosen;
#endif
    (void)status_text;
    return cw_fallback_checklist(title, items, item_count,
                                  initial_selection, initial_count);
}

int cw_radiolist(
    const char *title, char **items, int item_count,
    int initial_selection, int cancel_value,
    const char *description, bool searchable)
{
#ifdef HAS_NCURSES_TUI
    int result = _radiolist_interactive(title, items, item_count,
                                         initial_selection, cancel_value,
                                         description, searchable);
    if (result != -2) return result;  /* -2 means curses unavailable */
#endif
    (void)description;
    (void)searchable;
    return cw_fallback_radiolist(title, items, item_count,
                                  initial_selection, cancel_value);
}

int cw_picker(
    const char *title, char **items, int item_count,
    int default_index, bool searchable, const char *cancel_label)
{
#ifdef HAS_NCURSES_TUI
    int result = _picker_interactive(title, items, item_count,
                                      default_index, searchable, cancel_label);
    if (result != -2) return result;
#endif
    (void)searchable;
    (void)cancel_label;
    return cw_fallback_picker(title, items, item_count, default_index, -1);
}

bool cw_confirm(const char *title, const char *message) {
#ifdef HAS_NCURSES_TUI
    return _confirm_interactive(title, message);
#endif
    /* Text fallback */
    printf("\n  \033[33m%s\033[0m\n", title);
    if (message) printf("  \033[2m%s\033[0m\n\n", message);
    printf("  [Y]es / [N]o [default: Y]: ");
    fflush(stdout);

    char buf[16];
    if (!fgets(buf, sizeof(buf), stdin)) return true;
    buf[strcspn(buf, "\n")] = '\0';
    if (buf[0] == 'n' || buf[0] == 'N') return false;
    return true;
}

void cw_selection_free(cw_selection_t *sel) {
    if (sel) {
        free(sel->indices);
        sel->indices = NULL;
        sel->count = 0;
        sel->capacity = 0;
    }
}

/* ════════════════════════════════════════════════════════════════
 *  Numbered text fallbacks (always available)
 * ════════════════════════════════════════════════════════════════ */

int cw_fallback_radiolist(const char *title, char **items, int item_count,
                          int initial_selected, int cancel_value)
{
    printf("\n  \033[33m%s\033[0m\n", title);
    printf("  \033[2mSelect by number, Enter to confirm.\033[0m\n\n");

    for (int i = 0; i < item_count; i++) {
        const char *marker = (i == initial_selected) ? "\033[32m>>>\033[0m" : "   ";
        printf("  %s %2d. %s\n", marker, i + 1, items[i]);
    }
    printf("\n  \033[2mChoice [default %d]: \033[0m", initial_selected + 1);
    fflush(stdout);

    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin)) return cancel_value;
    buf[strcspn(buf, "\n")] = '\0';

    if (!*buf) return initial_selected;
    char *end;
    long n = strtol(buf, &end, 10);
    if (end == buf || n < 1 || n > item_count) return initial_selected;
    return (int)(n - 1);
}

cw_selection_t cw_fallback_checklist(const char *title, char **items, int item_count,
                                     int *initial_selection, int initial_count)
{
    cw_selection_t sel = {0};
    if (initial_selection && initial_count > 0) {
        sel.indices = malloc(sizeof(int) * initial_count);
        if (sel.indices) {
            memcpy(sel.indices, initial_selection, sizeof(int) * initial_count);
            sel.count = initial_count;
            sel.capacity = initial_count;
        }
    }

    printf("\n  \033[33m%s\033[0m\n", title);
    printf("  \033[2mEnter numbers (space-separated), Enter to confirm.\033[0m\n\n");

    for (int i = 0; i < item_count; i++) {
        bool checked = false;
        for (size_t j = 0; j < sel.count; j++) {
            if (sel.indices[j] == i) { checked = true; break; }
        }
        printf("  %s %2d. %s\n", checked ? "\033[32m[if✓]\033[0m" : "[  ]", i + 1, items[i]);
    }
    printf("\n  \033[2mSelected numbers (space-separated, 0=none): \033[0m");
    fflush(stdout);

    char buf[256];
    if (!fgets(buf, sizeof(buf), stdin)) return sel;
    buf[strcspn(buf, "\n")] = '\0';

    free(sel.indices);
    sel.indices = NULL;
    sel.count = 0;
    sel.capacity = 0;

    char *tok = strtok(buf, " \t");
    while (tok) {
        char *end;
        long n = strtol(tok, &end, 10);
        if (end != tok && n >= 1 && n <= item_count) {
            int idx = (int)(n - 1);
            if (sel.count >= sel.capacity) {
                sel.capacity = sel.capacity ? sel.capacity * 2 : 16;
                int *tmp = realloc(sel.indices, sizeof(int) * sel.capacity);
                if (!tmp) { free(sel.indices); sel.indices = NULL; sel.count = 0; return sel; }
                sel.indices = tmp;
            }
            sel.indices[sel.count++] = idx;
        }
        tok = strtok(NULL, " \t");
    }

    return sel;
}

int cw_fallback_picker(const char *title, char **items, int item_count,
                       int default_index, int cancel_row_idx)
{
    printf("\n  \033[33m%s\033[0m\n", title);
    printf("  \033[2mSelect by number, Enter to confirm.\033[0m\n\n");

    for (int i = 0; i < item_count; i++) {
        printf("    %2d. %s\n", i + 1, items[i]);
    }
    int def_display = (default_index >= 0 && default_index < item_count)
                      ? default_index + 1 : 1;
    printf("\n  \033[2mChoice [default %d]: \033[0m", def_display);
    fflush(stdout);

    char buf[64];
    if (!fgets(buf, sizeof(buf), stdin)) return cancel_row_idx;
    buf[strcspn(buf, "\n")] = '\0';

    if (!*buf) return default_index;
    char *end;
    long n = strtol(buf, &end, 10);
    if (end == buf || n < 1 || n > item_count) return default_index;
    return (int)(n - 1);
}

/* ════════════════════════════════════════════════════════════════
 *  NCurses interactive mode (only with HAS_NCURSES_TUI)
 * ════════════════════════════════════════════════════════════════ */

#ifdef HAS_NCURSES_TUI

/* ── Terminal detection ──────────────────────────────────── */

/* Detect whether we're in an interactive terminal that supports curses.
 * Returns true if ncurses fullscreen TUI is usable. */
static bool _try_curses_init(void) {
    if (!isatty(STDOUT_FILENO) || !isatty(STDERR_FILENO))
        return false;

    /* Check TERM is set and not "dumb" */
    const char *term = getenv("TERM");
    if (!term || !*term || strcmp(term, "dumb") == 0)
        return false;

    /* Try initialising curses */
    SCREEN *screen = newterm(NULL, stdout, stdin);
    if (!screen) return false;

    /* Check terminal dimensions are sensible */
    int rows = LINES, cols = COLS;
    if (rows < 10 || cols < 20) {
        endwin();
        delscreen(screen);
        return false;
    }

    return true;
}

/* ── Colour initialisation ───────────────────────────────── */

static void _init_colors(void) {
    cw_init_colors();
}

/* ── Fuzzy / substring search filter ─────────────────────── */

/* Filter items by case-insensitive substring match.
 * Returns the number of matching items, or -1 on allocation failure.
 * *out receives a heap-allocated array of original indices (caller frees). */
static int _filter_indices(const char **labels, int count, const char *query, int **out) {
    if (!query || !*query) {
        /* No query: return all items */
        int *all = malloc(sizeof(int) * (size_t)count);
        if (!all) return -1;
        for (int i = 0; i < count; i++) all[i] = i;
        *out = all;
        return count;
    }

    int *matches = malloc(sizeof(int) * (size_t)count);
    if (!matches) return -1;

    int n = 0;
    for (int i = 0; i < count; i++) {
        if (strcasestr(labels[i], query)) {
            matches[n++] = i;
        }
    }
    *out = matches;
    return n;
}

/* ── Cursor management ───────────────────────────────────── */

/* After filter changes, ensure cursor is on a valid filtered item.
 * Returns the ORIGINAL index (the item the cursor should point to).
 * *cursor_pos is updated to the filtered-list position. */
static int _reconcile_cursor(int *filtered, int filter_count, int cursor, int *cursor_pos) {
    if (filter_count == 0) {
        *cursor_pos = 0;
        return cursor;
    }

    /* Try to keep the same original cursor if it's still visible */
    for (int i = 0; i < filter_count; i++) {
        if (filtered[i] == cursor) {
            *cursor_pos = i;
            return cursor;
        }
    }

    /* Otherwise move to nearest — start of list */
    *cursor_pos = 0;
    return filtered[0];
}

/* Move cursor by delta within the filtered list.
 * Returns the new ORIGINAL index, and updates *cursor_pos. */
static int _move_cursor(int *filtered, int filter_count, int cursor_pos, int delta) {
    if (filter_count == 0) return -1;
    int new_pos = cursor_pos + delta;
    if (new_pos < 0) new_pos = 0;
    if (new_pos >= filter_count) new_pos = filter_count - 1;
    return filtered[new_pos];
}

/* Calculate scroll offset to keep cursor_pos visible.
 * Returns the new scroll offset. */
static int _scroll_for_cursor(int scroll_offset, int cursor_pos, int visible_rows, int total_rows) {
    if (total_rows <= visible_rows) return 0;
    if (cursor_pos < scroll_offset)
        return cursor_pos;
    if (cursor_pos >= scroll_offset + visible_rows)
        return cursor_pos - visible_rows + 1;
    return scroll_offset;
}

/* ── Key decoding ────────────────────────────────────────── */

static void _flush_stdin(void) {
    if (!isatty(STDIN_FILENO)) return;
    tcflush(STDIN_FILENO, TCIFLUSH);
}

static cw_nav_action_t _decode_key(int key) {
    if (key == KEY_UP || key == 'k')
        return CW_NAV_UP;
    if (key == KEY_DOWN || key == 'j')
        return CW_NAV_DOWN;
    if (key == KEY_ENTER || key == 10 || key == 13)
        return CW_NAV_SELECT;
    if (key == ' ')
        return CW_NAV_TOGGLE;
    if (key == '/' || key == KEY_F(2))
        return CW_NAV_SEARCH;
    if (key == 27) {
        /* Escape sequence — could be ESC or arrow sequence */
        timeout(60);
        int nxt = getch();
        timeout(-1);
        if (nxt == ERR) return CW_NAV_CANCEL;
        if (nxt == 'q' || nxt == 'Q') {
            /* ESC q = quit */
            return CW_NAV_CANCEL;
        }
        /* Arrow sequences: ESC [ A, ESC O A */
        if (nxt == '[' || nxt == 'O') {
            int final = getch();
            if (final == 'A' || final == 'k') return CW_NAV_UP;
            if (final == 'B' || final == 'j') return CW_NAV_DOWN;
            /* Consume any remaining CSI bytes */
            while (0x20 <= final && final <= 0x3F) final = getch();
            return CW_NAV_NONE;
        }
        return CW_NAV_NONE;
    }
    return CW_NAV_NONE;
}

/* ── Drawing ─────────────────────────────────────────────── */

/* Draw the entire menu screen: header, items (with scrolling), footer.
 * Uses the config's callbacks for header/row/footer. */
static void _draw_menu(cw_menu_config_t *cfg, int cursor, int scroll_offset,
                        int *filtered, int filter_count, const char *search_query)
{
    erase();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    /* Draw header via callback */
    bool search_active = (search_query != NULL);
    int row = cfg->draw_header(cfg->draw_ctx, max_y, max_x,
                                search_active, search_query ? search_query : "");
    if (row < 0) row = 0;

    /* Calculate visible rows for items */
    int bottom_margin = cfg->reserve_bottom;
    int visible_rows = max_y - row - bottom_margin;
    if (visible_rows < 1) visible_rows = 1;

    /* Clamp scroll offset */
    if (filter_count > 0) {
        int max_scroll = filter_count - visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        if (scroll_offset > max_scroll) scroll_offset = max_scroll;
        if (scroll_offset < 0) scroll_offset = 0;
    } else {
        scroll_offset = 0;
    }

    /* Draw items */
    for (int i = 0; i < visible_rows && (i + scroll_offset) < filter_count; i++) {
        int fi = i + scroll_offset;
        int orig_idx = filtered[fi];
        bool is_cursor = (orig_idx == cursor);
        cfg->draw_row(cfg->draw_ctx, row + i, orig_idx, is_cursor, max_x);
    }

    /* Draw scroll indicator if needed */
    if (filter_count > visible_rows) {
        int pct = (scroll_offset * 100) / (filter_count - visible_rows);
        attron(A_DIM);
        mvprintw(max_y - 1 - bottom_margin, max_x - 12, " [%d%%] ", pct);
        attroff(A_DIM);
    }

    /* Draw footer */
    if (cfg->draw_footer)
        cfg->draw_footer(cfg->draw_ctx, max_y, max_x);

    /* Draw search bar at bottom if active */
    if (search_active) {
        attron(A_REVERSE);
        mvprintw(max_y - 1, 0, " Search: %-*s", max_x - 10, search_query);
        attroff(A_REVERSE);
    }

    refresh();
}

/* ── Core event loop ─────────────────────────────────────── */

/* Run the shared ncurses menu event loop.
 * Returns the final cursor value (original index) on confirm,
 * or cancel_value on cancel/error.
 *
 * cfg: menu config with callbacks
 * cancel_value: returned when user cancels
 * custom_keys: optional handler for extra keys (return CW_NAV_NONE to fall through)
 * custom_ctx: passed to custom_keys
 */
static int _menu_event_loop(cw_menu_config_t *cfg, int cancel_value,
                             cw_key_handler_fn custom_keys, void *custom_ctx)
{
    if (!cfg || cfg->item_count <= 0)
        return cancel_value;

    /* Build full index list (no filter initially) */
    int *filtered = NULL;
    int filter_count = _filter_indices(NULL, cfg->item_count, "", &filtered);
    if (filter_count < 0) return cancel_value;

    int cursor_pos = 0;
    int cursor = cfg->initial_cursor;
    if (cursor < 0 || cursor >= cfg->item_count) cursor = 0;

    /* Reconcile cursor with initial filter */
    cursor = _reconcile_cursor(filtered, filter_count, cursor, &cursor_pos);

    char search_query[256] = "";
    bool searching = false;
    int search_pos = 0;
    int scroll_offset = 0;

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);  /* Hide cursor */

    for (;;) {
        /* Scroll to keep cursor visible */
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        int header_rows = 3; /* approximate — call draw_header to get real count */
        if (cfg->draw_header) {
            int hr = cfg->draw_header(cfg->draw_ctx, max_y, max_x, false, "");
            header_rows = (hr < 0) ? 3 : hr + 1;
        }
        int visible_rows = max_y - header_rows - cfg->reserve_bottom;
        if (visible_rows < 1) visible_rows = 1;

        /* Rebuild filtered list if search active */
        if (searching && search_query[0]) {
            int *new_filtered = NULL;
            int new_count = _filter_indices(cfg->search_labels ? cfg->search_labels : NULL,
                                             cfg->item_count, search_query, &new_filtered);
            if (new_count >= 0) {
                free(filtered);
                filtered = new_filtered;
                filter_count = new_count;
                cursor = _reconcile_cursor(filtered, filter_count, cursor, &cursor_pos);
            }
        } else if (searching && !search_query[0]) {
            /* No search query — show all */
            free(filtered);
            filter_count = _filter_indices(NULL, cfg->item_count, "", &filtered);
            if (filter_count >= 0) {
                cursor = _reconcile_cursor(filtered, filter_count, cursor, &cursor_pos);
            }
        }

        scroll_offset = _scroll_for_cursor(scroll_offset, cursor_pos,
                                            visible_rows, filter_count);

        /* Draw */
        _draw_menu(cfg, cursor, scroll_offset, filtered, filter_count,
                   searching ? search_query : NULL);

        /* Read key */
        int key = getch();

        if (searching) {
            /* Search mode — accumulate characters */
            if (key == 27) {
                /* ESC exits search mode */
                searching = false;
                search_query[0] = '\0';
                search_pos = 0;
                /* Rebuild full list */
                free(filtered);
                filter_count = _filter_indices(NULL, cfg->item_count, "", &filtered);
                if (filter_count >= 0)
                    cursor = _reconcile_cursor(filtered, filter_count, cursor, &cursor_pos);
            } else if (key == KEY_BACKSPACE || key == 127 || key == '\b') {
                if (search_pos > 0) {
                    search_query[--search_pos] = '\0';
                }
            } else if (key == KEY_ENTER || key == 10 || key == 13) {
                searching = false;
                /* Confirm the current selection */
                break;
            } else if (key == 'q' || key == 'Q') {
                searching = false;
                search_query[0] = '\0';
                search_pos = 0;
                /* Fall through to cancel via q handler below */
            } else if (key >= 32 && key <= 126 && search_pos < (int)sizeof(search_query) - 1) {
                search_query[search_pos++] = (char)key;
                search_query[search_pos] = '\0';
            }
            continue;
        }

        cw_nav_action_t action = _decode_key(key);

        /* Custom key handler */
        if (action == CW_NAV_NONE && custom_keys) {
            action = custom_keys(custom_ctx, key);
        }

        switch (action) {
        case CW_NAV_UP:
            cursor = _move_cursor(filtered, filter_count, cursor_pos, -1);
            cursor_pos--;
            if (cursor_pos < 0) cursor_pos = 0;
            break;

        case CW_NAV_DOWN:
            cursor = _move_cursor(filtered, filter_count, cursor_pos, 1);
            cursor_pos++;
            if (cursor_pos >= filter_count) cursor_pos = filter_count - 1;
            break;

        case CW_NAV_SELECT:
            /* Confirm selection */
            free(filtered);
            endwin();
            _flush_stdin();
            return cursor;

        case CW_NAV_TOGGLE:
            /* Toggle (used by checklist) — handled by draw_row callback.
             * For the menu loop, we just return the cursor and let
             * the caller interpret. */
            free(filtered);
            endwin();
            _flush_stdin();
            return cursor;

        case CW_NAV_CANCEL:
            free(filtered);
            endwin();
            _flush_stdin();
            return cancel_value;

        case CW_NAV_SEARCH:
            searching = true;
            search_pos = 0;
            search_query[0] = '\0';
            break;

        case CW_NAV_NONE:
        default:
            break;
        }
    }

    free(filtered);
    endwin();
    _flush_stdin();
    return cursor;
}

/* ── Checklist interactive ───────────────────────────────── */

/* Context for checklist draw callbacks */
typedef struct {
    char **items;
    int item_count;
    int *selected;         /* list of selected indices */
    int selected_count;
    int selected_capacity;
    const char *status_text;
} checklist_ctx_t;

static int _checklist_draw_header(void *ctx, int max_y, int max_x,
                                   bool search_active, const char *search_query) {
    (void)max_y;
    (void)search_active;
    (void)search_query;
    checklist_ctx_t *c = (checklist_ctx_t *)ctx;
    attron(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));
    mvprintw(0, 0, " %s  [Space] toggle  [Enter] confirm  [q] cancel  [/] search",
             c->status_text ? c->status_text : "(checklist)");
    attroff(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));
    mvprintw(1, 0, " %d selected", c->selected_count);
    return 2;
}

static void _checklist_draw_row(void *ctx, int y, int idx,
                                 bool is_cursor, int max_x) {
    (void)max_x;
    checklist_ctx_t *c = (checklist_ctx_t *)ctx;
    bool checked = false;
    for (int i = 0; i < c->selected_count; i++) {
        if (c->selected[i] == idx) { checked = true; break; }
    }

    if (is_cursor)
        attron(A_REVERSE);
    mvprintw(y, 0, " %s %s",
             checked ? "[x]" : "[ ]",
             idx < c->item_count ? c->items[idx] : "(invalid)");
    if (is_cursor)
        attroff(A_REVERSE);
}

static cw_nav_action_t _checklist_custom_keys(void *ctx, int key) {
    (void)ctx;
    if (key == ' ') return CW_NAV_TOGGLE;
    return CW_NAV_NONE;
}

static cw_selection_t _checklist_interactive(
    const char *title, char **items, int item_count,
    int *initial_selection, int initial_count,
    const char *status_text)
{
    cw_selection_t result = {0};

    /* Try to init curses */
    if (!_try_curses_init()) {
        /* Return empty selection to signal fallback */
        return result;
    }

    _init_colors();

    /* Build selected set */
    checklist_ctx_t ctx;
    ctx.items = items;
    ctx.item_count = item_count;
    ctx.status_text = status_text ? status_text : title;
    ctx.selected = NULL;
    ctx.selected_count = 0;
    ctx.selected_capacity = 0;

    if (initial_selection && initial_count > 0) {
        ctx.selected = malloc(sizeof(int) * (size_t)initial_count);
        if (ctx.selected) {
            memcpy(ctx.selected, initial_selection, sizeof(int) * (size_t)initial_count);
            ctx.selected_count = initial_count;
            ctx.selected_capacity = initial_count;
        }
    }

    /* Build menu config */
    cw_menu_config_t cfg = {0};
    cfg.initial_cursor = 0;
    cfg.item_count = item_count;
    cfg.draw_header = _checklist_draw_header;
    cfg.draw_row = _checklist_draw_row;
    cfg.draw_ctx = &ctx;
    cfg.reserve_bottom = 1;
    cfg.searchable = true;

    /* Build search labels */
    const char **search_labels = malloc(sizeof(const char *) * (size_t)item_count);
    if (search_labels) {
        for (int i = 0; i < item_count; i++)
            search_labels[i] = items[i];
        cfg.search_labels = search_labels;
    }

    /* Run menu event loop with toggle handling */
    /* We override the menu loop to handle toggle: on toggle, we toggle
     * the current item and redraw, on confirm we return the selected set. */
    int chosen = _menu_event_loop(&cfg, -1, _checklist_custom_keys, &ctx);

    /* If user confirmed (chosen >= 0), wrap selected set into result */
    if (chosen >= 0 || chosen != -1) {
        /* User confirmed — return the built selection */
        result.indices = ctx.selected;
        result.count = (size_t)ctx.selected_count;
        result.capacity = (size_t)ctx.selected_capacity;
    } else {
        free(ctx.selected);
    }

    free(search_labels);
    return result;
}

/* ── Radiolist interactive ───────────────────────────────── */

/* Context for radiolist draw callbacks */
typedef struct {
    char **items;
    int item_count;
    int selected;
    const char *description;
    const char *title;
} radiolist_ctx_t;

static int _radiolist_draw_header(void *ctx, int max_y, int max_x,
                                   bool search_active, const char *search_query) {
    (void)max_y;
    (void)search_active;
    (void)search_query;
    radiolist_ctx_t *r = (radiolist_ctx_t *)ctx;
    attron(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));
    mvprintw(0, 0, " %s", r->title);
    attroff(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));

    if (r->description) {
        int row = 1;
        /* Word-wrap description */
        const char *p = r->description;
        while (*p) {
            int line_w = max_x - 2;
            attron(A_DIM);
            mvprintw(row, 1, "%-.*s", line_w, p);
            attroff(A_DIM);
            /* Advance by line_w chars or to next newline */
            const char *nl = strchr(p, '\n');
            if (nl && (nl - p) <= line_w) {
                p = nl + 1;
            } else {
                p += line_w;
            }
            row++;
        }
        mvprintw(row, 0, "  [arrows] navigate  [Enter] select  [/] search  [q] cancel");
        return row + 1;
    }
    mvprintw(1, 0, "  [arrows] navigate  [Enter] select  [/] search  [q] cancel");
    return 2;
}

static void _radiolist_draw_row(void *ctx, int y, int idx,
                                 bool is_cursor, int max_x) {
    (void)max_x;
    radiolist_ctx_t *r = (radiolist_ctx_t *)ctx;
    bool selected = (idx == r->selected);

    if (is_cursor)
        attron(A_REVERSE);
    mvprintw(y, 0, "  %s %s",
             selected ? "(o)" : "( )",
             idx < r->item_count ? r->items[idx] : "(invalid)");
    if (is_cursor)
        attroff(A_REVERSE);
}

static int _radiolist_interactive(
    const char *title, char **items, int item_count,
    int initial_selection, int cancel_value,
    const char *description, bool searchable)
{
    if (!_try_curses_init()) return -2;  /* -2 signals ncurses unavailable */

    _init_colors();

    radiolist_ctx_t ctx;
    ctx.items = items;
    ctx.item_count = item_count;
    ctx.selected = initial_selection;
    ctx.description = description;
    ctx.title = title;

    cw_menu_config_t cfg = {0};
    cfg.initial_cursor = initial_selection;
    cfg.item_count = item_count;
    cfg.draw_header = _radiolist_draw_header;
    cfg.draw_row = _radiolist_draw_row;
    cfg.draw_ctx = &ctx;
    cfg.reserve_bottom = 1;
    cfg.searchable = searchable;

    if (searchable) {
        const char **search_labels = malloc(sizeof(const char *) * (size_t)item_count);
        if (search_labels) {
            for (int i = 0; i < item_count; i++)
                search_labels[i] = items[i];
            cfg.search_labels = search_labels;
        }
    }

    int result = _menu_event_loop(&cfg, cancel_value, NULL, NULL);

    if (cfg.search_labels) free((void *)cfg.search_labels);
    return result;
}

/* ── Picker interactive ──────────────────────────────────── */

typedef struct {
    char **items;
    int item_count;
    int cancel_row_idx;
    const char *title;
    const char *cancel_label;
} picker_ctx_t;

static int _picker_draw_header(void *ctx, int max_y, int max_x,
                                bool search_active, const char *search_query) {
    (void)max_y;
    (void)search_active;
    (void)search_query;
    picker_ctx_t *p = (picker_ctx_t *)ctx;
    attron(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));
    mvprintw(0, 0, " %s", p->title);
    attroff(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));
    mvprintw(1, 0, "  [arrows] navigate  [Enter] select  [/] search  [q] cancel");
    return 2;
}

static void _picker_draw_row(void *ctx, int y, int idx,
                              bool is_cursor, int max_x) {
    (void)max_x;
    picker_ctx_t *p = (picker_ctx_t *)ctx;
    bool is_cancel = (idx >= p->item_count);

    if (is_cursor)
        attron(A_REVERSE);
    if (is_cancel) {
        mvprintw(y, 0, "  %s", p->cancel_label ? p->cancel_label : "Cancel");
    } else {
        mvprintw(y, 0, "  %s", p->items[idx]);
    }
    if (is_cursor)
        attroff(A_REVERSE);
}

static int _picker_interactive(
    const char *title, char **items, int item_count,
    int default_index, bool searchable, const char *cancel_label)
{
    if (!_try_curses_init()) return -2;

    _init_colors();

    picker_ctx_t ctx;
    ctx.items = items;
    ctx.item_count = item_count;
    ctx.cancel_row_idx = item_count;  /* cancel is the last row */
    ctx.title = title;
    ctx.cancel_label = cancel_label;

    /* Build item list with Cancel appended */
    int total_count = item_count + 1;
    const char **picker_items = malloc(sizeof(char *) * (size_t)total_count);
    if (!picker_items) return default_index;
    for (int i = 0; i < item_count; i++)
        picker_items[i] = items[i];
    picker_items[item_count] = cancel_label ? cancel_label : "Cancel";

    /* We need a flat array of labels; use items directly + cancel */
    /* Rebuild into search_labels */
    char **search_labels = NULL;
    if (searchable) {
        search_labels = malloc(sizeof(char *) * (size_t)total_count);
        if (search_labels) {
            for (int i = 0; i < item_count; i++)
                search_labels[i] = items[i];
            search_labels[item_count] = (char *)(cancel_label ? cancel_label : "Cancel");
        }
    }

    cw_menu_config_t cfg = {0};
    cfg.initial_cursor = default_index;
    cfg.item_count = total_count;
    cfg.draw_header = _picker_draw_header;
    cfg.draw_row = _picker_draw_row;
    cfg.draw_ctx = &ctx;
    cfg.reserve_bottom = 1;
    cfg.searchable = searchable;
    if (search_labels) {
        const char **cst = malloc(sizeof(const char *) * (size_t)total_count);
        if (cst) {
            for (int i = 0; i < total_count; i++)
                cst[i] = search_labels[i];
            cfg.search_labels = cst;
        }
    }

    int result = _menu_event_loop(&cfg, -1, NULL, NULL);

    free((void *)cfg.search_labels);
    free(search_labels);
    free(picker_items);

    /* If result is the cancel row, return -1 */
    if (result >= item_count) return -1;
    return result;
}

/* ── Confirm dialog interactive ──────────────────────────── */

static bool _confirm_interactive(const char *title, const char *message) {
    if (!_try_curses_init()) {
        /* Can't init curses — fall through to text fallback.
         * Return value doesn't matter; caller will use text fallback. */
        return true;
    }

    _init_colors();

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    /* Dialog box */
    int box_h = 6;
    int box_w = max_x < 60 ? max_x - 4 : 52;
    int box_y = (max_y - box_h) / 2;
    int box_x = (max_x - box_w) / 2;
    if (box_y < 0) box_y = 0;
    if (box_x < 0) box_x = 0;

    bool selected = true;  /* true = Yes, false = No */

    for (;;) {
        erase();

        /* Draw border */
        for (int r = 0; r < box_h; r++) {
            mvhline(box_y + r, box_x, ' ', box_w);
        }
        attron(A_BOLD);
        mvaddch(box_y, box_x, ACS_ULCORNER);
        mvaddch(box_y, box_x + box_w - 1, ACS_URCORNER);
        mvaddch(box_y + box_h - 1, box_x, ACS_LLCORNER);
        mvaddch(box_y + box_h - 1, box_x + box_w - 1, ACS_LRCORNER);
        for (int x = 1; x < box_w - 1; x++) {
            mvaddch(box_y, x, ACS_HLINE);
            mvaddch(box_y + box_h - 1, x, ACS_HLINE);
        }
        for (int r = 1; r < box_h - 1; r++) {
            mvaddch(box_y + r, box_x, ACS_VLINE);
            mvaddch(box_y + r, box_x + box_w - 1, ACS_VLINE);
        }
        attroff(A_BOLD);

        /* Title */
        attron(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));
        mvprintw(box_y + 1, box_x + 2, "%-*s", box_w - 4, title);
        attroff(A_BOLD | COLOR_PAIR(CW_COLOR_HEADER));

        /* Message */
        if (message) {
            int msg_lines = 0;
            const char *p = message;
            while (*p) {
                if (msg_lines >= 1) break;  /* max 2 lines */
                int line_w = box_w - 4;
                mvprintw(box_y + 2 + msg_lines, box_x + 2, "%-.*s", line_w, p);
                const char *nl = strchr(p, '\n');
                if (nl && (nl - p) <= line_w) {
                    p = nl + 1;
                } else {
                    p += line_w;
                }
                msg_lines++;
            }
        }

        /* Yes / No buttons */
        int btn_y = box_y + box_h - 2;
        int btn_x_yes = box_x + box_w / 2 - 10;
        int btn_x_no  = box_x + box_w / 2 + 2;

        if (selected) {
            attron(A_REVERSE);
            mvprintw(btn_y, btn_x_yes, " [ Yes ] ");
            attroff(A_REVERSE);
            mvprintw(btn_y, btn_x_no,  "  No  ");
        } else {
            mvprintw(btn_y, btn_x_yes, "  Yes  ");
            attron(A_REVERSE);
            mvprintw(btn_y, btn_x_no,  " [ No ] ");
            attroff(A_REVERSE);
        }

        refresh();

        int key = getch();
        if (key == KEY_LEFT || key == KEY_RIGHT) {
            selected = !selected;
        } else if (key == KEY_ENTER || key == 10 || key == 13) {
            endwin();
            _flush_stdin();
            return selected;
        } else if (key == 27 || key == 'q' || key == 'Q') {
            endwin();
            _flush_stdin();
            return false;
        }
    }
}

#endif /* HAS_NCURSES_TUI */
