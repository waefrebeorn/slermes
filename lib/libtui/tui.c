/*
 * tui.c — Terminal UI library for C (input + display).
 * MIT License — WuBu Hermes Project
 */

#define _GNU_SOURCE
#include "tui.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

/* ====================================================================
 *  HISTORY
 * ==================================================================== */

#define HISTORY_MAX 1024

typedef struct {
    char **lines;
    size_t count;
    size_t capacity;
} history_t;

static history_t *history_new(void) {
    history_t *h = (history_t *)calloc(1, sizeof(history_t));
    if (!h) return NULL;
    h->capacity = 64;
    h->lines = (char **)calloc(h->capacity, sizeof(char *));
    if (!h->lines) { free(h); return NULL; }
    return h;
}

static void history_add(history_t *h, const char *line) {
    if (!h || !line || !*line) return;
    /* Don't add duplicate of last entry */
    if (h->count > 0 && strcmp(h->lines[h->count - 1], line) == 0) return;
    if (h->count >= h->capacity) {
        h->capacity *= 2;
        h->lines = (char **)realloc(h->lines, h->capacity * sizeof(char *));
        if (!h->lines) return;
    }
    if (h->count >= HISTORY_MAX) {
        free(h->lines[0]);
        memmove(h->lines, h->lines + 1, (h->count - 1) * sizeof(char *));
        h->count--;
    }
    h->lines[h->count++] = strdup(line);
}

/* ====================================================================
 *  INPUT (replaces prompt_toolkit)
 * ==================================================================== */

struct tui_input_t {
    char   prompt[256];
    bool   echo;
    history_t *history;
    struct termios orig_term;
    bool   term_saved;
};

tui_input_t *tui_input_new(const char *prompt) {
    tui_input_t *in = (tui_input_t *)calloc(1, sizeof(tui_input_t));
    if (!in) return NULL;
    if (prompt) strncpy(in->prompt, prompt, sizeof(in->prompt) - 1);
    in->echo = true;
    in->history = history_new();
    return in;
}

/* Set terminal to raw mode */
static bool raw_mode(tui_input_t *in) {
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &in->orig_term) < 0) return false;
    in->term_saved = true;
    raw = in->orig_term;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    return tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0;
}

/* Restore terminal */
static void restore_mode(tui_input_t *in) {
    if (in->term_saved)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &in->orig_term);
    in->term_saved = false;
}

char *tui_input_read_history(tui_input_t *in) {
    if (!in) return NULL;

    printf("%s", in->prompt);
    fflush(stdout);

    if (!raw_mode(in)) {
        /* Fallback: simple line input */
        char *buf = NULL;
        size_t n = 0;
        ssize_t len = getline(&buf, &n, stdin);
        if (len < 0) { free(buf); return NULL; }
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';

        if (in->echo) {
            history_add(in->history, buf);
        }
        return buf;
    }

    /* Line editor buffer */
    size_t bufsz = 256, pos = 0;
    char *buf = (char *)calloc(bufsz, 1);
    if (!buf) { restore_mode(in); return NULL; }

    int hist_idx = (int)in->history->count;  /* current = end (new) */

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) break;

        if (c == '\n' || c == '\r') {
            printf("\n");
            break;
        }

        if (c == 127 || c == '\b') { /* backspace */
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if (c == 27) { /* escape sequence */
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') { /* Up arrow — history back */
                    if (hist_idx > 0) {
                        /* Erase current line */
                        while (pos > 0) { printf("\b \b"); pos--; }
                        fflush(stdout);
                        hist_idx--;
                        strncpy(buf, in->history->lines[hist_idx], bufsz - 1);
                        pos = strlen(buf);
                        printf("%s", buf);
                        fflush(stdout);
                    }
                } else if (seq[1] == 'B') { /* Down arrow — history forward */
                    if (hist_idx < (int)in->history->count - 1) {
                        while (pos > 0) { printf("\b \b"); pos--; }
                        fflush(stdout);
                        hist_idx++;
                        strncpy(buf, in->history->lines[hist_idx], bufsz - 1);
                        pos = strlen(buf);
                        printf("%s", buf);
                        fflush(stdout);
                    } else if (hist_idx == (int)in->history->count - 1) {
                        /* At end — clear */
                        while (pos > 0) { printf("\b \b"); pos--; }
                        fflush(stdout);
                        hist_idx = (int)in->history->count;
                        buf[0] = '\0';
                        pos = 0;
                    }
                }
            }
            continue;
        }

        /* Regular character */
        if (pos + 1 >= bufsz) {
            bufsz *= 2;
            char *nb = (char *)realloc(buf, bufsz);
            if (!nb) break;
            buf = nb;
        }
        buf[pos++] = c;
        buf[pos] = '\0';
        if (in->echo) {
            putchar(c);
            fflush(stdout);
        }
    }

    restore_mode(in);
    if (pos == 0) { free(buf); return NULL; }
    if (in->echo) history_add(in->history, buf);
    return buf;
}

char *tui_input_read(tui_input_t *in) {
    return tui_input_read_history(in);
}

void tui_input_history_add(tui_input_t *in, const char *line) {
    if (in) history_add(in->history, line);
}

const char *tui_input_history_get(const tui_input_t *in, size_t idx) {
    if (!in || !in->history || idx >= in->history->count) return NULL;
    return in->history->lines[idx];
}

size_t tui_input_history_size(const tui_input_t *in) {
    return in && in->history ? in->history->count : 0;
}

void tui_input_set_echo(tui_input_t *in, bool echo) { if (in) in->echo = echo; }
void tui_input_set_prompt(tui_input_t *in, const char *prompt) {
    if (in && prompt) strncpy(in->prompt, prompt, sizeof(in->prompt) - 1);
}

void tui_input_free(tui_input_t *in) {
    if (!in) return;
    if (in->history) {
        for (size_t i = 0; i < in->history->count; i++)
            free(in->history->lines[i]);
        free(in->history->lines);
        free(in->history);
    }
    if (in->term_saved) restore_mode(in);
    free(in);
}

/* ====================================================================
 *  DISPLAY
 * ==================================================================== */

/* ANSI codes */
const char *tui_fg(tui_color_t c) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "\033[%dm", 30 + (int)c);
    return buf;
}
const char *tui_bg(tui_color_t c) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "\033[%dm", 40 + (int)c);
    return buf;
}
const char *tui_bold(void) { return "\033[1m"; }
const char *tui_dim(void) { return "\033[2m"; }
const char *tui_reset(void) { return "\033[0m"; }

void tui_printf(tui_color_t color, const char *fmt, ...) {
    printf("%s", tui_fg(color));
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("%s", tui_reset());
}

void tui_printfln(tui_color_t color, const char *fmt, ...) {
    printf("%s", tui_fg(color));
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("%s\n", tui_reset());
}

/* ====================================================================
 *  SPINNER
 * ==================================================================== */

struct tui_spinner_t {
    char label[128];
    int frame;
    bool done;
};

tui_spinner_t *tui_spinner_new(const char *label) {
    tui_spinner_t *s = (tui_spinner_t *)calloc(1, sizeof(tui_spinner_t));
    if (!s) return NULL;
    if (label) strncpy(s->label, label, sizeof(s->label) - 1);
    s->frame = 0;
    s->done = false;
    return s;
}

void tui_spinner_tick(tui_spinner_t *s) {
    if (!s || s->done) return;
    static const char *frames = "|/-\\";
    printf("\r%s %c", s->label, frames[s->frame]);
    fflush(stdout);
    s->frame = (s->frame + 1) % 4;
}

void tui_spinner_done(tui_spinner_t *s, const char *final_msg) {
    if (!s) return;
    s->done = true;
    printf("\r\x1b[K"); /* clear line */
    if (final_msg) printf("%s\n", final_msg);
    else printf("%s done.\n", s->label);
}

void tui_spinner_free(tui_spinner_t *s) {
    if (!s) return;
    if (!s->done) tui_spinner_done(s, NULL);
    free(s);
}

/* ====================================================================
 *  PROGRESS BAR (replaces tqdm)
 * ==================================================================== */

struct tui_progress_t {
    char label[128];
    int total;
    int current;
    bool done;
    struct timeval start;
};

tui_progress_t *tui_progress_new(const char *label, int total) {
    tui_progress_t *p = (tui_progress_t *)calloc(1, sizeof(tui_progress_t));
    if (!p) return NULL;
    if (label) strncpy(p->label, label, sizeof(p->label) - 1);
    p->total = total > 0 ? total : 1;
    p->current = 0;
    p->done = false;
    gettimeofday(&p->start, NULL);
    tui_progress_update(p, 0);
    return p;
}

void tui_progress_update(tui_progress_t *p, int current) {
    if (!p || p->done) return;
    p->current = current;

    int bar_width = 30;
    int pct = current * 100 / p->total;
    int filled = current * bar_width / p->total;

    /* Elapsed time */
    struct timeval now;
    gettimeofday(&now, NULL);
    double elapsed = (now.tv_sec - p->start.tv_sec)
                   + (now.tv_usec - p->start.tv_usec) / 1e6;

    printf("\r%s: [", p->label);
    for (int i = 0; i < bar_width; i++)
        putchar(i < filled ? '=' : (i == filled ? '>' : ' '));
    printf("] %3d%%  %6.1fs", pct, elapsed);
    fflush(stdout);
}

void tui_progress_done(tui_progress_t *p) {
    if (!p || p->done) return;
    p->done = true;
    tui_progress_update(p, p->total);
    printf("\n");
}

void tui_progress_free(tui_progress_t *p) {
    if (!p) return;
    if (!p->done) tui_progress_done(p);
    free(p);
}

/* ====================================================================
 *  BOX
 * ==================================================================== */

void tui_box(const char *title, const char *content, int width) {
    if (width <= 0) width = 50;

    /* Top line */
    printf("+");
    if (title && *title) {
        printf(" %s ", title);
        int remain = width - (int)strlen(title) - 3;
        for (int i = 0; i < remain; i++) printf("-");
    } else {
        for (int i = 0; i < width; i++) printf("-");
    }
    printf("+\n");

    /* Content — split by newlines */
    const char *p = content;
    const char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        printf("| %.*s|\n", (int)(nl - p), p);
        p = nl + 1;
    }
    if (*p) printf("| %s|\n", p);

    /* Bottom line */
    printf("+");
    for (int i = 0; i < width; i++) printf("-");
    printf("+\n");
}

/* ====================================================================
 *  RULER
 * ==================================================================== */

void tui_ruler(const char *label, tui_color_t color, int width) {
    if (width <= 0) width = 60;
    printf("%s", tui_fg(color));
    if (label && *label) {
        int lbl_len = (int)strlen(label) + 2;
        int side = (width - lbl_len) / 2;
        for (int i = 0; i < side; i++) printf("-");
        printf(" %s ", label);
        for (int i = width - side - lbl_len; i > 0; i--) printf("-");
    } else {
        for (int i = 0; i < width; i++) printf("-");
    }
    printf("%s\n", tui_reset());
}

/* ====================================================================
 *  TABLE
 * ==================================================================== */

void tui_table(const char *headers[], const char *rows[],
               int cols, int rows_count) {
    if (!headers || !rows || cols <= 0) return;

    /* Calculate column widths */
    int *widths = (int *)calloc((size_t)cols, sizeof(int));
    if (!widths) return;
    for (int c = 0; c < cols; c++)
        if (headers[c]) widths[c] = (int)strlen(headers[c]);

    for (int r = 0; r < rows_count; r++) {
        for (int c = 0; c < cols; c++) {
            int len = (int)strlen(rows[r * cols + c]);
            if (len > widths[c]) widths[c] = len;
        }
    }

    /* Header separator */
    printf("+");
    for (int c = 0; c < cols; c++) {
        for (int i = 0; i < widths[c] + 2; i++) printf("-");
        printf("+");
    }
    printf("\n");

    /* Headers */
    printf("|");
    for (int c = 0; c < cols; c++) {
        printf(" %-*s |", widths[c], headers[c] ? headers[c] : "");
    }
    printf("\n");

    /* Separator */
    printf("+");
    for (int c = 0; c < cols; c++) {
        for (int i = 0; i < widths[c] + 2; i++) printf("-");
        printf("+");
    }
    printf("\n");

    /* Rows */
    for (int r = 0; r < rows_count; r++) {
        printf("|");
        for (int c = 0; c < cols; c++) {
            const char *val = rows[r * cols + c] ? rows[r * cols + c] : "";
            printf(" %-*s |", widths[c], val);
        }
        printf("\n");
    }

    /* Bottom */
    printf("+");
    for (int c = 0; c < cols; c++) {
        for (int i = 0; i < widths[c] + 2; i++) printf("-");
        printf("+");
    }
    printf("\n");

    free(widths);
}

/* ====================================================================
 *  TOOL CALL VISUALIZATION (TU04)
 *  General tool call display — started/completed/failed events with
 *  colored output, emoji prefixes, and compact arg previews.
 *  Mirrors Python CLI's display_tool_activity() + display_inline_diff().
 * ==================================================================== */

/* Emoji map — hardcoded defaults per tool (matches CLI display_core.c) */
static const char *tool_emoji(const char *tool_name) {
    if (!tool_name) return "⚡";
    if (strcmp(tool_name, "terminal") == 0) return "$ ";
    if (strcmp(tool_name, "write_file") == 0) return "📝";
    if (strcmp(tool_name, "read_file") == 0) return "📖";
    if (strcmp(tool_name, "patch") == 0) return "🩹";
    if (strcmp(tool_name, "web_search") == 0) return "🔍";
    if (strcmp(tool_name, "search_files") == 0) return "🔎";
    if (strcmp(tool_name, "execute_code") == 0) return "🐍";
    if (strcmp(tool_name, "delegate_task") == 0) return "📋";
    if (strcmp(tool_name, "vision_analyze") == 0) return "👁️";
    if (strcmp(tool_name, "image_generate") == 0) return "🎨";
    if (strcmp(tool_name, "text_to_speech") == 0) return "🔊";
    if (strcmp(tool_name, "send_message") == 0) return "📤";
    if (strcmp(tool_name, "memory") == 0) return "🧠";
    if (strcmp(tool_name, "session_search") == 0) return "📚";
    if (strcmp(tool_name, "skill_view") == 0 || strcmp(tool_name, "skill_manage") == 0) return "🛠️";
    if (strcmp(tool_name, "cronjob") == 0) return "⏰";
    if (strcmp(tool_name, "todo") == 0) return "✅";
    if (strcmp(tool_name, "clarify") == 0) return "❓";
    if (strcmp(tool_name, "browser_navigate") == 0 ||
        strcmp(tool_name, "browser_click") == 0 ||
        strcmp(tool_name, "browser_type") == 0) return "🌐";
    return "⚡";
}

/* Preview builder — extract primary arg from tool call JSON.
 * Mirrors display_tool_preview() in display_core.c.
 * Returns malloc'd string (caller must free) or NULL. */
char *tui_tool_preview(const char *tool_name, const char *args_json) {
    if (!tool_name || !args_json) return NULL;

    /* Parse JSON args */
    json_t *args = json_parse(args_json, NULL);
    if (!args || args->type != JSON_OBJECT) {
        json_free(args);
        return NULL;
    }

    const char *preview = NULL;
    char buf[512];

    /* Primary arg key per tool (matches CLI mapping) */
    if (strcmp(tool_name, "terminal") == 0)
        preview = json_get_str(args, "command", NULL);
    else if (strcmp(tool_name, "write_file") == 0)
        preview = json_get_str(args, "file_path", NULL);
    else if (strcmp(tool_name, "read_file") == 0)
        preview = json_get_str(args, "file_path", NULL);
    else if (strcmp(tool_name, "patch") == 0)
        preview = json_get_str(args, "path", NULL);
    else if (strcmp(tool_name, "web_search") == 0)
        preview = json_get_str(args, "query", NULL);
    else if (strcmp(tool_name, "search_files") == 0)
        preview = json_get_str(args, "pattern", NULL);
    else if (strcmp(tool_name, "execute_code") == 0)
        preview = json_get_str(args, "code", NULL);
    else if (strcmp(tool_name, "delegate_task") == 0)
        preview = json_get_str(args, "task", NULL);
    else if (strcmp(tool_name, "vision_analyze") == 0)
        preview = json_get_str(args, "image_url", NULL);
    else if (strcmp(tool_name, "memory") == 0)
        preview = json_get_str(args, "query", NULL);
    else if (strcmp(tool_name, "session_search") == 0)
        preview = json_get_str(args, "query", NULL);
    else if (strcmp(tool_name, "send_message") == 0)
        preview = json_get_str(args, "message", NULL);
    else if (strcmp(tool_name, "cronjob") == 0)
        preview = json_get_str(args, "prompt", NULL);
    else if (strcmp(tool_name, "todo") == 0)
        preview = json_get_str(args, "content", NULL);
    else {
        /* Generic: try "query" then "path" then "text" */
        preview = json_get_str(args, "query", NULL);
        if (!preview) preview = json_get_str(args, "path", NULL);
        if (!preview) preview = json_get_str(args, "text", NULL);
    }

    char *result = NULL;
    if (preview) {
        /* Truncate long previews */
        size_t len = strlen(preview);
        if (len > 80) {
            snprintf(buf, sizeof(buf), "%.77s...", preview);
            result = strdup(buf);
        } else {
            result = strdup(preview);
        }
    }

    json_free(args);
    return result;
}

/* Display a tool call event line.
 * Mirrors display_tool_activity() in display_core.c.
 * state: TUI_TOOL_STARTED (cyan+bold), TUI_TOOL_COMPLETED (green+bold), TUI_TOOL_FAILED (red+bold) */
void tui_tool_call(const char *tool_name, tui_tool_state_t state, const char *preview) {
    if (!tool_name) return;

    const char *emoji = tool_emoji(tool_name);
    tui_color_t color;
    const char *status_icon = "";

    switch (state) {
        case TUI_TOOL_STARTED:
            color = TUI_CYAN;
            break;
        case TUI_TOOL_COMPLETED:
            color = TUI_GREEN;
            status_icon = "✓ ";
            break;
        case TUI_TOOL_FAILED:
            color = TUI_RED;
            status_icon = "✗ ";
            break;
        default:
            color = TUI_DEFAULT;
            break;
    }

    /* Output: "  ⚡ tool_name  preview" */
    printf("  %s ", emoji);
    printf("%s", tui_fg(color));
    printf("%s", tui_bold());
    printf("%s%s", status_icon, tool_name);
    printf("%s", tui_reset());
    if (preview) {
        printf(" ");
        printf("%s", tui_fg(TUI_WHITE));
        printf("%s", tui_dim());
        printf("%s", preview);
        printf("%s", tui_reset());
    }
    printf("\n");
    fflush(stdout);
}

/* Inline diff rendering — colored unified diff (TU04).
 * Mirrors display_inline_diff() in display_core.c.
 * + lines: green bg, - lines: red bg, @@ lines: dim purple, context: dim gray.
 * Returns malloc'd string (caller must free) or NULL. */
char *tui_inline_diff(const char *diff_text) {
    if (!diff_text) return NULL;

    size_t in_len = strlen(diff_text);
    size_t cap = in_len * 3 + 4096; /* ANSI expansion headroom */
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t pos = 0;

    const char *p = diff_text;
    const char *nl;
    while ((nl = strchr(p, '\n')) != NULL) {
        size_t line_len = (size_t)(nl - p);
        char line[4096];
        size_t lcpy = line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
        memcpy(line, p, lcpy);
        line[lcpy] = '\0';

        if (line_len > 0 && line[0] == '+') {
            pos += snprintf(out + pos, cap - pos,
                "%s%s%s\n", tui_bg(TUI_GREEN), line, tui_reset());
        } else if (line_len > 0 && line[0] == '-') {
            pos += snprintf(out + pos, cap - pos,
                "%s%s%s\n", tui_bg(TUI_RED), line, tui_reset());
        } else if (line_len > 1 && line[0] == '@' && line[1] == '@') {
            pos += snprintf(out + pos, cap - pos,
                "%s%s%s\n", tui_dim(), line, tui_reset());
        } else if (line_len > 0 && line[0] == ' ') {
            pos += snprintf(out + pos, cap - pos,
                "%s%s\n", tui_dim(), line);
        } else {
            pos += snprintf(out + pos, cap - pos, "%s\n", line);
        }

        if (pos >= cap - 256) break; /* safety */
        p = nl + 1;
    }

    /* Handle last line without trailing newline */
    if (*p && pos < cap - 256) {
        pos += snprintf(out + pos, cap - pos, "%s\n", p);
    }

    return out;
}

/* ====================================================================
 *  SETTINGS PANEL (TU05)
 *  Navigable settings UI panel for viewing/editing configuration.
 *  Mirrors CLI's /config show <section> output in a TUI-friendly format.
 * ==================================================================== */

/* Render a settings section as a navigable panel.
 * Draws a box with the section title, lists key=value entries,
 * highlights the selected entry, and shows scroll indicators.
 *
 * section    — the settings section to render
 * width      — panel width (0 for auto=60)
 * height     — visible height (0 for auto=20)
 */
void tui_settings_panel(const tui_settings_section_t *section, int width, int height) {
    if (!section || section->entry_count == 0) return;
    if (width <= 0) width = 60;
    if (height <= 0) height = 20;

    int content_width = width - 4; /* account for borders */

    /* Top border with title */
    printf("%s", tui_fg(TUI_CYAN));
    printf("+");
    if (section->title[0]) {
        int title_len = (int)strlen(section->title) + 2;
        int side = (width - title_len - 2) / 2;
        for (int i = 0; i < side; i++) printf("-");
        printf(" %s ", section->title);
        for (int i = width - side - title_len - 2; i > 0; i--) printf("-");
    } else {
        for (int i = 0; i < width - 2; i++) printf("-");
    }
    printf("+%s\n", tui_reset());

    /* Description line */
    if (section->description[0]) {
        printf("%s|%s %-.*s%s%s |\n", tui_fg(TUI_CYAN), tui_reset(),
               content_width, section->description,
               (int)strlen(section->description) > content_width ? "..." : "",
               tui_fg(TUI_CYAN));
        printf("+");
        for (int i = 0; i < width - 2; i++) printf("-");
        printf("+%s\n", tui_reset());
    }

    /* Entries */
    int visible = height - 4; /* reserve space for borders + description */
    if (section->description[0]) visible -= 2;
    if (visible < 1) visible = 1;

    int start = section->scroll_offset;
    int end = start + visible;
    if (end > section->entry_count) end = section->entry_count;
    if (start > 0) {
        printf("%s|%s  ... %d more above ...%s%*s%s|\n",
               tui_fg(TUI_CYAN), tui_dim(),
               start, tui_reset(),
               content_width - 22 - (start >= 10 ? 1 : 0) - (start >= 100 ? 1 : 0), "",
               tui_fg(TUI_CYAN));
        int skip = (start < 2) ? start : 2;
        start = start - skip;
        visible -= 1;
        if (start < 0) start = 0;
    }

    for (int i = start; i < end && i < start + visible; i++) {
        const tui_setting_entry_t *e = &section->entries[i];
        char line[512];
        int line_len = snprintf(line, sizeof(line), "%s=%s", e->key, e->value);

        if (i == section->selected_idx) {
            /* Highlighted entry */
            printf("%s|%s %s>%-*s<%s%s|\n",
                   tui_fg(TUI_CYAN), tui_reset(),
                   tui_bold(), content_width - 1, line,
                   tui_reset(), tui_fg(TUI_CYAN));
        } else {
            printf("%s|%s  %-.*s%s%s|\n",
                   tui_fg(TUI_CYAN), tui_reset(),
                   content_width, line,
                   line_len > content_width ? "..." : "",
                   tui_fg(TUI_CYAN));
        }
    }

    /* Fill remaining visible area */
    int shown = end - start;
    for (int i = shown; i < visible; i++) {
        printf("%s|%s%*s%s|\n", tui_fg(TUI_CYAN), tui_reset(),
               content_width, "", tui_fg(TUI_CYAN));
    }

    /* Bottom border with scroll info */
    printf("%s+", tui_fg(TUI_CYAN));
    int remaining = section->entry_count - end;
    if (remaining > 0) {
        char scroll_info[64];
        snprintf(scroll_info, sizeof(scroll_info), " %d/%d (%d more) ",
                 section->selected_idx + 1, section->entry_count, remaining);
        int info_len = (int)strlen(scroll_info);
        int side = (width - 2 - info_len) / 2;
        for (int i = 0; i < side; i++) printf("-");
        printf("%s", scroll_info);
        for (int i = width - 2 - side - info_len; i > 0; i--) printf("-");
    } else {
        for (int i = 0; i < width - 2; i++) printf("-");
    }
    printf("+%s\n", tui_reset());

    /* Selected entry description */
    if (section->selected_idx >= 0 && section->selected_idx < section->entry_count) {
        const tui_setting_entry_t *e = &section->entries[section->selected_idx];
        if (e->description[0]) {
            printf("%s  %s%s\n", tui_dim(), e->description, tui_reset());
        }
    }
}

/* Convenience: create a settings section from a config struct.
 * Populates entries for the "model" section.
 * Returns number of entries filled. */
int tui_settings_fill_model(tui_setting_entry_t *entries, int max_entries) {
    if (!entries || max_entries <= 0) return 0;
    int n = 0;
    if (n < max_entries) {
        snprintf(entries[n].key, sizeof(entries[n].key), "model");
        snprintf(entries[n].value, sizeof(entries[n].value), "(active model)");
        snprintf(entries[n].description, sizeof(entries[n].description),
                 "Active LLM model. Use /model set <name> to change.");
        entries[n].editable = true;
        n++;
    }
    if (n < max_entries) {
        snprintf(entries[n].key, sizeof(entries[n].key), "provider");
        snprintf(entries[n].value, sizeof(entries[n].value), "(active provider)");
        snprintf(entries[n].description, sizeof(entries[n].description),
                 "Active LLM provider. Use /model set <provider>/<model>.");
        entries[n].editable = true;
        n++;
    }
    if (n < max_entries) {
        snprintf(entries[n].key, sizeof(entries[n].key), "parallel_tool_calls");
        snprintf(entries[n].value, sizeof(entries[n].value), "true");
        snprintf(entries[n].description, sizeof(entries[n].description),
                 "Allow multiple tool calls in parallel.");
        entries[n].editable = true;
        n++;
    }
    if (n < max_entries) {
        snprintf(entries[n].key, sizeof(entries[n].key), "max_tool_calls_round");
        snprintf(entries[n].value, sizeof(entries[n].value), "0 (unlimited)");
        snprintf(entries[n].description, sizeof(entries[n].description),
                 "Max tool calls per agent turn. 0 = unlimited.");
        entries[n].editable = true;
        n++;
    }
    return n;
}
