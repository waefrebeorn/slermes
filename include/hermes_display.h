/**
 * @defgroup hermes_display Display
 * @brief Terminal output formatting.
 *
 *
ANSI color, spinner/progress display, structured output.
Used by CLI and TUI for status messages and activity
feedback.
 *
 * @{
 */
#ifndef HERMES_DISPLAY_H
#define HERMES_DISPLAY_H

/*
 * hermes_display.h — Terminal display for Hermes C.
 * Uses ANSI escape codes. No ncurses dependency.
 * Supports: colors, progress bars, spinners, panels.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Terminal color codes */
typedef enum {
    DISPLAY_BLACK   = 0,
    DISPLAY_RED     = 1,
    DISPLAY_GREEN   = 2,
    DISPLAY_YELLOW  = 3,
    DISPLAY_BLUE    = 4,
    DISPLAY_MAGENTA = 5,
    DISPLAY_CYAN    = 6,
    DISPLAY_WHITE   = 7,
    DISPLAY_DEFAULT = 9,
} display_color_t;

/* Style flags */
typedef enum {
    DISPLAY_NORMAL      = 0,
    DISPLAY_BOLD        = 1 << 0,
    DISPLAY_DIM         = 1 << 1,
    DISPLAY_ITALIC      = 1 << 2,
    DISPLAY_UNDERLINE   = 1 << 3,
} display_style_t;

/* Initialize terminal (check if output is a TTY) */
void display_init(void);

/* Set text color and style */
void display_set_fg(display_color_t color);
void display_set_bg(display_color_t color);
void display_set_style(display_style_t style);
void display_reset(void);

/* Set truecolor (24-bit) foreground/background */
void display_set_fg_rgb(int r, int g, int b);
void display_set_bg_rgb(int r, int g, int b);

/* 256-color palette foreground and background */
void display_set_fg_256(int color);
void display_set_bg_256(int color);

/* Print formatted text with truecolor foreground */
void display_printf_hex(const char *hex_fg, display_style_t style,
                        const char *fmt, ...);

/* Print colored text */
void display_printf(display_color_t color, display_style_t style,
                    const char *fmt, ...);

/* Clear screen */
void display_clear(void);

/* Move cursor */
void display_goto(int row, int col);
void display_save_pos(void);
void display_restore_pos(void);

/* Progress bar */
typedef struct {
    int    width;
    int    current;
    int    total;
    char   label[64];
} display_progress_t;

void display_progress_init(display_progress_t *bar, const char *label, int total);
void display_progress_update(display_progress_t *bar, int current);
void display_progress_done(display_progress_t *bar);

/* Spinner */
typedef struct {
    int   frame;
    int   frame_count;
    char *label;
    char *face;           /* current kawaii face string (malloc'd) */
    bool  active;
} display_spinner_t;

void display_spinner_start(display_spinner_t *sp, const char *label);
void display_spinner_tick(display_spinner_t *sp);
void display_spinner_stop(display_spinner_t *sp, const char *done_msg);
display_spinner_t *display_spinner_enter(display_spinner_t *sp, const char *label);
int display_spinner_exit(display_spinner_t *sp);

/* Spinner type enum — mirrors Python KawaiiSpinner.SPINNERS */
typedef enum {
    SPINNER_KAWAII = 0,  /* default: kawaii face cycles */
    SPINNER_DOTS,        /* braille dots */
    SPINNER_BOUNCE,      /* braille bouncing */
    SPINNER_GROW,        /* growing block */
    SPINNER_ARROWS,      /* rotating arrows */
    SPINNER_STAR,        /* star sparkle */
    SPINNER_MOON,        /* moon phases */
    SPINNER_PULSE,       /* pulse arcs */
    SPINNER_BRAIN,       /* brain emoji cycle */
    SPINNER_SPARKLE,     /* sparkle glyphs */
} spinner_type_t;

/* Kawaii spinner with animated face cycles + verb + wings */
typedef struct {
    int    frame;
    char  *label;
    spinner_type_t type; /* which spinner frame set to use */
    char   face[64];       /* current face emoji/kaomoji */
    char   verb[64];       /* current thinking verb (empty for waiting) */
    char   wing_left[16];  /* left wing decoration */
    char   wing_right[16]; /* right wing decoration */
    bool   active;
    bool   thinking;       /* true=thinking faces, false=waiting faces */
    double start_time;     /* seconds since epoch */
} display_kawaii_t;

void display_kawaii_start(display_kawaii_t *sp, const char *label, bool thinking);
void display_kawaii_tick(display_kawaii_t *sp);
void display_kawaii_stop(display_kawaii_t *sp, const char *done_msg);

/* Parse spinner style name to type enum (for display.spinner_style config). */
spinner_type_t display_parse_spinner_type(const char *style);

/* Set the display skin for skin-driven styling. Pass NULL to unset. */
void display_set_skin(void *skin);

/* Syntax highlight: wrap inline code/block fences with ANSI colors. Returns malloc'd string. */
char *display_highlight_code(const char *text);

/* Tool activity feed — build one-line preview from tool name + args JSON */
/* Returns malloc'd string (caller free) or NULL if no preview possible. */
char *display_tool_preview(const char *tool_name, const char *args_json);

/* ─── Diff ANSI accessors (port of Python _diff_dim, _diff_file, etc.) ─── */
/* Each returns a pointer to the cached ANSI escape sequence (static buffer). */
const char *display_diff_dim(void);
const char *display_diff_file(void);
const char *display_diff_hunk(void);
const char *display_diff_minus(void);
const char *display_diff_plus(void);

/* Render a unified diff with ANSI color for inline display */
/* Returns malloc'd string (caller free). */
char *display_inline_diff(const char *diff_text);

/* Print a tool activity line with ┊ prefix, emoji, tool name, preview */
void display_tool_activity(const char *tool_name, const char *preview,
                           display_color_t color);

/* ================================================================
 *  Output Helpers — colored print wrappers (Python cli_output.py parity)
 * ================================================================ */

/* Print a dim informational message with 2-space indent */
void display_print_info(const char *text);

/* Print a green success message with ✓ prefix */
void display_print_success(const char *text);

/* Print a yellow warning message with ⚠ prefix */
void display_print_warning(const char *text);

/* Print a red error message with ✗ prefix */
void display_print_error(const char *text);

/* Print a bold yellow header with leading newline */
void display_print_header(const char *text);

/* Print a rich error with contextual suggestion */
void display_print_error_rich(const char *error_msg, const char *suggestion);

/* Display a random startup tip after banner */
void display_show_tip(void);

/* Print a box/panel around text (with word-wrap) */
void display_panel(const char *title, const char *content, display_color_t color);

/* Print a panel with TrueColor hex border color */
void display_panel_hex(const char *title, const char *content, const char *border_hex);

/* Horizontal rule with TrueColor hex */
void display_hr_hex(const char *hex_fg);

/* ================================================================
 *  Status Bar
 * ================================================================ */

/* Display a status bar line showing model, session, and context info.
 * Uses skin colors (status_bar_bg, status_bar_text, status_bar_dim)
 * when a skin is active via display_set_skin(). Pass NULL fg_bg to use
 * terminal defaults. */
void display_statusbar(const char *model, const char *session_id,
                       int turn_count, int token_count, int max_iters,
                       int iteration_count, double estimated_cost);

/* Print an ASCII table with headers and aligned columns.
 * columns: number of columns
 * headers: array of column header strings (NULL for no header)
 * rows: array of strings, each containing tab-separated column values
 * num_rows: number of rows
 * color: color for borders and headers
 */
void display_table(int columns, const char **headers,
                   const char **rows, int num_rows,
                   display_color_t color);

/* Word-wrap text to max_width columns. Returns malloc'd string (caller free). */
char *display_word_wrap(const char *text, int max_width);

/* Print a horizontal rule */
void display_hr(display_color_t color);

/* Check if terminal supports colors */
bool display_has_color(void);

/* Get terminal width */
int display_width(void);

/* Detect terminal color scheme. Returns true if dark theme (default), false if light.
 * Checks COLORFGBG, DARK_BG env vars. Defaults to dark. */
bool display_is_dark_theme(void);

/* ================================================================
 *  Tool failure / error display helpers (port of Python display.py)
 * ================================================================ */

/* Trim an error message for inline display — strip long paths, truncate.
 * Port of Python display.py:_trim_error(). */
void display_trim_error(const char *msg, char *out_buf, size_t out_sz);

/* Inspect a tool result string for signs of failure.
 * Returns true if failure detected, populates suffix_buf with short tag
 * like " [exit 1]" or " [full]" or " [error]".
 * Port of Python display.py:_detect_tool_failure(). */
bool display_detect_tool_failure(const char *tool_name, const char *result,
                                  char *suffix_buf, size_t suffix_sz);

/* ================================================================
 *  Tool preview max length (port of Python display.py)
 * ================================================================ */

/* Set the global max length for tool call previews. 0 = no limit.
 * Port of Python display.py:set_tool_preview_max_len(). */
void set_tool_preview_max_len(int n);

/* Return the configured max preview length (0 = unlimited).
 * Port of Python display.py:get_tool_preview_max_len(). */
int get_tool_preview_max_len(void);

/* ================================================================
 *  Additional display.py ports (module-level helpers)
 * ================================================================ */

/* Collapse whitespace (newlines, tabs) to single spaces.
 * Port of Python display.py:_oneline(). Writes into out_buf of out_sz bytes. */
void display_oneline(const char *text, char *out_buf, size_t out_sz);

/* Get the tool output prefix character from the active skin.
 * Port of Python display.py:get_skin_tool_prefix(). Returns "┊" by default. */
const char *display_get_skin_tool_prefix(void);

/* Get the display emoji for a tool name.
 * Resolution: (1) skin tool_emojis override, (2) hardcoded defaults, (3) "⚡" fallback.
 * Port of Python display.py:get_tool_emoji(). Returns pointer to static string. */
const char *display_get_tool_emoji(const char *tool_name);

/* Conservatively detect whether a tool result JSON string represents success.
 * Checks for "error" field and "success" field.
 * Port of Python display.py:_result_succeeded(). */
bool display_result_succeeded(const char *result);

/* Generate a formatted tool completion line for CLI quiet mode.
 * Format: "| {emoji} {verb:9} {detail}  {duration}"
 * Port of Python display.py:get_cute_tool_message(). Returns malloc'd string (caller free). */
char *display_cute_tool_message(const char *tool_name, const char *args_json,
                                double duration, const char *result_json);

/* ================================================================
 *  Edit diff path helpers (port of Python display.py)
 * ================================================================ */

/* Resolve a possibly-relative path against cwd, expanding ~.
 * Port of Python display.py:_resolved_path(). Writes into resolved_buf. */
char *display_resolved_path(const char *path, char *resolved_buf, size_t buf_sz);

/* Read a file's UTF-8 content. Returns malloc'd string or NULL on error.
 * Port of Python display.py:_snapshot_text(). */
char *display_snapshot_text(const char *path);

/* Given an absolute path, return a cwd-relative version for display.
 * Port of Python display.py:_display_diff_path(). Returns malloc'd string. */
char *display_diff_path(const char *abs_path);

/* ─── Diff pipeline helpers (port of Python display.py) ─── */

/* Split a unified diff string into per-file sections.
 * Returns malloc'd JSON array of section strings, or NULL.
 * Port of Python display.py:_split_unified_diff_sections(). */
char *display_split_diff_sections(const char *diff);

/* Summarise rendered diff sections with file-count and line-count caps.
 * max_files=6, max_lines=100. Returns malloc'd JSON array of rendered lines.
 * Port of Python display.py:_summarize_rendered_diff_sections(). */
char *display_summarize_diff(const char *diff, int max_files, int max_lines);

/* Extract a unified diff from a file-edit tool result JSON string.
 * Checks "patch" → "diff" field, or generates from pre/post snapshot.
 * Returns malloc'd diff text, or NULL. Caller must free().
 * Port of Python display.py:extract_edit_diff(). */
char *display_extract_edit_diff(const char *tool_name, const char *result_json,
                                 const char *function_args_json,
                                 const char *pre_content, const char *post_content);

/**
 * Port of Python display.py:_emit_inline_diff().
 * Emit rendered diff text through the provided print function.
 * @param diff_text The diff text to emit (already rendered, one per line).
 * @param print_fn Function pointer to call with each line (NULL-safe).
 * @return true if any output was emitted.
 */
bool display_emit_inline_diff(const char *diff_text, void (*print_fn)(const char *));

/**
 * Port of Python display.py:render_edit_diff_with_delta().
 * Render an edit diff inline without taking over the terminal UI.
 * Extracts diff via display_extract_edit_diff(), renders via display_summarize_diff()
 * and emits via display_emit_inline_diff().
 * @return true if any output was emitted.
 */
bool display_render_edit_diff(const char *tool_name, const char *result_json,
                              const char *function_args_json,
                              const char *pre_content, const char *post_content,
                              void (*print_fn)(const char *));

/* ================================================================
 *  LocalEditSnapshot — pre-tool filesystem snapshot
 *  Port of Python display.py LocalEditSnapshot dataclass
 * ================================================================ */

#define DISPLAY_MAX_SNAPSHOT_PATHS 32

/**
 * Snapshot type for capture/diff workflow
 * Port of Python display.py LocalEditSnapshot dataclass
 */
typedef struct display_local_edit_snapshot_t {
    char *paths[DISPLAY_MAX_SNAPSHOT_PATHS];
    char *before[DISPLAY_MAX_SNAPSHOT_PATHS];
    int count;
} display_local_edit_snapshot_t;

/* Create a new empty snapshot */
display_local_edit_snapshot_t *display_snapshot_create(void);

/* Free a snapshot and its contents */
void display_snapshot_free(display_local_edit_snapshot_t *snap);

/* Resolve local edit paths for a tool and populate snapshot.
 * Returns number of paths added.
 * Port of Python display.py:_resolve_local_edit_paths(). */
int display_snapshot_resolve_paths(display_local_edit_snapshot_t *snap,
                                    const char *tool_name,
                                    const char *function_args_json);

/* Capture before-state for local write previews.
 * Returns malloc'd snapshot or NULL if no paths to track.
 * Port of Python display.py:capture_local_edit_snapshot(). */
display_local_edit_snapshot_t *display_capture_local_edit_snapshot(const char *tool_name,
                                                                    const char *function_args_json);

/* Generate unified diff from a snapshot's before-state vs current files.
 * Returns malloc'd diff string or NULL.
 * Port of Python display.py:_diff_from_snapshot(). */
char *display_diff_from_snapshot(display_local_edit_snapshot_t *snap);

/* Port of Python display.py:KawaiiSpinner.get_waiting_faces().
 * Return waiting faces from skin spinner config, falling back to KAWAII_WAITING.
 * out_count receives the number of faces. */
const char **display_get_waiting_faces(int *out_count);

/* Port of Python display.py:KawaiiSpinner.get_thinking_faces().
 * Return thinking faces from skin spinner config, falling back to KAWAII_THINKING.
 * out_count receives the number of faces. */
const char **display_get_thinking_faces(int *out_count);

/* Port of Python display.py:KawaiiSpinner.get_thinking_verbs().
 * Return thinking verbs from skin spinner config, falling back to THINKING_VERBS.
 * out_count receives the number of verbs. */
const char **display_get_thinking_verbs(int *out_count);

/* Port of Python agent/insights.py:_bar_chart(). Generate horizontal bar chart
 * strings from integer values. Returns array of n malloc'd strings (caller
 * must free each string and the array via display_bar_chart_free). */
char **display_bar_chart(const int *values, int n, int max_width);
void display_bar_chart_free(char **bars, int n);

#ifdef __cplusplus
}
#endif

/** @} */ /* end of hermes_display group */
#endif /* HERMES_DISPLAY_H */
