/*
 * display_core.c — Terminal display for Hermes C.
 * ANSI escape codes only. No ncurses dependency.
 * Provides: display_init, display_reset, display_printf, progress, spinner, panel.
 */

#include "hermes_display.h"
#include "hermes_json.h"
#include "skin.h"
#include "ansi.h"
#include "display_diff.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <libgen.h>
#include <errno.h>
#include "hermes_tokenizer.h"
#include "hermes_tool_result.h"

/* ================================================================
 *  State
 * ================================================================ */

static bool is_tty = false;
skin_t *g_display_skin = NULL;  /* active skin for display styling */

#define ANSI_ESC "\x1B["

/* ================================================================
 *  Syntax highlighting
 * ================================================================ */

/* Port of Python: rich syntax highlighting for code in CLI output.
 * Wraps inline code (backticks) with ANSI yellow for inline,
 * and code blocks (``` ```) with ANSI cyan foreground.
 * Caller must free the returned string. */
char *display_highlight_code(const char *text) {
    if (!text || !is_tty) return strdup(text ? text : "");

    size_t len = strlen(text);
    char *out = (char*)malloc(len * 2 + 256);
    if (!out) return strdup(text);

    size_t pos = 0;
    bool in_code_block = false;
    bool in_inline_code = false;

    for (size_t i = 0; i < len; i++) {
        if (!in_inline_code && i + 2 < len && text[i] == '`' && text[i+1] == '`' && text[i+2] == '`') {
            if (in_code_block) {
                pos += snprintf(out + pos, len * 2 + 256 - pos, ANSI_ESC "0m");
                in_code_block = false;
            } else {
                pos += snprintf(out + pos, len * 2 + 256 - pos, ANSI_ESC "36m");
                in_code_block = true;
            }
            i += 2;
            continue;
        }
        if (!in_code_block && text[i] == '`') {
            if (in_inline_code) {
                pos += snprintf(out + pos, len * 2 + 256 - pos, ANSI_ESC "0m");
                in_inline_code = false;
            } else {
                pos += snprintf(out + pos, len * 2 + 256 - pos, ANSI_ESC "33m");
                in_inline_code = true;
            }
            continue;
        }
        out[pos++] = text[i];
        if (pos >= len * 2 + 200) break;
    }
    if (in_code_block || in_inline_code)
        pos += snprintf(out + pos, len * 2 + 256 - pos, ANSI_ESC "0m");
    out[pos] = '\0';
    return out;
}

/* ================================================================
 *  Initialization
 * ================================================================ */

void display_init(void) {
    is_tty = isatty(STDOUT_FILENO);
    /* V21: NO_COLOR env var or TERM=dumb disables color */
    if (is_tty) {
        const char *no_color = getenv("NO_COLOR");
        if (no_color) // NO_COLOR set = disable colors (per no-color.org spec)
            is_tty = 0;
        const char *term = getenv("TERM");
        if (term && strcmp(term, "dumb") == 0)
            is_tty = 0;
    }
    if (is_tty) {
        /* Enable alternate screen? No, keep it simple */
    }
}

void display_reset(void) {
    if (is_tty) {
        printf(ANSI_ESC "0m");
        fflush(stdout);
    }
}

/* ================================================================
 *  Color + Style
 * ================================================================ */

void display_set_fg(display_color_t color) {
    if (!is_tty) return;
    if (color == DISPLAY_DEFAULT)
        printf(ANSI_ESC "39m");
    else
        printf(ANSI_ESC "9%dm", (int)color);
    fflush(stdout);
}

void display_set_bg(display_color_t color) {
    if (!is_tty) return;
    if (color == DISPLAY_DEFAULT)
        printf(ANSI_ESC "49m");
    else
        printf(ANSI_ESC "10%dm", (int)color);
    fflush(stdout);
}

/* Truecolor (24-bit) support */
void display_set_fg_rgb(int r, int g, int b) {
    if (!is_tty) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    printf(ANSI_ESC "38;2;%d;%d;%dm", r, g, b);
    fflush(stdout);
}

/* 256-color palette foreground */
void display_set_fg_256(int color) {
    if (!is_tty) return;
    if (color < 0) color = 0;
    if (color > 255) color = 255;
    printf(ANSI_ESC "38;5;%dm", color);
    fflush(stdout);
}

/* 256-color palette background */
void display_set_bg_256(int color) {
    if (!is_tty) return;
    if (color < 0) color = 0;
    if (color > 255) color = 255;
    printf(ANSI_ESC "48;5;%dm", color);
    fflush(stdout);
}

void display_set_bg_rgb(int r, int g, int b) {
    if (!is_tty) return;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    printf(ANSI_ESC "48;2;%d;%d;%dm", r, g, b);
    fflush(stdout);
}

void display_set_style(display_style_t style) {
    if (!is_tty) return;
    if (style & DISPLAY_BOLD)
        printf(ANSI_ESC "1m");
    if (style & DISPLAY_DIM)
        printf(ANSI_ESC "2m");
    if (style & DISPLAY_ITALIC)
        printf(ANSI_ESC "3m");
    if (style & DISPLAY_UNDERLINE)
        printf(ANSI_ESC "4m");
    fflush(stdout);
}

/* ================================================================
 *  Printf with color
 * ================================================================ */

void display_printf(display_color_t color, display_style_t style,
                    const char *fmt, ...)
{
    if (!is_tty) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }

    display_set_fg(color);
    display_set_style(style);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    display_reset();
}

/* Print with 24-bit truecolor foreground */
void display_printf_hex(const char *hex_fg, display_style_t style,
                         const char *fmt, ...)
{
    if (!is_tty || !hex_fg) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
    int r, g, b;
    if (!ansi_parse_hex(hex_fg, &r, &g, &b)) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        fflush(stdout);
        return;
    }
    display_set_fg_rgb(r, g, b);
    display_set_style(style);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    display_reset();
}

/* ================================================================
 *  Cursor control
 * ================================================================ */

void display_clear(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "2J" ANSI_ESC "H");
    fflush(stdout);
}

void display_goto(int row, int col) {
    if (!is_tty) return;
    printf(ANSI_ESC "%d;%dH", row, col);
    fflush(stdout);
}

void display_save_pos(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "s");
    fflush(stdout);
}

void display_restore_pos(void) {
    if (!is_tty) return;
    printf(ANSI_ESC "u");
    fflush(stdout);
}

/* ================================================================
 *  Progress bar
 * ================================================================ */

void display_progress_init(display_progress_t *bar, const char *label, int total) {
    if (!bar) return;
    bar->current = 0;
    bar->total = total > 0 ? total : 100;
    bar->width = 40;
    snprintf(bar->label, sizeof(bar->label), "%s", label ? label : "");
    if (is_tty) {
        display_printf(DISPLAY_WHITE, DISPLAY_DIM, "%s: ", bar->label);
        fflush(stdout);
    }
}

void display_progress_update(display_progress_t *bar, int current) {
    if (!bar || !is_tty) return;
    bar->current = current;
    int pct = (bar->total > 0) ? (current * 100 / bar->total) : 0;
    int filled = (bar->width * current) / bar->total;
    printf("\r%s: [", bar->label);
    for (int i = 0; i < bar->width; i++) {
        if (i < filled) putchar('=');
        else if (i == filled) putchar('>');
        else putchar(' ');
    }
    printf("] %d%%", pct);
    fflush(stdout);
}

void display_progress_done(display_progress_t *bar) {
    if (!bar) return;
    if (is_tty) {
        display_progress_update(bar, bar->total);
        printf("\n");
    }
    fflush(stdout);
}

/* ================================================================
 *  Spinner
 * ================================================================ */

static const char SPINNER_CHARS[] = {'|', '/', '-', '\\'};

void display_spinner_start(display_spinner_t *sp, const char *label) {
    if (!sp) return;
    sp->frame = 0;
    sp->frame_count = 0;
    sp->active = true;
    sp->face = NULL;
    if (label) {
        sp->label = strdup(label);
    } else {
        sp->label = NULL;
    }
}

/* Port of Python display.py:KawaiiSpinner.tick().
/* AG26: Port of Python agent/display.py:_write() */
/* AG26: Port of Python agent/display.py:_is_tty() */
/* AG26: Port of Python agent/display.py:_is_patch_stdout_proxy() */
/* AG26: Port of Python agent/display.py:print_above() */
/* Advance spinner tick and print. */
void display_spinner_tick(display_spinner_t *sp) {
    printf("\r%c %s", SPINNER_CHARS[sp->frame % 4],
           sp->label ? sp->label : "");
    sp->frame++;
    sp->frame_count++;
    fflush(stdout);
}

void display_spinner_stop(display_spinner_t *sp, const char *done_msg) {
    if (!sp) return;
    sp->active = false;
    if (is_tty) {
        if (done_msg)
            printf("\r\xE2\x9C\x93 %s\n", done_msg);
        else
            printf("\r\xE2\x9C\x93 %s\n", sp->label ? sp->label : "done");
    }
    free(sp->label);
    sp->label = NULL;
    free(sp->face);
    sp->face = NULL;
    fflush(stdout);
}

/* PoP: display_spinner_enter @ agent/display.py:KawaiiSpinner.__enter__ */
/* Context-manager entry: start the spinner and return the spinner handle
 * (mirrors Python `with KawaiiSpinner(...) as sp:` returning self). */
display_spinner_t *display_spinner_enter(display_spinner_t *sp, const char *label) {
    display_spinner_start(sp, label);
    return sp;
}

/* PoP: display_spinner_exit @ agent/display.py:KawaiiSpinner.__exit__ */
/* Context-manager exit: stop the spinner. Returns 0 (Python __exit__ returns
 * False, i.e. does not suppress exceptions). */
int display_spinner_exit(display_spinner_t *sp) {
    display_spinner_stop(sp, NULL);
    return 0;
}

/* ================================================================
 *  Kawaii Spinner — animated faces for LLM wait
 * ================================================================ */

/* V11: Spinner frame sets — mirrors Python KawaiiSpinner.SPINNERS */
static const char *SPINNER_DOTS_FRAMES[] = {
    "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏",
};
static const int N_DOTS = 10;

static const char *SPINNER_BOUNCE_FRAMES[] = {
    "⠁", "⠂", "⠄", "⡀", "⢀", "⠠", "⠐", "⠈",
};
static const int N_BOUNCE = 8;

static const char *SPINNER_GROW_FRAMES[] = {
    "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█", "▇", "▆", "▅", "▄", "▃", "▂",
};
static const int N_GROW = 14;

static const char *SPINNER_ARROWS_FRAMES[] = {
    "←", "↖", "↑", "↗", "→", "↘", "↓", "↙",
};
static const int N_ARROWS = 8;

static const char *SPINNER_STAR_FRAMES[] = {
    "✶", "✷", "✸", "✹", "✺", "✹", "✸", "✷",
};
static const int N_STAR = 8;

static const char *SPINNER_MOON_FRAMES[] = {
    "🌑", "🌒", "🌓", "🌔", "🌕", "🌖", "🌗", "🌘",
};
static const int N_MOON = 8;

static const char *SPINNER_PULSE_FRAMES[] = {
    "◜", "◠", "◝", "◞", "◡", "◟",
};
static const int N_PULSE = 6;

static const char *SPINNER_BRAIN_FRAMES[] = {
    "🧠", "💭", "💡", "✨", "💫", "🌟", "💡", "💭",
};
static const int N_BRAIN = 8;

static const char *SPINNER_SPARKLE_FRAMES[] = {
    "⁺", "˚", "*", "✧", "✦", "✧", "*", "˚",
};
static const int N_SPARKLE = 8;

/* Get the current spinner frame character for the given type + index.
 * Returns the frame string, or NULL for kawaii face mode. */
static const char *spinner_get_frame(spinner_type_t type, int idx) {
    switch (type) {
        case SPINNER_DOTS:    return SPINNER_DOTS_FRAMES[idx % N_DOTS];
        case SPINNER_BOUNCE:  return SPINNER_BOUNCE_FRAMES[idx % N_BOUNCE];
        case SPINNER_GROW:    return SPINNER_GROW_FRAMES[idx % N_GROW];
        case SPINNER_ARROWS:  return SPINNER_ARROWS_FRAMES[idx % N_ARROWS];
        case SPINNER_STAR:    return SPINNER_STAR_FRAMES[idx % N_STAR];
        case SPINNER_MOON:    return SPINNER_MOON_FRAMES[idx % N_MOON];
        case SPINNER_PULSE:   return SPINNER_PULSE_FRAMES[idx % N_PULSE];
        case SPINNER_BRAIN:   return SPINNER_BRAIN_FRAMES[idx % N_BRAIN];
        case SPINNER_SPARKLE: return SPINNER_SPARKLE_FRAMES[idx % N_SPARKLE];
        default:              return NULL; /* kawaii face mode */
    }
}

/* Parse a spinner style name (from display.spinner_style config) to type enum.
 * Port of Python display.py:KawaiiSpinner.SPINNERS key mapping.
 * Mirrors the spinner type keys used in Python KawaiiSpinner class. */
spinner_type_t display_parse_spinner_type(const char *style) {
    if (!style || !style[0]) return SPINNER_KAWAII;
    if (strcmp(style, "dots") == 0)    return SPINNER_DOTS;
    if (strcmp(style, "bounce") == 0)  return SPINNER_BOUNCE;
    if (strcmp(style, "grow") == 0)    return SPINNER_GROW;
    if (strcmp(style, "arrows") == 0)  return SPINNER_ARROWS;
    if (strcmp(style, "star") == 0)    return SPINNER_STAR;
    if (strcmp(style, "moon") == 0)    return SPINNER_MOON;
    if (strcmp(style, "pulse") == 0)   return SPINNER_PULSE;
    if (strcmp(style, "brain") == 0)   return SPINNER_BRAIN;
    if (strcmp(style, "sparkle") == 0) return SPINNER_SPARKLE;
    if (strcmp(style, "kawaii") == 0)  return SPINNER_KAWAII;
    /* Also match classic/default for compatibility */
    if (strcmp(style, "classic") == 0 || strcmp(style, "default") == 0)
        return SPINNER_KAWAII;
    return SPINNER_KAWAII; /* fallback */
}

static const char *KAWAII_WAITING[] = {
    "(｡◕‿◕｡)", "(◕‿◕✿)", "٩(◕‿◕｡)۶", "(✿◠‿◠)", "( ˘▽˘)っ",
    "♪(´ε` )", "(◕ᴗ◕✿)", "ヾ(＾∇＾)", "(≧◡≦)", "(★ω★)",
};
static const int N_WAITING = 10;

static const char *KAWAII_THINKING[] = {
    "(｡•́︿•̀｡)", "(◔_◔)", "(¬‿¬)", "( •_•)>⌐■-■", "(⌐■_■)",
    "(´･_･`)", "◉_◉", "(°ロ°)", "( ˘⌣˘)♡", "ヽ(>∀<☆)☆",
    "٩(๑❛ᴗ❛๑)۶", "(⊙_⊙)", "(¬_¬)", "( ͡° ͜ʖ ͡°)", "ಠ_ಠ",
};
static const int N_THINKING = 15;

/* Thinking verbs — fallback when skin doesn't define spinner.thinking_verbs */
static const char *THINKING_VERBS[] = {
    "pondering", "contemplating", "musing", "cogitating", "ruminating",
    "deliberating", "mulling", "reflecting", "processing", "reasoning",
    "analyzing", "computing", "synthesizing", "formulating", "brainstorming",
};
static const int N_VERBS = 15;

/* Port of Python display.py:KawaiiSpinner.get_waiting_faces().
 * Return waiting faces from skin spinner config, falling back to KAWAII_WAITING. */
const char **display_get_waiting_faces(int *out_count) {
    if (out_count) *out_count = N_WAITING;
    if (g_display_skin) {
        const json_t *faces_arr = (const json_t *)skin_get_json(g_display_skin, "spinner.waiting_faces");
        if (faces_arr && faces_arr->type == JSON_ARRAY && json_len(faces_arr) > 0) {
            /* Build dynamic array from skin - for now just return static */
        }
    }
    return (const char **)KAWAII_WAITING;
}

/* Port of Python display.py:KawaiiSpinner.get_thinking_faces().
 * Return thinking faces from skin spinner config, falling back to KAWAII_THINKING. */
const char **display_get_thinking_faces(int *out_count) {
    if (out_count) *out_count = N_THINKING;
    if (g_display_skin) {
        const json_t *faces_arr = (const json_t *)skin_get_json(g_display_skin, "spinner.thinking_faces");
        if (faces_arr && faces_arr->type == JSON_ARRAY && json_len(faces_arr) > 0) {
            /* Build dynamic array from skin - for now just return static */
        }
    }
    return (const char **)KAWAII_THINKING;
}

/* Port of Python display.py:KawaiiSpinner.get_thinking_verbs().
 * Return thinking verbs from skin spinner config, falling back to THINKING_VERBS. */
const char **display_get_thinking_verbs(int *out_count) {
    if (out_count) *out_count = N_VERBS;
    if (g_display_skin) {
        const json_t *verbs_arr = (const json_t *)skin_get_json(g_display_skin, "spinner.thinking_verbs");
        if (verbs_arr && verbs_arr->type == JSON_ARRAY && json_len(verbs_arr) > 0) {
            /* Build dynamic array from skin - for now just return static */
        }
    }
    return (const char **)THINKING_VERBS;
}

/* Set the skin pointer for display styling. Used by KawaiiSpinner to
 * read spinner config (faces, verbs, wings) from the active skin.
 * Port of Python display.py skin integration via _get_skin() and skin engine. */
void display_set_skin(void *skin) {
    g_display_skin = (skin_t *)skin;
}

/* Load wings from skin spinner config. Returns true if wings set.
 * Port of Python display.py:KawaiiSpinner skin integration (get_spinner_wings). */
static bool load_skin_wings(char *left, size_t left_sz, char *right, size_t right_sz) {
    if (!g_display_skin) return false;
    const json_t *wings_arr = (const json_t *)skin_get_json(g_display_skin, "spinner.wings");
    if (!wings_arr || wings_arr->type != JSON_ARRAY || json_len(wings_arr) == 0)
        return false;
    /* Get first wing pair */
    const json_t *pair = json_get(wings_arr, 0);
    if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2)
        return false;
    const json_t *l = json_get(pair, 0);
    const json_t *r = json_get(pair, 1);
    if (l && l->type == JSON_STRING) snprintf(left, left_sz, "%s", l->str_val);
    if (r && r->type == JSON_STRING) snprintf(right, right_sz, "%s", r->str_val);
    return left[0] != '\0' || right[0] != '\0';
}

/* Port of Python display.py:KawaiiSpinner.__init__/start().
/* AG26: Port of Python agent/display.py:start() */
/* AG26: Port of Python agent/display.py:__init__() */
/* AG26: Port of Python agent/display.py:_get_skin() */
/* Start kawaii spinner with label and thinking mode.
 * Mirrors Python KawaiiSpinner class with face/verb cycles + wings. */
void display_kawaii_start(display_kawaii_t *k, const char *label, bool thinking) {
    if (!k) return;
    k->frame = 0;
    k->active = true;
    k->thinking = thinking;
    k->face[0] = '\0';
    k->verb[0] = '\0';
    k->wing_left[0] = '\0';
    k->wing_right[0] = '\0';
    k->label = label ? strdup(label) : NULL;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    k->start_time = (double)tv.tv_sec + (double)tv.tv_usec / 1e6;
    /* Load wings from skin */
    load_skin_wings(k->wing_left, sizeof(k->wing_left),
                    k->wing_right, sizeof(k->wing_right));
    /* Print initial state */
    const char *frame = spinner_get_frame(k->type, 0);
    if (frame) {
        /* V11: Frame-based spinner */
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, frame, k->label ? k->label : "", k->wing_right);
        else if (thinking && k->verb[0])
            printf("\r%s  %s  (%s)", frame, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", frame, k->label ? k->label : "");
    } else {
        if (thinking) {
            snprintf(k->face, sizeof(k->face), "%s", KAWAII_THINKING[0]);
            snprintf(k->verb, sizeof(k->verb), "%s", THINKING_VERBS[0]);
        } else {
            snprintf(k->face, sizeof(k->face), "%s", KAWAII_WAITING[0]);
        }
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, k->face, k->label ? k->label : "", k->wing_right);
        else if (thinking && k->verb[0])
            printf("\r%s  %s  (%s)", k->face, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", k->face, k->label ? k->label : "");
    }
    fflush(stdout);
}

/* Port of Python display.py:KawaiiSpinner.tick().
/* AG26: Port of Python agent/display.py:tick() */
/* AG26: Port of Python agent/display.py:_write() */
/* Advance spinner frame and print next face/verb.
 * Mirrors Python KawaiiSpinner.tick(). */
void display_kawaii_tick(display_kawaii_t *k) {
    if (!k || !k->active || !is_tty) return;
    k->frame++;
    int idx = k->frame;
    const char *frame = spinner_get_frame(k->type, idx);
    if (frame) {
        /* V11: Frame-based spinner */
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, frame, k->label ? k->label : "", k->wing_right);
        else if (k->thinking && k->verb[0])
            printf("\r%s  %s  (%s)", frame, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", frame, k->label ? k->label : "");
    } else {
        const char *face;
        if (k->thinking) {
            face = KAWAII_THINKING[idx % N_THINKING];
            const char *verb = THINKING_VERBS[idx % N_VERBS];
            snprintf(k->verb, sizeof(k->verb), "%s", verb);
        } else {
            face = KAWAII_WAITING[idx % N_WAITING];
        }
        snprintf(k->face, sizeof(k->face), "%s", face);
        if (k->wing_left[0])
            printf("\r%s%s  %s  %s", k->wing_left, face, k->label ? k->label : "", k->wing_right);
        else if (k->thinking && k->verb[0])
            printf("\r%s  %s  (%s)", face, k->label ? k->label : "", k->verb);
        else
            printf("\r%s  %s", face, k->label ? k->label : "");
    }
    fflush(stdout);
}

/* Port of Python display.py:KawaiiSpinner.stop().
/* AG26: Port of Python agent/display.py:stop() */
/* AG26: Port of Python agent/display.py:_is_tty() */
/* AG26: Port of Python agent/display.py:_animate() */
/* AG26: Port of Python agent/display.py:update_text() */
/* Stop spinner, print final face with checkmark.
 * Mirrors Python KawaiiSpinner.stop(). */
void display_kawaii_stop(display_kawaii_t *k, const char *done_msg) {
    if (!k) return;
    k->active = false;
    int idx = k->frame;
    const char *frame = spinner_get_frame(k->type, idx);
    const char *display_char;
    if (frame) {
        display_char = frame;
    } else if (k->thinking) {
        display_char = KAWAII_THINKING[idx % N_THINKING];
    } else {
        display_char = KAWAII_WAITING[idx % N_WAITING];
    }
    if (is_tty) {
        if (k->wing_left[0])
            printf("\r%s%s  ", k->wing_left, display_char);
        else
            printf("\r%s  ", display_char);
        if (done_msg) {
            display_printf(DISPLAY_GREEN, DISPLAY_NORMAL, "✓ ");
            printf("%s\n", done_msg);
        } else {
            display_printf(DISPLAY_GREEN, DISPLAY_NORMAL, "✓ ");
            printf("%s\n", k->label ? k->label : "done");
        }
    }
    free(k->label);
    k->label = NULL;
    fflush(stdout);
}

/* ================================================================
 *  Tool preview builder — extract primary arg from tool call JSON
 * ================================================================ */

/* Port of Python display.py:build_tool_preview().
 * Build a short preview of a tool call's primary argument for display.
 * Mirrors Python build_tool_preview() with per-tool primary arg mapping. */
char *display_tool_preview(const char *tool_name, const char *args_json) {
    if (!tool_name || !args_json) return NULL;

    /* Parse JSON args */
    json_t *args = json_parse(args_json, NULL);
    if (!args || args->type != JSON_OBJECT) {
        json_free(args);
        return NULL;
    }

    char buf[512];
    const char *preview = NULL;

    /* ── process: "action session_id \"data\" 30s" ─────────── */
    if (strcmp(tool_name, "process") == 0) {
        const char *action = json_get_str(args, "action", "");
        const char *sid = json_get_str(args, "session_id", "");
        const char *data = json_get_str(args, "data", "");
        double timeout_val = json_get_num(args, "timeout", 0);

        size_t pos = 0;
        if (action[0]) { pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", action); }
        if (sid[0]) { pos += snprintf(buf + pos, sizeof(buf) - pos, " %s", sid); }
        if (data[0]) {
            char onelined[64];
            display_oneline(data, onelined, sizeof(onelined));
            size_t dlen = strlen(onelined);
            if (dlen > 20) onelined[17] = '.'; onelined[18] = '.'; onelined[19] = '.'; onelined[20] = '\0';
            pos += snprintf(buf + pos, sizeof(buf) - pos, " \"%s\"", onelined);
        }
        if (timeout_val > 0 && strcmp(action, "wait") == 0)
            pos += snprintf(buf + pos, sizeof(buf) - pos, " %.0fs", timeout_val);
        preview = buf;
        if (pos == 0) preview = NULL;
        goto done;
    }

    /* ── todo: "reading task list" / "updating N task(s)" / "planning N task(s)" ── */
    if (strcmp(tool_name, "todo") == 0) {
        json_t *todos = json_obj_get(args, "todos");
        int merge_flag = 0;
        const char *m = json_get_str(args, "merge", NULL);
        if (m && (strcmp(m, "true") == 0 || strcmp(m, "1") == 0)) merge_flag = 1;

        if (!todos) {
            snprintf(buf, sizeof(buf), "reading task list");
        } else if (merge_flag) {
            snprintf(buf, sizeof(buf), "updating %zu task(s)", json_len(todos));
        } else {
            snprintf(buf, sizeof(buf), "planning %zu task(s)", json_len(todos));
        }
        preview = buf;
        goto done;
    }

    /* ── session_search: 'recall: "query"' ─────────────────── */
    if (strcmp(tool_name, "session_search") == 0) {
        const char *query = json_get_str(args, "query", "");
        char onelined[64];
        display_oneline(query, onelined, sizeof(onelined));
        size_t qlen = strlen(onelined);
        if (qlen > 25) {
            onelined[22] = '.'; onelined[23] = '.'; onelined[24] = '.'; onelined[25] = '\0';
        }
        snprintf(buf, sizeof(buf), "recall: \"%s\"", onelined);
        preview = buf;
        goto done;
    }

    /* ── memory: "+target: \"content\"" / "~target: \"old\"" / "-target: \"old\"" ── */
    if (strcmp(tool_name, "memory") == 0) {
        const char *action = json_get_str(args, "action", "");
        const char *target = json_get_str(args, "target", "");
        if (strcmp(action, "add") == 0) {
            const char *content = json_get_str(args, "content", "");
            char onelined[64];
            display_oneline(content ? content : "", onelined, sizeof(onelined));
            size_t clen = strlen(onelined);
            if (clen > 25) { onelined[22] = '.'; onelined[23] = '.'; onelined[24] = '.'; onelined[25] = '\0'; }
            snprintf(buf, sizeof(buf), "+%s: \"%s\"", target, onelined);
        } else if (strcmp(action, "replace") == 0) {
            const char *old = json_get_str(args, "old_text", NULL);
            if (!old) old = "<missing old_text>";
            char onelined[64];
            display_oneline(old, onelined, sizeof(onelined));
            size_t olen = strlen(onelined);
            if (olen > 20) { onelined[17] = '.'; onelined[18] = '.'; onelined[19] = '.'; onelined[20] = '\0'; }
            snprintf(buf, sizeof(buf), "~%s: \"%s\"", target, onelined);
        } else if (strcmp(action, "remove") == 0) {
            const char *old = json_get_str(args, "old_text", NULL);
            if (!old) old = "<missing old_text>";
            char onelined[64];
            display_oneline(old, onelined, sizeof(onelined));
            size_t olen = strlen(onelined);
            if (olen > 20) { onelined[17] = '.'; onelined[18] = '.'; onelined[19] = '.'; onelined[20] = '\0'; }
            snprintf(buf, sizeof(buf), "-%s: \"%s\"", target, onelined);
        } else {
            snprintf(buf, sizeof(buf), "%s", action[0] ? action : "");
        }
        preview = buf;
        if (!preview[0]) preview = NULL;
        goto done;
    }

    /* ── send_message: 'to target: "msg..."' ───────────────── */
    if (strcmp(tool_name, "send_message") == 0) {
        const char *target = json_get_str(args, "target", "?");
        const char *msg = json_get_str(args, "message", "");
        char onelined[64];
        display_oneline(msg, onelined, sizeof(onelined));
        size_t mlen = strlen(onelined);
        if (mlen > 20) { onelined[17] = '.'; onelined[18] = '.'; onelined[19] = '.'; onelined[20] = '\0'; }
        snprintf(buf, sizeof(buf), "to %s: \"%s\"", target, onelined);
        preview = buf;
        goto done;
    }

    /* ── Primary arg key per tool ─────────────────────── */
    struct { const char *tools; const char *key; } const primary[] = {
        {"terminal", "command"},
        {"web_search", "query"},
        {"web_extract", "urls"},
        {"read_file", "path"},
        {"write_file", "path"},
        {"patch", "path"},
        {"search_files", "pattern"},
        {"browser_navigate", "url"},
        {"browser_click", "ref"},
        {"browser_type", "text"},
        {"image_generate", "prompt"},
        {"text_to_speech", "text"},
        {"vision_analyze", "question"},
        {"mixture_of_agents", "user_prompt"},
        {"skill_view", "name"},
        {"skills_list", "category"},
        {"cronjob", "action"},
        {"execute_code", "code"},
        {"delegate_task", "goal"},
        {"clarify", "question"},
        {"skill_manage", "name"},
        {"browser_snapshot", "full"},
        {"browser_scroll", "direction"},
        {"browser_back", NULL},
        {"browser_press", "key"},
        {"browser_get_images", NULL},
        {"browser_vision", NULL},
        {NULL, NULL},
    };

    for (int i = 0; primary[i].tools; i++) {
        if (strcmp(tool_name, primary[i].tools) == 0) {
            if (!primary[i].key) {
                /* Tools with no primary arg — use tool name as preview */
                snprintf(buf, sizeof(buf), "%s", tool_name);
                preview = buf;
            } else if (strcmp(primary[i].key, "urls") == 0) {
                json_t *urls = json_obj_get(args, "urls");
                if (urls && urls->type == JSON_ARRAY && json_len(urls) > 0) {
                    json_t *first = json_get(urls, 0);
                    if (first && first->type == JSON_STRING) {
                        snprintf(buf, sizeof(buf), "%.*s", (int)sizeof(buf)-1, first->str_val);
                        preview = buf;
                    }
                }
            } else if (strcmp(primary[i].key, "category") == 0) {
                const char *cat = json_get_str(args, "category", "all");
                snprintf(buf, sizeof(buf), "list %s", cat);
                preview = buf;
            } else if (strcmp(primary[i].key, "full") == 0) {
                double full_flag = json_get_num(args, "full", 0);
                snprintf(buf, sizeof(buf), "%s", (full_flag != 0) ? "full page" : "compact");
                preview = buf;
            } else if (strcmp(primary[i].key, "direction") == 0) {
                preview = json_get_str(args, "direction", "down");
            } else if (strcmp(primary[i].key, "code") == 0) {
                const char *code = json_get_str(args, "code", "");
                char onelined[64];
                display_oneline(code, onelined, sizeof(onelined));
                /* Show first line only */
                char *nl = strchr(onelined, '\n');
                if (nl) *nl = '\0';
                snprintf(buf, sizeof(buf), "%s", onelined);
                preview = buf;
            } else if (strcmp(primary[i].key, "action") == 0 && strcmp(tool_name, "cronjob") == 0) {
                const char *action = json_get_str(args, "action", "");
                if (strcmp(action, "create") == 0) {
                    const char *name = json_get_str(args, "name", NULL);
                    json_t *skills = json_obj_get(args, "skills");
                    const char *prompt = json_get_str(args, "prompt", NULL);
                    if (name) {
                        snprintf(buf, sizeof(buf), "create %s", name);
                    } else if (skills && json_len(skills) > 0) {
                        json_t *first = json_get(skills, 0);
                        if (first && first->type == JSON_STRING)
                            snprintf(buf, sizeof(buf), "create %s", first->str_val);
                    } else if (prompt) {
                        char onelined[48];
                        display_oneline(prompt, onelined, sizeof(onelined));
                        snprintf(buf, sizeof(buf), "create %s", onelined);
                    } else {
                        snprintf(buf, sizeof(buf), "create task");
                    }
                } else if (strcmp(action, "list") == 0) {
                    snprintf(buf, sizeof(buf), "listing");
                } else {
                    const char *jid = json_get_str(args, "job_id", "");
                    snprintf(buf, sizeof(buf), "%s %s", action, jid);
                }
                preview = buf;
            } else {
                preview = json_get_str(args, primary[i].key, NULL);
            }
            goto done;
        }
    }

    /* ── Fallback: try common keys ──────────────────────────── */
    if (!preview) {
        const char *fallbacks[] = {"query", "text", "command", "path", "name", "prompt", "code", "goal", "message", "content", "action", NULL};
        for (int i = 0; fallbacks[i]; i++) {
            preview = json_get_str(args, fallbacks[i], NULL);
            if (preview) break;
        }
    }

done:
    char *result = NULL;
    if (preview && preview[0]) {
        /* Collapse whitespace, truncate */
        char cleaned[128];
        int j = 0;
        int last_space = 0;
        for (int i = 0; preview[i] && j < (int)sizeof(cleaned) - 4; i++) {
            if (preview[i] == ' ' || preview[i] == '\t' || preview[i] == '\n') {
                if (!last_space) { cleaned[j++] = ' '; last_space = 1; }
            } else {
                cleaned[j++] = preview[i];
                last_space = 0;
            }
        }
        /* Truncate with ... */
        if (j > 60) {
            j = 57;
            cleaned[j++] = '.';
            cleaned[j++] = '.';
            cleaned[j++] = '.';
        }
        cleaned[j] = '\0';
        result = strdup(cleaned);
    }

    json_free(args);
    return result;
}

/* ================================================================
 *  Tool activity line — ┊ prefix + emoji + tool_name + preview
 * ================================================================ */

/* Port of Python display.py tool activity display (cli.py _print_tool_activity).
 * Print tool activity line with emoji, tool name, and preview. */
void display_tool_activity(const char *tool_name, const char *preview,
                           display_color_t color) {
    if (!tool_name) return;

    /* Emoji map — check skin tool_emojis first, fall back to hardcoded */
    const char *emoji = NULL;
    if (g_display_skin) {
        char key[128];
        snprintf(key, sizeof(key), "tool_emojis.%s", tool_name);
        emoji = skin_get(g_display_skin, key, NULL);
    }
    if (!emoji) {
        /* Hardcoded defaults per tool */
        if (strcmp(tool_name, "terminal") == 0) emoji = "$ ";
        else if (strcmp(tool_name, "write_file") == 0) emoji = "📝";
        else if (strcmp(tool_name, "read_file") == 0) emoji = "📖";
        else if (strcmp(tool_name, "patch") == 0) emoji = "🩹";
        else if (strcmp(tool_name, "web_search") == 0) emoji = "🔍";
        else if (strcmp(tool_name, "search_files") == 0) emoji = "🔎";
        else if (strcmp(tool_name, "execute_code") == 0) emoji = "🐍";
        else if (strcmp(tool_name, "delegate_task") == 0) emoji = "📋";
        else if (strcmp(tool_name, "vision_analyze") == 0) emoji = "👁️";
        else if (strcmp(tool_name, "image_generate") == 0) emoji = "🎨";
        else if (strcmp(tool_name, "text_to_speech") == 0) emoji = "🔊";
        else if (strcmp(tool_name, "send_message") == 0) emoji = "📤";
        else if (strcmp(tool_name, "memory") == 0) emoji = "🧠";
        else if (strcmp(tool_name, "session_search") == 0) emoji = "📚";
        else if (strcmp(tool_name, "skill_view") == 0 || strcmp(tool_name, "skill_manage") == 0) emoji = "🛠️";
        else if (strcmp(tool_name, "cronjob") == 0) emoji = "⏰";
        else if (strcmp(tool_name, "todo") == 0) emoji = "✅";
        else if (strcmp(tool_name, "clarify") == 0) emoji = "❓";
        else if (strcmp(tool_name, "browser_navigate") == 0 || strcmp(tool_name, "browser_click") == 0
                 || strcmp(tool_name, "browser_type") == 0) emoji = "🌐";
        else emoji = "⚡";
    }

    printf("  %s ", emoji);
    display_printf(color, DISPLAY_BOLD, "%s", tool_name);
    if (preview) {
        printf(" ");
        display_printf(DISPLAY_DEFAULT, DISPLAY_DIM, "%s", preview);
    }
    printf("\n");
    fflush(stdout);
}


/* ================================================================
 *  Word Wrapping
 * ================================================================ */

/* Word-wrap text to max_width columns. Preserves existing newlines (paragraphs).
 * Returns malloc'd string (caller free). Tabs treated as single space.
 * ANSI escape sequences are NOT stripped — caller should wrap plain text. */
char *display_word_wrap(const char *text, int max_width) {
    if (!text || max_width < 1) return strdup(text ? text : "");
    size_t in_len = strlen(text);
    /* Upper bound: each char could need \n before it */
    size_t cap = in_len + (in_len / (max_width > 1 ? max_width : 1)) + 2;
    char *out = (char *)malloc(cap);
    if (!out) return strdup(text);
    size_t pos = 0;
    int col = 0;        /* current display column */
    int word_start = -1; /* buffer position of current word start (-1 = in whitespace) */
    int word_width = 0;

    for (size_t i = 0; i < in_len && pos < cap - 1; i++) {
        char ch = text[i];
        if (ch == '\n') {
            /* Flush current word if any */
            if (word_start >= 0) {
                size_t wlen = i - (size_t)word_start;
                if (pos + wlen + 1 >= cap) break;
                memcpy(out + pos, text + word_start, wlen);
                pos += wlen;
                col += word_width;
                word_start = -1;
                word_width = 0;
            }
            out[pos++] = '\n';
            col = 0;
            continue;
        }
        if (ch == ' ' || ch == '\t') {
            if (word_start >= 0) {
                /* Check if word fits on current line */
                if (col + word_width > max_width) {
                    /* Insert newline before the word */
                    out[pos++] = '\n';
                    col = 0;
                }
                size_t wlen = i - (size_t)word_start;
                if (pos + wlen + 1 >= cap) break;
                memcpy(out + pos, text + word_start, wlen);
                pos += wlen;
                col += word_width;
                word_start = -1;
                word_width = 0;
            }
            /* Emit the space (only if not at column 0) */
            if (col > 0 && pos < cap - 1) {
                out[pos++] = ' ';
                col++;
            }
            continue;
        }
        /* Regular character: part of a word */
        if (word_start < 0) word_start = (int)i;
        /* Increment display width for this char (skip UTF-8 continuation bytes) */
        if (((unsigned char)ch & 0xC0) != 0x80) word_width++;
    }
    /* Flush last word */
    if (word_start >= 0) {
        size_t wlen = in_len - (size_t)word_start;
        if (col + word_width > max_width && col > 0) {
            out[pos++] = '\n';
            col = 0;
        }
        if (pos + wlen + 1 >= cap) {
            if (pos < cap) out[pos] = '\0';
        } else {
            memcpy(out + pos, text + word_start, wlen);
            pos += wlen;
        }
    }
    out[pos] = '\0';
    return out;
}

/* Panel with TrueColor hex border (wraps display_panel with hex→RGB) */
void display_panel_hex(const char *title, const char *content, const char *border_hex) {
    if (!content || !border_hex) return;
    int r, g, b;
    if (!ansi_parse_hex(border_hex, &r, &g, &b)) {
        display_panel(title, content, DISPLAY_WHITE);
        return;
    }
    int term_width = display_width();

    /* Calculate inner width */
    int inner = term_width > 80 ? 80 : (term_width < 40 ? 40 : term_width);
    inner = (inner * 8) / 10;

    /* Top border with title */
    display_set_fg_rgb(r, g, b);
    display_set_style(DISPLAY_BOLD);
    if (title && title[0]) {
        printf("\n\xE2\x94\x8C %s ", title);
        int remaining = inner - (int)strlen(title) - 3;
        for (int i = 0; i < remaining && i < 60; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    } else {
        printf("\n\xE2\x94\x8C");
        for (int i = 0; i < inner; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    }
    display_reset();

    /* Content */
    printf("%s\n", content);

    /* Bottom border */
    display_set_fg_rgb(r, g, b);
    display_set_style(DISPLAY_BOLD);
    printf("\xE2\x94\x94");
    for (int i = 0; i < inner; i++)
        printf("\xE2\x94\x80");
    printf("\xE2\x94\x98\n");
    display_reset();
    fflush(stdout);
}

/* Horizontal rule with TrueColor hex */
void display_hr_hex(const char *hex_fg) {
    if (!hex_fg) { display_hr(DISPLAY_WHITE); return; }
    int r, g, b;
    if (!ansi_parse_hex(hex_fg, &r, &g, &b)) { display_hr(DISPLAY_WHITE); return; }
    int w = display_width();
    if (w > 78) w = 78;
    display_set_fg_rgb(r, g, b);
    for (int i = 0; i < w; i++)
        printf("\xE2\x94\x80");
    printf("\n");
    display_reset();
    fflush(stdout);
}

/* ================================================================
 *  Status Bar
 * ================================================================ */

/* Get a hex color from the active skin or fallback, parsed into RGB.
 * Returns true if color was available. */
static bool skin_color_rgb(const char *key, const char *fallback_hex,
                           int *r, int *g, int *b) {
    if (!g_display_skin) return ansi_parse_hex(fallback_hex, r, g, b);
    const char *val = skin_get(g_display_skin, key, NULL);
    if (!val) val = fallback_hex;
    return ansi_parse_hex(val, r, g, b);
}

/* Display a status bar line with context%, budget, and cost */
void display_statusbar(const char *model, const char *session_id,
                        int turn_count, int token_count, int max_iters,
                        int iteration_count, double estimated_cost) {
    (void)session_id;
    if (!is_tty) return;

    /* Get skin colors */
    int bg_r, bg_g, bg_b, fg_r, fg_g, fg_b, dim_r, dim_g, dim_b;
    int good_r, good_g, good_b, warn_r, warn_g, warn_b;
    int bad_r, bad_g, bad_b, crit_r, crit_g, crit_b;

    if (!skin_color_rgb("colors.status_bar_bg", "#1a1a2e", &bg_r, &bg_g, &bg_b))
        { bg_r=26; bg_g=26; bg_b=46; }
    if (!skin_color_rgb("colors.status_bar_strong", "#FFD700", &fg_r, &fg_g, &fg_b))
        { fg_r=255; fg_g=215; fg_b=0; }
    skin_color_rgb("colors.status_bar_dim", "#8B8682", &dim_r, &dim_g, &dim_b);
    skin_color_rgb("colors.status_bar_good", "#8FBC8F", &good_r, &good_g, &good_b);
    skin_color_rgb("colors.status_bar_warn", "#FFD700", &warn_r, &warn_g, &warn_b);
    skin_color_rgb("colors.status_bar_bad", "#FF8C00", &bad_r, &bad_g, &bad_b);
    skin_color_rgb("colors.status_bar_critical", "#FF6B6B", &crit_r, &crit_g, &crit_b);

    /* Get terminal width */
    int w = display_width();
    if (w > 100) w = 100;
    if (w < 40) { w = 40; } /* Minimum */

    /* Build real timestamp */
    char time_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    if (tm_info)
        strftime(time_str, sizeof(time_str), "%H:%M", tm_info);
    else
        snprintf(time_str, sizeof(time_str), "--:--");

    /* D19: Context usage % */
    char ctx_str[32] = "";
    size_t ctx_max = hermes_token_context_size(model);
    if (ctx_max > 0 && token_count > 0) {
        int ctx_pct = (int)((double)token_count / ctx_max * 100.0);
        if (ctx_pct > 100) ctx_pct = 100;
        snprintf(ctx_str, sizeof(ctx_str), " ctx:%d%%", ctx_pct);
    }

    /* D20: Budget (iterations used / max) */
    char budget_str[32] = "";
    if (max_iters > 0 && iteration_count > 0) {
        snprintf(budget_str, sizeof(budget_str), " %d/%d", iteration_count, max_iters);
    }

    /* D20: Estimated cost */
    char cost_str[32] = "";
    if (estimated_cost > 0.0) {
        if (estimated_cost < 0.01)
            snprintf(cost_str, sizeof(cost_str), " $%.4f", estimated_cost);
        else
            snprintf(cost_str, sizeof(cost_str), " $%.2f", estimated_cost);
    }

    /* Build status bar segments */
    char left[256];
    snprintf(left, sizeof(left), " %s ", model ? model : "default");
    char right[256];
    if (token_count > 0 && turn_count > 0) {
        snprintf(right, sizeof(right), " iter:%d%s tok:%d%s%s %s",
                 turn_count, budget_str, token_count, ctx_str, cost_str, time_str);
    } else if (token_count > 0) {
        snprintf(right, sizeof(right), " iter:%d%s tok:%d%s%s %s",
                 turn_count, budget_str, token_count, ctx_str, cost_str, time_str);
    } else {
        snprintf(right, sizeof(right), " iter:%d%s %s",
                 turn_count, budget_str, time_str);
    }

    /* Truncate if too long */
    int max_left = w / 2 - 4;
    if ((int)strlen(left) > max_left) left[max_left] = '\0';
    if ((int)strlen(right) > w - (int)strlen(left) - 4)
        right[w - (int)strlen(left) - 4] = '\0';

    /* Clear line, then paint background */
    printf("\n\x1B[2K\x1B[48;2;%d;%d;%dm", bg_r, bg_g, bg_b);

    /* Left: model in fg color */
    printf("\x1B[38;2;%d;%d;%dm%s", fg_r, fg_g, fg_b, left);

    /* Right: aligned, session in dim, context in context color */
    int pad = w - (int)strlen(left) - (int)strlen(right);
    if (pad < 1) pad = 1;
    for (int i = 0; i < pad; i++) putchar(' ');

    /* Right side in dim */
    printf("\x1B[38;2;%d;%d;%dm%s", dim_r, dim_g, dim_b, right);

    /* Reset */
    printf("\x1B[0m\n");
    fflush(stdout);
}

/* ================================================================
 *  Panel / Box
 * ================================================================ */

void display_panel(const char *title, const char *content, display_color_t color) {
    if (!content) return;
    int term_width = display_width();

    /* Calculate inner width (80% of terminal, min 40, max 80) */
    int inner = term_width > 80 ? 80 : (term_width < 40 ? 40 : term_width);
    inner = (inner * 8) / 10;

    /* Top border */
    if (title && title[0]) {
        display_printf(color, DISPLAY_BOLD, "\n\xE2\x94\x8C");
        printf(" %s ", title);
        int remaining = inner - (int)strlen(title) - 3;
        for (int i = 0; i < remaining && i < 60; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    } else {
        display_printf(color, DISPLAY_BOLD, "\n\xE2\x94\x8C");
        for (int i = 0; i < inner; i++)
            printf("\xE2\x94\x80");
        printf("\xE2\x94\x90\n");
    }

    /* Content with word-wrap */
    int wrap_width = inner - 2; /* leave 1-char padding on each side */
    if (wrap_width < 20) wrap_width = 20;
    char *wrapped = display_word_wrap(content, wrap_width);
    if (wrapped) {
        /* Print each wrapped line with left-padding for inner box */
        const char *line_start = wrapped;
        const char *nl;
        while ((nl = strchr(line_start, '\n')) != NULL) {
            size_t line_len = (size_t)(nl - line_start);
            printf(" ");
            if (line_len > 0) {
                printf("%.*s", (int)line_len, line_start);
            }
            printf("\n");
            line_start = nl + 1;
        }
        /* Last line (no trailing newline) */
        if (*line_start) {
            printf(" %s\n", line_start);
        }
        free(wrapped);
    } else {
        display_printf(color, DISPLAY_NORMAL, "%s", content);
        if (content[strlen(content) - 1] != '\n')
            printf("\n");
    }

    /* Bottom border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x94");
    for (int i = 0; i < inner; i++)
        printf("\xE2\x94\x80");
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x98\n\n");
}

void display_hr(display_color_t color) {
    int w = display_width();
    if (w > 80) w = 80;
    display_printf(color, DISPLAY_DIM, "");
    for (int i = 0; i < w; i++)
        printf("\xE2\x94\x80");
    printf("\n");
    fflush(stdout);
}

/* ================================================================
 *  ASCII Table
 * ================================================================ */

void display_table(int columns, const char **headers,
                   const char **rows, int num_rows,
                   display_color_t color) {
    if (columns < 1) return;

    /* Calculate max column widths */
    int *widths = calloc(columns, sizeof(int));
    if (!widths) return;

    /* Measure headers */
    for (int c = 0; c < columns && headers; c++) {
        int len = (int)strlen(headers[c] ? headers[c] : "");
        if (len > widths[c]) widths[c] = len;
    }

    /* Measure rows — split by tab */
    for (int r = 0; r < num_rows; r++) {
        const char *p = rows[r];
        for (int c = 0; c < columns; c++) {
            const char *next = strchr(p, '\t');
            int len = next ? (int)(next - p) : (int)strlen(p);
            if (len > widths[c]) widths[c] = len;
            p = next ? next + 1 : p + strlen(p);
        }
    }

    /* Clamp to terminal width */
    int term_w = display_width();
    int total = 1; /* left border */
    for (int c = 0; c < columns; c++) {
        if (widths[c] > 40) widths[c] = 40;
        total += widths[c] + 3; /* padding + border */
        if (total > term_w - 4) {
            /* Shrink proportionally — simple approach: clamp last */
            widths[c] = term_w - total + 40 - 3;
            if (widths[c] < 5) widths[c] = 5;
            break;
        }
    }

    /* Top border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x8C");
    for (int c = 0; c < columns; c++) {
        for (int i = 0; i < widths[c] + 2; i++)
            printf("\xE2\x94\x80");
        if (c < columns - 1)
            printf("\xE2\x94\xAC");
    }
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x90\n");

    /* Header row */
    if (headers) {
        display_printf(color, DISPLAY_BOLD, "\xE2\x94\x82");
        for (int c = 0; c < columns; c++) {
            const char *h = headers[c] ? headers[c] : "";
            int hlen = (int)strlen(h);
            printf(" %s", h);
            for (int p = hlen; p < widths[c]; p++) printf(" ");
            printf(" \xE2\x94\x82");
        }
        printf("\n");

        /* Header separator */
        display_printf(color, DISPLAY_BOLD, "\xE2\x94\x9C");
        for (int c = 0; c < columns; c++) {
            for (int i = 0; i < widths[c] + 2; i++)
                printf("\xE2\x94\x80");
            if (c < columns - 1)
                printf("\xE2\x94\xBC");
        }
        display_printf(color, DISPLAY_BOLD, "\xE2\x94\xA4\n");
    }

    /* Data rows */
    for (int r = 0; r < num_rows; r++) {
        display_printf(color, DISPLAY_NORMAL, "\xE2\x94\x82");
        const char *p = rows[r];
        for (int c = 0; c < columns; c++) {
            const char *next = strchr(p, '\t');
            int len = next ? (int)(next - p) : (int)strlen(p);
            printf(" ");
            if (len > 0) printf("%.*s", len, p);
            for (int pad = len; pad < widths[c]; pad++) printf(" ");
            printf(" \xE2\x94\x82");
            p = next ? next + 1 : p + len;
        }
        printf("\n");
    }

    /* Bottom border */
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x94");
    for (int c = 0; c < columns; c++) {
        for (int i = 0; i < widths[c] + 2; i++)
            printf("\xE2\x94\x80");
        if (c < columns - 1)
            printf("\xE2\x94\xB4");
    }
    display_printf(color, DISPLAY_BOLD, "\xE2\x94\x98\n");

    free(widths);
}

/* ================================================================
 *  Startup Tips (V11)
 * ================================================================ */

static const char *TIPS[] = {
    "/background <prompt> runs a task in a separate session while your current one stays free.",
    "/branch forks the current session to explore a different direction.",
    "/compress manually compresses conversation context when things get long.",
    "/rollback lists filesystem checkpoints — restore files the agent modified.",
    "/title \"my project\" names your session — resume with /resume or -c.",
    "/resume picks up where you left off in a named session.",
    "/undo removes the last exchange from the conversation.",
    "/retry resends your last message when response wasn't right.",
    "/verbose cycles tool progress display: off → new → all.",
    "/reasoning high increases thinking depth. /reasoning show displays it.",
    "/model lets you switch models mid-session — try /model sonnet.",
    "/skin changes the CLI theme — try ares, mono, slate, or charizard.",
    "/statusbar toggles a bar showing model, tokens, context, cost.",
    "/tools disable browser temporarily removes browser tools.",
    "/cron manages scheduled tasks — recurring prompts to any platform.",
    "/usage shows token usage, cost breakdown, session duration.",
    "/insights shows usage analytics for the last 30 days.",
    "/config shows your current configuration at a glance.",
    "/stop kills all running background processes.",
    "@file:path/to/file.py injects file contents into your message.",
    "@folder:src/ injects a directory tree listing.",
    "@diff injects unstaged git changes into the message.",
    "@staged injects staged git changes (git diff --staged).",
    "@url:https://... fetches a web page and injects its text content.",
    "Ctrl+C during agent thinking returns you to the prompt without losing context.",
    "Ctrl+W in the prompt line deletes the previous word.",
    "Ctrl+A / Ctrl+E jump to start/end of the input line.",
    "Up arrow recalls previous messages in the session.",
    "--json flag outputs machine-readable JSON for CLI tools and scripts.",
    "Tab completion works on slash commands, file paths, and tool names.",
};

#define TIPS_COUNT (int)(sizeof(TIPS) / sizeof(TIPS[0]))

void display_show_tip(void) {
    int idx = rand() % TIPS_COUNT;
    display_printf_hex("#B8860B", DISPLAY_DIM, "  \xe2\x9c\xa6 Tip: %s\n", TIPS[idx]);
}

/* ================================================================
 *  Utility
 * ================================================================ */

bool display_has_color(void) {
    return is_tty;
}

int display_width(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (int)ws.ws_col;
    return 80;
}

/* ================================================================
 *  Theme detection — colors.light / dark scheme auto-select
 * ================================================================ */

bool display_is_dark_theme(void) {
    /* 1. Check COLORFGBG (rxvt, konsole, etc): "fg;bg" where bg is 0-15 */
    const char *cfgbg = getenv("COLORFGBG");
    if (cfgbg) {
        const char *semi = strrchr(cfgbg, ';');
        if (semi) {
            int bg = atoi(semi + 1);
            /* 0-6=dark, 7-15=light. 0=black, 7=white, 8-15=bright */
            if (bg >= 7) return false; /* light background */
            return true;               /* dark background */
        }
    }

    /* 2. Check DARK_BG / LIGHT_BG explicit hints */
    const char *dark_bg = getenv("DARK_BG");
    if (dark_bg) {
        if (strcmp(dark_bg, "1") == 0 || strcasecmp(dark_bg, "true") == 0)
            return true;
        return false;
    }
    const char *light_bg = getenv("LIGHT_BG");
    if (light_bg) {
        if (strcmp(light_bg, "1") == 0 || strcasecmp(light_bg, "true") == 0)
            return false;
    }

    /* 3. VCANVAS_BACKGROUND (Visual Studio / Codespaces) */
    const char *vcbg = getenv("VCANVAS_BACKGROUND");
    if (vcbg && strcasecmp(vcbg, "light") == 0)
        return false;

    /* 4. KONSOLE_PROFILE_NAME — Konsole */
    /* Dark profiles usually contain "dark", light profiles "light" */
    const char *kpn = getenv("KONSOLE_PROFILE_NAME");
    if (kpn) {
        if (strstr(kpn, "light") || strstr(kpn, "Light") || strstr(kpn, "White"))
            return false;
        return true; /* konsole with dark profile = dark */
    }

    /* 5. Default: dark (safe for most devs) */
    return true;
}

/* ── Tool failure detection (port of Python display.py) ────── */

#define ERROR_SUFFIX_MAX_LEN 60

/* Port of Python display.py:_trim_error().
 * Shrink an error message for inline display.
 * Strips long absolute paths down to just the filename.
 * Returns the trimmed string in out_buf (caller-provided buffer). */
void display_trim_error(const char *msg, char *out_buf, size_t out_sz) {
    if (!msg || !out_buf || out_sz == 0) return;
    const char *p = msg;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

    /* "File not found: /very/long/absolute/path/foo.py" → "File not found: foo.py" */
    const char *fnf = strstr(p, "File not found:");
    if (fnf) {
        const char *tail = fnf + strlen("File not found:");
        while (*tail == ' ') tail++;
        const char *last_slash = strrchr(tail, '/');
        if (last_slash) {
            snprintf(out_buf, out_sz, "File not found: %s", last_slash + 1);
            return;
        }
    }

    /* Truncate if too long */
    size_t len = strlen(p);
    if (len > ERROR_SUFFIX_MAX_LEN) {
        size_t keep = (ERROR_SUFFIX_MAX_LEN > 3) ? ERROR_SUFFIX_MAX_LEN - 3 : 0;
        snprintf(out_buf, out_sz, "%.*s...", (int)keep, p);
        return;
    }
    snprintf(out_buf, out_sz, "%s", p);
}

/* Port of Python display.py:_detect_tool_failure().
 * Inspect a tool result string for signs of failure.
 * Returns true if failure detected, and populates suffix_buf
 * with a short informational tag like " [exit 1]" or " [full]".
 * On success returns false and suffix_buf is set to empty string. */
bool display_detect_tool_failure(const char *tool_name, const char *result,
                                  char *suffix_buf, size_t suffix_sz) {
    if (!suffix_buf || suffix_sz == 0) return false;
    suffix_buf[0] = '\0';
    if (!tool_name || !result) return false;

    if (file_mutation_result_landed(tool_name, result))
        return false;

    /* Try parsing result as JSON */
    json_t *data = json_parse(result, NULL);

    /* Terminal: non-zero exit code */
    if (strcmp(tool_name, "terminal") == 0) {
        if (data && data->type == JSON_OBJECT) {
            double exit_code = json_get_num(data, "exit_code", 0);
            if (exit_code != 0) {
                const char *err = json_get_str(data, "error", NULL);
                if (err && err[0]) {
                    char trimmed[128];
                    display_trim_error(err, trimmed, sizeof(trimmed));
                    snprintf(suffix_buf, suffix_sz, " [%s]", trimmed);
                } else {
                    snprintf(suffix_buf, suffix_sz, " [exit %.0f]", exit_code);
                }
                json_free(data);
                return true;
            }
        }
        json_free(data);
        return false;
    }

    /* Memory: "store full" detection */
    if (strcmp(tool_name, "memory") == 0) {
        if (data && data->type == JSON_OBJECT) {
            bool success = json_get_bool(data, "success", true);
            if (!success) {
                const char *err = json_get_str(data, "error", "");
                if (strstr(err, "exceed the limit")) {
                    snprintf(suffix_buf, suffix_sz, " [full]");
                    json_free(data);
                    return true;
                }
            }
        }
    }

    /* Structured error in JSON result */
    if (data && data->type == JSON_OBJECT) {
        const char *err = json_get_str(data, "error", NULL);
        if (!err) err = json_get_str(data, "message", NULL);
        bool success = json_get_bool(data, "success", true);
        if (err && err[0] && (!success)) {
            char trimmed[128];
            display_trim_error(err, trimmed, sizeof(trimmed));
            snprintf(suffix_buf, suffix_sz, " [%s]", trimmed);
            json_free(data);
            return true;
        }
    }
    json_free(data);

    /* Generic heuristic for non-tool result strings */
    if (!result) return false;
    size_t rlen = strlen(result);
    size_t scan_len = (rlen < 500) ? rlen : 500;
    char *lower = (char *)malloc(scan_len + 1);
    if (!lower) return false;
    for (size_t i = 0; i < scan_len; i++)
        lower[i] = (result[i] >= 'A' && result[i] <= 'Z') ? result[i] + 32 : result[i];
    lower[scan_len] = '\0';

    bool found = (strstr(lower, "\"error\"") || strstr(lower, "\"failed\"")
                  || strncmp(result, "Error", 5) == 0);
    free(lower);
    if (found) {
        snprintf(suffix_buf, suffix_sz, " [error]");
        return true;
    }

    return false;
}

/* ================================================================
 *  Additional display.py ports
 * ================================================================ */

/* Port of Python display.py:_oneline().
 * Collapse whitespace (including newlines) to single spaces. */
void display_oneline(const char *text, char *out_buf, size_t out_sz) {
    if (!text || !out_buf || out_sz == 0) return;
    size_t pos = 0;
    int last_space = 0;
    for (size_t i = 0; text[i] && pos < out_sz - 1; i++) {
        char ch = text[i];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (!last_space) { out_buf[pos++] = ' '; last_space = 1; }
        } else {
            out_buf[pos++] = ch;
            last_space = 0;
        }
    }
    out_buf[pos] = '\0';
}

/* Port of Python display.py:get_skin_tool_prefix().
 * Get tool output prefix character from active skin.
 * Returns "┊" by default. */
const char *display_get_skin_tool_prefix(void) {
    if (g_display_skin) {
        const char *prefix = skin_get(g_display_skin, "tool_prefix", NULL);
        if (prefix) return prefix;
    }
    return "\xe2\x94\x8a"; /* "┊" in UTF-8 */
}

/* Port of Python display.py:get_tool_emoji().
 * Get the display emoji for a tool name.
 * Resolution: (1) skin tool_emojis override, (2) hardcoded map, (3) "⚡" fallback. */
const char *display_get_tool_emoji(const char *tool_name) {
    if (!tool_name) return "\xe2\x9a\xa1"; /* ⚡ */

    /* 1. Skin override */
    if (g_display_skin) {
        char key[128];
        snprintf(key, sizeof(key), "tool_emojis.%s", tool_name);
        const char *emoji = skin_get(g_display_skin, key, NULL);
        if (emoji) return emoji;
    }

    /* 2. Hardcoded map */
    if (strcmp(tool_name, "terminal") == 0) return "\xf0\x9f\x92\xbb";    /* 💻 */
    if (strcmp(tool_name, "write_file") == 0) return "\xf0\x9f\x93\x9d";   /* 📝 */
    if (strcmp(tool_name, "read_file") == 0) return "\xf0\x9f\x93\x96";    /* 📖 */
    if (strcmp(tool_name, "patch") == 0) return "\xf0\x9f\xa9\xb9";       /* 🩹 */
    if (strcmp(tool_name, "web_search") == 0) return "\xf0\x9f\x94\x8d";   /* 🔍 */
    if (strcmp(tool_name, "web_extract") == 0) return "\xf0\x9f\x93\x84";  /* 📄 */
    if (strcmp(tool_name, "search_files") == 0) return "\xf0\x9f\x94\x8e"; /* 🔎 */
    if (strcmp(tool_name, "execute_code") == 0) return "\xf0\x9f\x90\x8d"; /* 🐍 */
    if (strcmp(tool_name, "delegate_task") == 0) return "\xf0\x9f\x93\x8b"; /* 📋 */
    if (strcmp(tool_name, "vision_analyze") == 0) return "\xf0\x9f\x91\x81\xef\xb8\x8f"; /* 👁️ */
    if (strcmp(tool_name, "image_generate") == 0) return "\xf0\x9f\x8e\xa8"; /* 🎨 */
    if (strcmp(tool_name, "text_to_speech") == 0) return "\xf0\x9f\x94\x8a"; /* 🔊 */
    if (strcmp(tool_name, "send_message") == 0) return "\xf0\x9f\x93\xa4"; /* 📤 */
    if (strcmp(tool_name, "memory") == 0) return "\xf0\x9f\xa7\xa0";       /* 🧠 */
    if (strcmp(tool_name, "session_search") == 0) return "\xf0\x9f\x93\x9a"; /* 📚 */
    if (strcmp(tool_name, "skill_view") == 0 || strcmp(tool_name, "skill_manage") == 0)
        return "\xf0\x9f\x9b\xa0\xef\xb8\x8f"; /* 🛠️ */
    if (strcmp(tool_name, "skills_list") == 0) return "\xf0\x9f\x93\x9a";  /* 📚 */
    if (strcmp(tool_name, "cronjob") == 0) return "\xe2\x8f\xb0";          /* ⏰ */
    if (strcmp(tool_name, "todo") == 0) return "\xe2\x9c\x85";             /* ✅ */
    if (strcmp(tool_name, "clarify") == 0) return "\xe2\x9d\x93";          /* ❓ */
    if (strcmp(tool_name, "process") == 0) return "\xe2\x9a\x99\xef\xb8\x8f"; /* ⚙️ */
    if (strcmp(tool_name, "browser_navigate") == 0) return "\xf0\x9f\x8c\x90"; /* 🌐 */
    if (strcmp(tool_name, "browser_click") == 0) return "\xf0\x9f\x91\x86"; /* 👆 */
    if (strcmp(tool_name, "browser_type") == 0) return "\xe2\x8c\xa8\xef\xb8\x8f"; /* ⌨️ */
    if (strcmp(tool_name, "browser_snapshot") == 0) return "\xf0\x9f\x93\xb8"; /* 📸 */
    if (strcmp(tool_name, "browser_scroll") == 0) return "\xe2\x86\x93";    /* ↓ */
    if (strcmp(tool_name, "browser_back") == 0) return "\xe2\x97\x80\xef\xb8\x8f"; /* ◀️ */
    if (strcmp(tool_name, "browser_press") == 0) return "\xe2\x8c\xa8\xef\xb8\x8f"; /* ⌨️ */

    /* 3. Fallback */
    return "\xe2\x9a\xa1"; /* ⚡ */
}

/* Port of Python display.py:_result_succeeded().
 * Conservatively detect whether a tool result JSON string represents success.
 * Checks for "error" field and "success" field. */
bool display_result_succeeded(const char *result) {
    if (!result) return false;
    json_t *data = json_parse(result, NULL);
    if (!data || data->type != JSON_OBJECT) {
        json_free(data);
        return false;
    }
    const char *err = json_get_str(data, "error", NULL);
    if (err) {
        json_free(data);
        return false;
    }
    if (json_has(data, "success")) {
        bool ok = json_get_bool(data, "success", false);
        json_free(data);
        return ok;
    }
    json_free(data);
    return true;
}

/* ── Tool preview max length (port of Python display.py) ──── */

/* Static: 0 = unlimited (default) */
static int g_tool_preview_max_len = 0;

/* Port of Python display.py:set_tool_preview_max_len().
 * Set the global max length for tool call previews. 0 = no limit. */
void set_tool_preview_max_len(int n) {
    g_tool_preview_max_len = (n > 0) ? n : 0;
}

/* PoP: cli_agent_display_get_tool_preview_max_len @ agent/display.py:get_tool_preview_max_len */
/* Port of Python display.py:get_tool_preview_max_len().
 * Return the configured max preview length (0 = unlimited).
 * The value is set at startup from config or defaults to 2048 chars. */
int get_tool_preview_max_len(void)
{
    if (g_tool_preview_max_len <= 0) {
        g_tool_preview_max_len = 2048;  /* default preview length */
    }
    return g_tool_preview_max_len;
}

/* ================================================================
 *  Cute tool message — formatted tool completion line
 *  Port of Python display.py:get_cute_tool_message()
 * ================================================================ */

/* Static helpers for display_cute_tool_message.

 * Truncate a string to at most `limit` chars. If `limit` is 0, returns s
 * unchanged (no limit). When `path_mode` is true, uses "..." prefix for
 * overlong paths instead of "..." suffix. Returns pointer to static buf. */
static const char *_trunc_or_path(const char *s, int limit, bool path_mode) {
    if (!s) return "";
    if (limit <= 0) return s;
    size_t len = strlen(s);
    if (len <= (size_t)limit) return s;
    /* Use static buffer — single caller per line, safe */
    static char buf[256];
    if (path_mode) {
        snprintf(buf, sizeof(buf), "...%s", s + len - (size_t)(limit - 3));
    } else {
        snprintf(buf, sizeof(buf), "%.*s...", limit - 3, s);
    }
    return buf;
}

/* Generate a formatted tool completion line for CLI quiet mode.
 * Format: "| {emoji} {verb:9} {detail}  {duration}"
 * Port of Python display.py:get_cute_tool_message().
 * Returns malloc'd string (caller free) or NULL on error. */
char *display_cute_tool_message(const char *tool_name, const char *args_json,
                                double duration, const char *result_json) {
    if (!tool_name) return NULL;

    char dur_buf[32];
    snprintf(dur_buf, sizeof(dur_buf), "%.1fs", duration);

    char suffix_buf[128] = "";
    bool is_failure = display_detect_tool_failure(tool_name, result_json, suffix_buf, sizeof(suffix_buf));

    const char *skin_prefix = display_get_skin_tool_prefix();

    /* Parse args */
    json_t *args = NULL;
    if (args_json) {
        args = json_parse(args_json, NULL);
        if (args && args->type != JSON_OBJECT) { json_free(args); args = NULL; }
    }

    char line[4096];
    line[0] = '\0';

    /* Build the format line per tool */
    if (strcmp(tool_name, "web_search") == 0) {
        const char *q = json_get_str(args, "query", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x94\x8d search    %s  %s",
                 skin_prefix, _trunc_or_path(q, 42, false), dur_buf);
    } else if (strcmp(tool_name, "web_extract") == 0) {
        json_t *urls = json_obj_get(args, "urls");
        if (urls && urls->type == JSON_ARRAY && json_len(urls) > 0) {
            json_t *first = json_get(urls, 0);
            if (first && first->type == JSON_STRING) {
                const char *url = first->str_val;
                const char *domain = url;
                if (strncmp(url, "https://", 8) == 0) domain = url + 8;
                else if (strncmp(url, "http://", 7) == 0) domain = url + 7;
                const char *slash = strchr(domain, '/');
                char domain_buf[128];
                if (slash) {
                    size_t dlen = (size_t)(slash - domain);
                    if (dlen > sizeof(domain_buf)-1) dlen = sizeof(domain_buf)-1;
                    memcpy(domain_buf, domain, dlen);
                    domain_buf[dlen] = '\0';
                    domain = domain_buf;
                }
                size_t count = json_len(urls);
                if (count > 1) {
                    snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x84 fetch     %s +%zu  %s",
                             skin_prefix, _trunc_or_path(domain, 35, false), count - 1, dur_buf);
                } else {
                    snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x84 fetch     %s  %s",
                             skin_prefix, _trunc_or_path(domain, 35, false), dur_buf);
                }
            }
        }
        if (!line[0]) {
            snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x84 fetch     pages  %s", skin_prefix, dur_buf);
        }
    } else if (strcmp(tool_name, "terminal") == 0) {
        const char *cmd = json_get_str(args, "command", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x92\xbb $         %s  %s",
                 skin_prefix, _trunc_or_path(cmd, 42, false), dur_buf);
    } else if (strcmp(tool_name, "process") == 0) {
        const char *action = json_get_str(args, "action", "?");
        const char *sid = json_get_str(args, "session_id", "");
        char sid12[13];
        snprintf(sid12, sizeof(sid12), "%.12s", sid);
        const char *label = NULL;
        char label_buf[128];
        if (strcmp(action, "list") == 0) label = "ls processes";
        else if (strcmp(action, "poll") == 0) { snprintf(label_buf, sizeof(label_buf), "poll %s", sid12); label = label_buf; }
        else if (strcmp(action, "log") == 0) { snprintf(label_buf, sizeof(label_buf), "log %s", sid12); label = label_buf; }
        else if (strcmp(action, "wait") == 0) { snprintf(label_buf, sizeof(label_buf), "wait %s", sid12); label = label_buf; }
        else if (strcmp(action, "kill") == 0) { snprintf(label_buf, sizeof(label_buf), "kill %s", sid12); label = label_buf; }
        else if (strcmp(action, "write") == 0) { snprintf(label_buf, sizeof(label_buf), "write %s", sid12); label = label_buf; }
        else if (strcmp(action, "submit") == 0) { snprintf(label_buf, sizeof(label_buf), "submit %s", sid12); label = label_buf; }
        else { snprintf(label_buf, sizeof(label_buf), "%s %s", action, sid12); label = label_buf; }
        snprintf(line, sizeof(line), "%s \xe2\x9a\x99\xef\xb8\x8f  proc      %s  %s",
                 skin_prefix, label, dur_buf);
    } else if (strcmp(tool_name, "read_file") == 0) {
        const char *p = json_get_str(args, "path", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x96 read      %s  %s",
                 skin_prefix, _trunc_or_path(p, g_tool_preview_max_len > 0 ? g_tool_preview_max_len : 35, true), dur_buf);
    } else if (strcmp(tool_name, "write_file") == 0) {
        const char *p = json_get_str(args, "path", "");
        snprintf(line, sizeof(line), "%s \xe2\x9c\x8d\xef\xb8\x8f  write     %s  %s",
                 skin_prefix, _trunc_or_path(p, g_tool_preview_max_len > 0 ? g_tool_preview_max_len : 35, true), dur_buf);
    } else if (strcmp(tool_name, "patch") == 0) {
        const char *p = json_get_str(args, "path", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x94\xa7 patch     %s  %s",
                 skin_prefix, _trunc_or_path(p, g_tool_preview_max_len > 0 ? g_tool_preview_max_len : 35, true), dur_buf);
    } else if (strcmp(tool_name, "search_files") == 0) {
        const char *pattern = json_get_str(args, "pattern", "");
        const char *target = json_get_str(args, "target", "content");
        bool is_files = (strcmp(target, "files") == 0);
        snprintf(line, sizeof(line), "%s \xf0\x9f\x94\x8e %s %s  %s",
                 skin_prefix, is_files ? "find      " : "grep      ",
                 _trunc_or_path(pattern, 35, false), dur_buf);
    } else if (strcmp(tool_name, "browser_navigate") == 0) {
        const char *url = json_get_str(args, "url", "");
        const char *domain = url;
        if (strncmp(url, "https://", 8) == 0) domain = url + 8;
        else if (strncmp(url, "http://", 7) == 0) domain = url + 7;
        char domain_buf[128];
        const char *slash = strchr(domain, '/');
        if (slash) {
            size_t dlen = (size_t)(slash - domain);
            if (dlen > sizeof(domain_buf)-1) dlen = sizeof(domain_buf)-1;
            memcpy(domain_buf, domain, dlen);
            domain_buf[dlen] = '\0';
            domain = domain_buf;
        }
        snprintf(line, sizeof(line), "%s \xf0\x9f\x8c\x90 navigate  %s  %s",
                 skin_prefix, _trunc_or_path(domain, 35, false), dur_buf);
    } else if (strcmp(tool_name, "browser_snapshot") == 0) {
        double full_flag = json_get_num(args, "full", 0);
        snprintf(line, sizeof(line), "%s \xf0\x9f\x93\xb8 snapshot  %s  %s",
                 skin_prefix, (full_flag != 0) ? "full" : "compact", dur_buf);
    } else if (strcmp(tool_name, "browser_click") == 0) {
        const char *ref = json_get_str(args, "ref", "?");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x91\x86 click     %s  %s", skin_prefix, ref, dur_buf);
    } else if (strcmp(tool_name, "browser_type") == 0) {
        const char *text = json_get_str(args, "text", "");
        snprintf(line, sizeof(line), "%s \xe2\x8c\xa8\xef\xb8\x8f  type      \"%s\"  %s",
                 skin_prefix, _trunc_or_path(text, 30, false), dur_buf);
    } else if (strcmp(tool_name, "browser_scroll") == 0) {
        const char *d = json_get_str(args, "direction", "down");
        const char *arrow = "\xe2\x86\x93"; /* ↓ */
        if (strcmp(d, "up") == 0) arrow = "\xe2\x86\x91";       /* ↑ */
        else if (strcmp(d, "right") == 0) arrow = "\xe2\x86\x92"; /* → */
        else if (strcmp(d, "left") == 0) arrow = "\xe2\x86\x90";   /* ← */
        snprintf(line, sizeof(line), "%s %s  scroll    %s  %s", skin_prefix, arrow, d, dur_buf);
    } else if (strcmp(tool_name, "browser_back") == 0) {
        snprintf(line, sizeof(line), "%s \xe2\x97\x80\xef\xb8\x8f  back      %s", skin_prefix, dur_buf);
    } else if (strcmp(tool_name, "browser_press") == 0) {
        const char *key = json_get_str(args, "key", "?");
        snprintf(line, sizeof(line), "%s \xe2\x8c\xa8\xef\xb8\x8f  press     %s  %s", skin_prefix, key, dur_buf);
    } else if (strcmp(tool_name, "browser_get_images") == 0) {
        snprintf(line, sizeof(line), "%s \xf0\x9f\x96\xbc\xef\xb8\x8f  images    extracting  %s", skin_prefix, dur_buf);
    } else if (strcmp(tool_name, "browser_vision") == 0) {
        snprintf(line, sizeof(line), "%s \xf0\x9f\x91\x81\xef\xb8\x8f  vision    analyzing page  %s", skin_prefix, dur_buf);
    } else if (strcmp(tool_name, "todo") == 0) {
        json_t *todos = json_obj_get(args, "todos");
        int merge_flag = 0;
        const char *m = json_get_str(args, "merge", NULL);
        if (m && (strcmp(m, "true") == 0 || strcmp(m, "1") == 0)) merge_flag = 1;

        /* Try to parse result for completion progress */
        int total = 0, done = 0;
        if (result_json) {
            json_t *rdata = json_parse(result_json, NULL);
            if (rdata && rdata->type == JSON_OBJECT) {
                json_t *summary = json_obj_get(rdata, "summary");
                if (summary && summary->type == JSON_OBJECT) {
                    total = (int)json_get_num(summary, "total", 0);
                    done = (int)json_get_num(summary, "completed", 0);
                }
            }
            json_free(rdata);
        }

        if (!todos) {
            if (total > 0)
                snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x8b plan      %d/%d task(s)  %s", skin_prefix, done, total, dur_buf);
            else
                snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x8b plan      reading tasks  %s", skin_prefix, dur_buf);
        } else if (merge_flag) {
            if (total > 0 && done > 0)
                snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x8b plan      update %d/%d \xe2\x9c\x93  %s", skin_prefix, done, total, dur_buf);
            else
                snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x8b plan      update %zu task(s)  %s", skin_prefix, json_len(todos), dur_buf);
        } else {
            if (total > 0 && done > 0)
                snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x8b plan      %d/%d task(s)  %s", skin_prefix, done, total, dur_buf);
            else
                snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x8b plan      %zu task(s)  %s", skin_prefix, json_len(todos), dur_buf);
        }
    } else if (strcmp(tool_name, "session_search") == 0) {
        const char *query = json_get_str(args, "query", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x94\x8d recall    \"%s\"  %s",
                 skin_prefix, _trunc_or_path(query, 35, false), dur_buf);
    } else if (strcmp(tool_name, "memory") == 0) {
        const char *action = json_get_str(args, "action", "?");
        const char *target = json_get_str(args, "target", "");
        if (strcmp(action, "add") == 0) {
            const char *content = json_get_str(args, "content", "");
            snprintf(line, sizeof(line), "%s \xf0\x9f\xa7\xa0 memory    +%s: \"%s\"  %s",
                     skin_prefix, target, _trunc_or_path(content, 30, false), dur_buf);
        } else if (strcmp(action, "replace") == 0) {
            const char *old = json_get_str(args, "old_text", NULL);
            if (!old) old = "<missing old_text>";
            snprintf(line, sizeof(line), "%s \xf0\x9f\xa7\xa0 memory    ~%s: \"%s\"  %s",
                     skin_prefix, target, _trunc_or_path(old, 20, false), dur_buf);
        } else if (strcmp(action, "remove") == 0) {
            const char *old = json_get_str(args, "old_text", NULL);
            if (!old) old = "<missing old_text>";
            snprintf(line, sizeof(line), "%s \xf0\x9f\xa7\xa0 memory    -%s: \"%s\"  %s",
                     skin_prefix, target, _trunc_or_path(old, 20, false), dur_buf);
        } else {
            snprintf(line, sizeof(line), "%s \xf0\x9f\xa7\xa0 memory    %s  %s", skin_prefix, action, dur_buf);
        }
    } else if (strcmp(tool_name, "skills_list") == 0) {
        const char *cat = json_get_str(args, "category", "all");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x9a skills    list %s  %s", skin_prefix, cat, dur_buf);
    } else if (strcmp(tool_name, "skill_view") == 0) {
        const char *name = json_get_str(args, "name", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x93\x9a skill     %s  %s",
                 skin_prefix, _trunc_or_path(name, 30, false), dur_buf);
    } else if (strcmp(tool_name, "image_generate") == 0) {
        const char *prompt = json_get_str(args, "prompt", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x8e\xa8 create    %s  %s",
                 skin_prefix, _trunc_or_path(prompt, 35, false), dur_buf);
    } else if (strcmp(tool_name, "text_to_speech") == 0) {
        const char *text = json_get_str(args, "text", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x94\x8a speak     %s  %s",
                 skin_prefix, _trunc_or_path(text, 30, false), dur_buf);
    } else if (strcmp(tool_name, "vision_analyze") == 0) {
        const char *question = json_get_str(args, "question", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x91\x81\xef\xb8\x8f  vision    %s  %s",
                 skin_prefix, _trunc_or_path(question, 30, false), dur_buf);
    } else if (strcmp(tool_name, "mixture_of_agents") == 0) {
        const char *prompt = json_get_str(args, "user_prompt", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\xa7\xa0 reason    %s  %s",
                 skin_prefix, _trunc_or_path(prompt, 30, false), dur_buf);
    } else if (strcmp(tool_name, "send_message") == 0) {
        const char *target = json_get_str(args, "target", "?");
        const char *msg = json_get_str(args, "message", "");
        snprintf(line, sizeof(line), "%s \xf0\x9f\x93\xa8 send      %s: \"%s\"  %s",
                 skin_prefix, target, _trunc_or_path(msg, 25, false), dur_buf);
    } else if (strcmp(tool_name, "cronjob") == 0) {
        const char *action = json_get_str(args, "action", "?");
        if (strcmp(action, "create") == 0) {
            const char *name = json_get_str(args, "name", NULL);
            json_t *skills = json_obj_get(args, "skills");
            const char *prompt = json_get_str(args, "prompt", NULL);
            const char *label = NULL;
            char label_buf[128];
            if (name) label = name;
            else if (skills && json_len(skills) > 0) {
                json_t *first = json_get(skills, 0);
                if (first && first->type == JSON_STRING) label = first->str_val;
            }
            else if (prompt) {
                snprintf(label_buf, sizeof(label_buf), "%s", prompt);
                label = label_buf;
            }
            else label = "task";
            snprintf(line, sizeof(line), "%s \xe2\x8f\xb0 cron      create %s  %s",
                     skin_prefix, _trunc_or_path(label, 24, false), dur_buf);
        } else if (strcmp(action, "list") == 0) {
            snprintf(line, sizeof(line), "%s \xe2\x8f\xb0 cron      listing  %s", skin_prefix, dur_buf);
        } else {
            const char *jid = json_get_str(args, "job_id", "");
            snprintf(line, sizeof(line), "%s \xe2\x8f\xb0 cron      %s %s  %s", skin_prefix, action, jid, dur_buf);
        }
    } else if (strcmp(tool_name, "execute_code") == 0) {
        const char *code = json_get_str(args, "code", "");
        char first_line[128];
        display_oneline(code, first_line, sizeof(first_line));
        /* Take only first line */
        char *nl = strchr(first_line, '\n');
        if (nl) *nl = '\0';
        snprintf(line, sizeof(line), "%s \xf0\x9f\x90\x8d exec      %s  %s",
                 skin_prefix, _trunc_or_path(first_line, 35, false), dur_buf);
    } else if (strcmp(tool_name, "delegate_task") == 0) {
        json_t *tasks = json_obj_get(args, "tasks");
        if (tasks && tasks->type == JSON_ARRAY) {
            snprintf(line, sizeof(line), "%s \xf0\x9f\x94\x80 delegate  %zu parallel tasks  %s",
                     skin_prefix, json_len(tasks), dur_buf);
        } else {
            const char *goal = json_get_str(args, "goal", "");
            snprintf(line, sizeof(line), "%s \xf0\x9f\x94\x80 delegate  %s  %s",
                     skin_prefix, _trunc_or_path(goal, 35, false), dur_buf);
        }
    }

    /* Fallback for unknown tools */
    if (!line[0]) {
        char *preview = display_tool_preview(tool_name, args_json);
        const char *preview_str = preview ? preview : "";
        snprintf(line, sizeof(line), "%s \xe2\x9a\xa1 %-9s %s  %s",
                 skin_prefix, tool_name, _trunc_or_path(preview_str, 35, false), dur_buf);
        free(preview);
    }

    json_free(args);

    /* Apply failure suffix if needed */
    if (is_failure && suffix_buf[0]) {
        size_t llen = strlen(line);
        snprintf(line + llen, sizeof(line) - llen, "%s", suffix_buf);
    }

    return strdup(line);
}

/* ================================================================
 *  Edit diff path helpers (port of Python display.py)
 * ================================================================ */

/* Port of Python display.py:_resolved_path().
 * Resolve a possibly-relative filesystem path against current cwd.
 * Expands ~/ and makes absolute. Writes into resolved_buf (caller allocates).
 * Returns resolved_buf on success, NULL on error. */
char *display_resolved_path(const char *path, char *resolved_buf, size_t buf_sz) {
    if (!path || !resolved_buf || buf_sz == 0) return NULL;
    char expanded[PATH_MAX];
    /* Expand ~ prefix */
    if (path[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = getenv("USERPROFILE");
        if (!home) {
            snprintf(resolved_buf, buf_sz, "%s", path);
            return resolved_buf;
        }
        if (path[1] == '/' || path[1] == '\0') {
            snprintf(expanded, sizeof(expanded), "%s%s", home, path + 1);
        } else {
            snprintf(expanded, sizeof(expanded), "%s", path);
        }
    } else {
        snprintf(expanded, sizeof(expanded), "%s", path);
    }

    /* If already absolute, realpath it */
    if (expanded[0] == '/') {
        char *rp = realpath(expanded, resolved_buf);
        if (!rp) {
            /* realpath may fail if file doesn't exist — use expanded */
            snprintf(resolved_buf, buf_sz, "%s", expanded);
        }
        return resolved_buf;
    }

    /* Relative: prepend cwd */
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        snprintf(resolved_buf, buf_sz, "%s", expanded);
        return resolved_buf;
    }
    char combined[PATH_MAX * 2];
    snprintf(combined, sizeof(combined), "%s/%s", cwd, expanded);
    char *rp = realpath(combined, resolved_buf);
    if (!rp) {
        snprintf(resolved_buf, buf_sz, "%s", combined);
    }
    return resolved_buf;
}

/* Port of Python display.py:_snapshot_text().
 * Read a file's UTF-8 content. Returns malloc'd string or NULL on error. */
char *display_snapshot_text(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    if (fsize < 0 || fsize > 1024 * 1024) { /* cap at 1MB */
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *content = (char *)malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return NULL; }
    size_t nread = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[nread] = '\0';
    return content;
}

/* Port of Python display.py:_display_diff_path().
 * Given an absolute path, return a cwd-relative version for display.
 * Returns malloc'd string (caller free) or strdup of input. */
char *display_diff_path(const char *abs_path) {
    if (!abs_path) return NULL;
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) return strdup(abs_path);

    size_t cwd_len = strlen(cwd);
    if (strncmp(abs_path, cwd, cwd_len) == 0 && abs_path[cwd_len] == '/') {
        return strdup(abs_path + cwd_len + 1);
    }
    return strdup(abs_path);
}

/* ================================================================
 *  LocalEditSnapshot — pre-tool filesystem snapshot
 *  Port of Python display.py LocalEditSnapshot dataclass
 * ================================================================ */

#define DISPLAY_MAX_SNAPSHOT_PATHS 32

/* The struct is now defined in hermes_display.h */


/* Create a new snapshot */
display_local_edit_snapshot_t *display_snapshot_create(void) {
    display_local_edit_snapshot_t *snap = (display_local_edit_snapshot_t *)calloc(1, sizeof(display_local_edit_snapshot_t));
    return snap;
}

/* Free a snapshot and its contents */
void display_snapshot_free(display_local_edit_snapshot_t *snap) {
    if (!snap) return;
    for (int i = 0; i < snap->count; i++) {
        free(snap->paths[i]);
        free(snap->before[i]);
    }
    free(snap);
}

/* Port of Python display.py:_resolve_local_edit_paths().
 * Resolve local filesystem targets for write-capable tools.
 * Writes resolved paths into snapshot. Returns number of paths added. */
int display_snapshot_resolve_paths(display_local_edit_snapshot_t *snap,
                                    const char *tool_name,
                                    const char *function_args_json) {
    if (!snap || !tool_name || !function_args_json) return 0;

    json_t *args = json_parse(function_args_json, NULL);
    if (!args || args->type != JSON_OBJECT) {
        json_free(args);
        return 0;
    }

    int added = 0;
    char resolved[PATH_MAX];

    if (strcmp(tool_name, "write_file") == 0) {
        const char *path = json_get_str(args, "path", NULL);
        if (path && display_resolved_path(path, resolved, sizeof(resolved))) {
            if (snap->count < DISPLAY_MAX_SNAPSHOT_PATHS) {
                snap->paths[snap->count] = strdup(resolved);
                snap->before[snap->count] = display_snapshot_text(resolved);
                snap->count++;
                added++;
            }
        }
    } else if (strcmp(tool_name, "patch") == 0) {
        const char *path = json_get_str(args, "path", NULL);
        if (path && display_resolved_path(path, resolved, sizeof(resolved))) {
            if (snap->count < DISPLAY_MAX_SNAPSHOT_PATHS) {
                snap->paths[snap->count] = strdup(resolved);
                snap->before[snap->count] = display_snapshot_text(resolved);
                snap->count++;
                added++;
            }
        }
    }
    /* Port of Python display.py:_resolve_skill_manage_paths().
     * Resolve paths from skill_manager tool arguments.
     * The skill_manager tool accepts "action", "skill_name", "source_path"
     * fields — extract the path if source_path is present. */
    if (strcmp(tool_name, "skill_manager") == 0 || strcmp(tool_name, "skills_guard") == 0) {
        json_node_t *source_path = json_object_get(args, "source_path");
        json_node_t *skill_name = json_object_get(args, "skill_name");
        const char *path_str = NULL;
        if (source_path && source_path->type == JSON_STRING)
            path_str = source_path->str_val;
        else if (skill_name && skill_name->type == JSON_STRING)
            path_str = skill_name->str_val;
        if (path_str && *path_str) {
            char resolved[4096];
            /* Expand tilde if present */
            if (path_str[0] == '~') {
                const char *home = getenv("HOME");
                if (home) {
                    snprintf(resolved, sizeof(resolved), "%s%s", home, path_str + 1);
                } else {
                    snprintf(resolved, sizeof(resolved), "%s", path_str);
                }
            } else {
                snprintf(resolved, sizeof(resolved), "%s", path_str);
            }
            /* Check if path is writeable */
            if (access(resolved, W_OK) == 0 || errno == ENOENT) {
                snap->paths[snap->count] = strdup(resolved);
                snap->before[snap->count] = display_snapshot_text(resolved);
                snap->count++;
                added++;
            }
        }
    }

    json_free(args);
    return added;
}

/* Port of Python display.py:capture_local_edit_snapshot().
 * Capture before-state for local write previews.
 * Returns malloc'd snapshot or NULL if no paths to track. */
display_local_edit_snapshot_t *display_capture_local_edit_snapshot(const char *tool_name,
                                                                    const char *function_args_json) {
    display_local_edit_snapshot_t *snap = display_snapshot_create();
    if (!snap) return NULL;

    int added = display_snapshot_resolve_paths(snap, tool_name, function_args_json);
    if (added == 0) {
        display_snapshot_free(snap);
        return NULL;
    }
    return snap;
}

/* Port of Python display.py:_diff_from_snapshot().
 * Generate unified diff text from a stored before-state and current files.
 * Returns malloc'd diff string or NULL (no changes / error). */
char *display_diff_from_snapshot(display_local_edit_snapshot_t *snap) {
    if (!snap || snap->count == 0) return NULL;

    /* Real unified diff via libdifflib (Python: difflib.unified_diff). */
    extern char *difflib_unified_diff(const char *a, const char *b, int context_lines);

    char *all_diffs = NULL;
    size_t all_len = 0;

    for (int i = 0; i < snap->count; i++) {
        const char *path = snap->paths[i];
        const char *before = snap->before[i];
        char *after = display_snapshot_text(path);

        if (before && after && strcmp(before, after) == 0) {
            free(after);
            continue; /* no change */
        }

        char *display_path = display_diff_path(path);
        if (!display_path) display_path = strdup(path);

        /* unified_diff([] if before is None else ..., fromfile=a/<p>, tofile=b/<p>) */
        char *body = difflib_unified_diff(before ? before : "",
                                          after ? after : "", 3);
        if (!body || !body[0]) {
            free(body);
            free(display_path);
            if (after) free(after);
            continue; /* empty diff -> skip chunk, same as Python */
        }

        /* difflib_unified_diff emits generic ---/+++ headers; replace them
         * with the a/<path> b/<path> labels Python passes as
         * fromfile/tofile. Skip up to two leading header lines. */
        const char *hunks = body;
        for (int h = 0; h < 2; h++) {
            if (strncmp(hunks, "--- ", 4) == 0 || strncmp(hunks, "+++ ", 4) == 0) {
                const char *nl = strchr(hunks, '\n');
                if (!nl) break;
                hunks = nl + 1;
            }
        }

        char hdr[512];
        int hdr_len = snprintf(hdr, sizeof(hdr),
                               "--- a/%s\n+++ b/%s\n", display_path, display_path);
        free(display_path);

        size_t hunks_len = strlen(hunks);
        char *merged = (char *)realloc(all_diffs,
                                       all_len + (size_t)hdr_len + hunks_len + 1);
        if (!merged) {
            free(body);
            if (after) free(after);
            continue;
        }
        all_diffs = merged;
        memcpy(all_diffs + all_len, hdr, (size_t)hdr_len);
        all_len += (size_t)hdr_len;
        memcpy(all_diffs + all_len, hunks, hunks_len);
        all_len += hunks_len;
        all_diffs[all_len] = '\0';

        free(body);
        if (after) free(after);
    }

    return all_diffs;
}

/* Port of Python agent/insights.py:_bar_chart(). */
char **display_bar_chart(const int *values, int n, int max_width) {
    if (!values || n <= 0 || max_width <= 0) return NULL;
    int peak = 0;
    for (int i = 0; i < n; i++) if (values[i] > peak) peak = values[i];
    if (peak == 0) peak = 1;
    char **bars = calloc(n, sizeof(char *));
    if (!bars) return NULL;
    for (int i = 0; i < n; i++) {
        int w = values[i] > 0 ? (values[i] * max_width / peak) : 0;
        if (w < 1 && values[i] > 0) w = 1;
        bars[i] = calloc(w + 1, 1);
        if (bars[i]) memset(bars[i], '#', w);
    }
    return bars;
}

void display_bar_chart_free(char **bars, int n) {
    if (!bars) return;
    for (int i = 0; i < n; i++) free(bars[i]);
    free(bars);
}
