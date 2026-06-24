/*
 * curses_widget.h — Curses interactive widget library.
 *
 * Port of Python hermes_cli/curses_ui.py: multi-select checklists,
 * single-select radiolists, confirmation dialogs, and fuzzy-search
 * picker menus — all driven by ncurses with arrow-key navigation.
 *
 * Every widget provides a curses-based interactive mode and a numbered
 * text fallback for terminals where curses is unavailable.
 *
 * See Also: hermes_cli/curses_ui.py (Python original, ~872 LOC)
 *   → curses_checklist()     → cw_checklist()
 *   → curses_radiolist()     → cw_radiolist()
 *   → curses_single_select() → cw_picker()
 *   → Confirm dialog         → cw_confirm()
 */

#ifndef HERMES_CURSES_WIDGET_H
#define HERMES_CURSES_WIDGET_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Result types ──────────────────────────────────────────── */

/* Wrapper so we can return sets of ints (checklist) */
typedef struct {
    int *indices;      /* heap-allocated array of selected indices */
    size_t count;      /* number of selected indices */
    size_t capacity;   /* internal capacity */
} cw_selection_t;

/* ── Colour pair indices (used internally by all widgets) ──── */
#define CW_COLOR_NORMAL   0  /* default terminal colours */
#define CW_COLOR_SELECTED 1  /* green — selected item */
#define CW_COLOR_HEADER   2  /* yellow — title text */
#define CW_COLOR_DIM      3  /* dim gray — hints, status */

/* ── Navigation action codes ──────────────────────────────── */
typedef enum {
    CW_NAV_NONE = 0,
    CW_NAV_UP,
    CW_NAV_DOWN,
    CW_NAV_SELECT,   /* Enter */
    CW_NAV_TOGGLE,   /* Space (checklist only) */
    CW_NAV_CANCEL,   /* ESC / q */
    CW_NAV_SEARCH,   /* / key */
} cw_nav_action_t;

/* ── Menu callbacks ───────────────────────────────────────── */

/* Draw title/hint rows. Returns the first row index where the item list starts. */
typedef int (*cw_draw_header_fn)(void *ctx, int max_y, int max_x,
                                 bool search_active, const char *search_query);

/* Draw one item row. idx is the ORIGINAL item index (not a filtered position). */
typedef void (*cw_draw_row_fn)(void *ctx, int y, int idx,
                               bool is_cursor, int max_x);

/* Draw bottom status bar (optional — pass NULL to skip). */
typedef void (*cw_draw_footer_fn)(void *ctx, int max_y, int max_x);

/* Resolve a keypress. Return CW_NAV_* or custom action code. */
typedef cw_nav_action_t (*cw_key_handler_fn)(void *ctx, int key);

/* ── Config ──────────────────────────────────────────────── */

typedef struct {
    int        initial_cursor;   /* starting cursor position */
    int        item_count;       /* total number of items */
    cw_draw_header_fn draw_header;   /* required */
    cw_draw_row_fn    draw_row;      /* required */
    void      *draw_ctx;         /* user context for callbacks */
    int        reserve_bottom;   /* rows reserved at bottom (0-2) */
    cw_draw_footer_fn draw_footer; /* optional */
    bool       extra_colors;     /* init colour pair 3 (dim) */
    bool       searchable;       /* enable / key for fuzzy search */
    const char **search_labels;    /* per-item text for search (len=item_count) */
    const char *title;           /* widget title (displayed in fallback too) */
} cw_menu_config_t;

/* ── API ─────────────────────────────────────────────────── */

/*
 * Initialize colour pairs. Called automatically by each widget;
 * safe to call early to pre-init.
 */
void cw_init_colors(void);

/*
 * Read one keypress and decode to a navigation action.
 * Handles arrow keys, vi keys (j/k), escape sequences,
 * Enter, Space, q.
 */
cw_nav_action_t cw_read_key(int key);

/*
 * Run the shared curses menu event loop.
 * Exposed so custom widgets can reuse the loop machinery.
 * Returns the final value from on_action, or cancel_value on cancel.
 */
int cw_run_menu(cw_menu_config_t *cfg, int cancel_value,
                cw_key_handler_fn custom_keys, void *custom_ctx);

/* ── Widgets ─────────────────────────────────────────────── */

/*
 * Multi-select checklist with checkboxes.
 * Returns a heap-allocated cw_selection_t (caller must free with cw_selection_free).
 * On cancel/error, returns current selected set (unchanged).
 * When curses is unavailable, falls back to a numbered text prompt.
 */
cw_selection_t cw_checklist(
    const char *title,
    char **items,
    int item_count,
    int *initial_selection,   /* array of pre-selected indices, or NULL */
    int initial_count,        /* length of initial_selection (0 = none pre-selected) */
    const char *status_text   /* optional bottom status text, NULL = none */
);

/*
 * Single-select radio list.
 * Returns the selected index (0-based) on confirm, or cancel_value on cancel.
 * When curses is unavailable, falls back to numbered text prompt.
 */
int cw_radiolist(
    const char *title,
    char **items,
    int item_count,
    int initial_selection,    /* index of pre-selected item */
    int cancel_value,         /* returned on cancel */
    const char *description,  /* optional multi-line description, NULL = none */
    bool searchable           /* enable / key for fuzzy filtering */
);

/*
 * Single-select picker menu (with optional Cancel row at the bottom).
 * Returns the selected index (0-based), or -1 on cancel.
 * When curses is unavailable, falls back to numbered text prompt.
 */
int cw_picker(
    const char *title,
    char **items,
    int item_count,
    int default_index,        /* default selection */
    bool searchable,          /* enable / key for fuzzy filtering */
    const char *cancel_label  /* label for cancel row, or NULL for default "Cancel" */
);

/*
 * Confirmation dialog (yes/no).
 * Returns true for Yes, false for No / cancel.
 * When curses is unavailable, falls back to text prompt.
 */
bool cw_confirm(
    const char *title,
    const char *message      /* message to display; line breaks supported */
);

/*
 * Free a cw_selection_t returned by cw_checklist.
 */
void cw_selection_free(cw_selection_t *sel);

/*
 * Numbered text fallback: display items as a numbered list, prompt for selection.
 * Used automatically by widget functions when curses is unavailable.
 * Can be called directly for custom use.
 */

/* Radio-style numbered fallback (single select) */
int cw_fallback_radiolist(const char *title, char **items, int item_count,
                          int initial_selected, int cancel_value);

/* Checklist-style numbered fallback (multi-select) */
cw_selection_t cw_fallback_checklist(const char *title, char **items, int item_count,
                                     int *initial_selection, int initial_count);

/* Picker-style numbered fallback (with cancel row) */
int cw_fallback_picker(const char *title, char **items, int item_count,
                       int default_index, int cancel_row_idx);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CURSES_WIDGET_H */
