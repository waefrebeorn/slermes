#ifndef TUI_STATE_H
#define TUI_STATE_H
/*
 * tui_state.h — the TUI module's own state types (module-internal, not a
 * god header: only tui_fullscreen.c and its future subsystem splits use it).
 *
 * Extracted from tui_fullscreen.c per soul.md ("self-contained modules,
 * opaque structs, minimal includes"). Holds every subsystem state struct +
 * tui_global_state_t + the single tui instance, so each subsystem overlay can
 * later move to its own .c with one shared extern.
 */
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "hermes_agent.h"      /* agent_state_t, session_meta_t             */
#include "chat_render.h"      /* chat_rendered_msg_t                        */
#include "chat_composer.h"    /* composer_t, slash_*                        */

/* Slash command entry for autocomplete (full table defined below).
 * Named tui_slash_cmd_t to avoid colliding with the slash_cmd_t ENUM in
 * chat_composer.h (used by tui_composer_detect_slash). */
typedef struct {
    const char *cmd;
    const char *desc;
    const char *args; /* arg hints */
} tui_slash_cmd_t;

/* Model picker overlay state (instance + logic defined below). */
typedef struct {
    char names[MODEL_PICKER_MAX][MODEL_NAME_MAX];
    int  count;
    int  selected;
    int  scroll_offset;
} model_picker_state_t;


/* ── TUI Chat Render Integration ────────────────────────────────────────── */
/* Uses chat_render.c for message display with markdown + syntax highlighting */

static chat_rendered_msg_t *tui_render_cache[MAX_MESSAGES_DISPLAY];
static int tui_render_cache_count = 0;

/* Render a message using chat_render and cache it */
static void tui_render_message(const char *text, const char *role) {
    if (!text || !role) return;
    chat_rendered_msg_t *rendered = chat_render_message(text, role);
    if (!rendered) return;
    if (tui_render_cache_count < MAX_MESSAGES_DISPLAY) {
        tui_render_cache[tui_render_cache_count++] = rendered;
    } else {
        /* Shift cache: free oldest, shift down */
        chat_render_free(tui_render_cache[0]);
        for (int i = 0; i < MAX_MESSAGES_DISPLAY - 1; i++) {
            tui_render_cache[i] = tui_render_cache[i + 1];
        }
        tui_render_cache[MAX_MESSAGES_DISPLAY - 1] = rendered;
    }
}

/* Render a code block with syntax highlighting */
static void tui_render_code_block(const char *code, const char *lang) {
    if (!code) return;
    chat_rendered_msg_t *rendered = chat_render_highlight(code, lang ? lang : "text");
    if (!rendered) return;
    if (tui_render_cache_count < MAX_MESSAGES_DISPLAY) {
        tui_render_cache[tui_render_cache_count++] = rendered;
    } else {
        chat_render_free(tui_render_cache[0]);
        for (int i = 0; i < MAX_MESSAGES_DISPLAY - 1; i++) {
            tui_render_cache[i] = tui_render_cache[i + 1];
        }
        tui_render_cache[MAX_MESSAGES_DISPLAY - 1] = rendered;
    }
}

/* Free all cached rendered messages */
static void tui_render_cache_free(void) {
    for (int i = 0; i < tui_render_cache_count; i++) {
        if (tui_render_cache[i]) {
            chat_render_free(tui_render_cache[i]);
            tui_render_cache[i] = NULL;
        }
    }
    tui_render_cache_count = 0;
}

/* ── TUI Chat Composer Integration ──────────────────────────────────────── */
/* Uses chat_composer.c for text editing, slash commands, attachments */

static composer_t *tui_composer = NULL;
static slash_context_t tui_slash_ctx;

/* Initialize the TUI composer */
static void tui_composer_init(void) {
    if (tui_composer) return;
    tui_composer = composer_create();
    if (tui_composer) {
        tui_composer->multiline = true;
        memset(&tui_slash_ctx, 0, sizeof(tui_slash_ctx));
        tui_slash_ctx.composer = tui_composer;
        tui_slash_ctx.app_ctx = NULL;
    }
}

/* Shutdown the TUI composer */
static void tui_composer_shutdown(void) {
    if (tui_composer) {
        composer_dispose(tui_composer);
        tui_composer = NULL;
    }
}

/* Insert text into the TUI composer */
static void tui_composer_insert(const char *text) {
    if (!tui_composer || !text) return;
    composer_insert(tui_composer, text);
    composer_update_suggestions(tui_composer);
}

/* Submit TUI composer text — returns text if message, NULL if slash command handled */
static char *tui_composer_submit(void) {
    if (!tui_composer) return NULL;
    char *text = NULL;
    slash_result_t result = composer_submit_with_slash(tui_composer, &tui_slash_ctx, &text);
    if (result == SLASH_RESULT_OK || result == SLASH_RESULT_NEW_SESSION) {
        /* Slash command handled, free text if any */
        if (text) { free(text); text = NULL; }
        return NULL;
    }
    return text; /* Regular message text */
}

/* Detect slash command in TUI input */
static slash_cmd_t tui_composer_detect_slash(void) {
    if (!tui_composer) return SLASH_NONE;
    return composer_detect_slash(tui_composer);
}

/* Get composer text for display */
static const char *tui_composer_get_text(void) {
    if (!tui_composer) return "";
    return composer_get_text(tui_composer);
}

/* Get composer cursor position */
static int tui_composer_get_cursor(void) {
    if (!tui_composer) return 0;
    return tui_composer->cursor_pos;
}

/* ── Terminal Search Integration ────────────────────────────────────────── */
/* Search within TUI message history */

static char tui_search_query[256] = "";
static int tui_search_active = 0;
static int tui_search_match_count = 0;

/* Activate search mode */
static void tui_search_activate(void) {
    tui_search_active = 1;
    tui_search_query[0] = '\0';
}

/* Deactivate search mode */
static void tui_search_deactivate(void) {
    tui_search_active = 0;
}

/* Find next match in rendered messages */
static int tui_search_find_next(const char *query) {
    if (!query || !query[0]) return -1;
    strncpy(tui_search_query, query, sizeof(tui_search_query) - 1);
    tui_search_match_count = 0;
    for (int i = 0; i < tui_render_cache_count; i++) {
        if (!tui_render_cache[i]) continue;
        char *plain = chat_render_plain_text(tui_render_cache[i]);
        if (plain) {
            if (strcasestr(plain, query) != NULL) {
                tui_search_match_count++;
            }
            free(plain);
        }
    }
    return tui_search_match_count;
}

/* ── Session Switching (F1-F12) ─────────────────────────────────────────── */

static int tui_session_hotkey = -1; /* F1=0, F2=1, ... F11=10 */

/* Handle F-key session switch */
static void tui_session_switch(int fkey) {
    if (fkey < 0 || fkey > 11) return;
    tui_session_hotkey = fkey;
    /* Signal session browser to activate */
    /* Actual session switch handled by main loop */
}

/* ── Model Picker Overlay ───────────────────────────────────────────────── */
/* NOTE: the real model-picker state lives in the main `tui` struct
 * (tui.model_picker) and its init/draw/input live further down the file
 * (see tui_model_picker_init at the T13 section). The earlier standalone
 * tui_model_picker global + toggle was dead duplicate code (never wired to
 * the draw/input path) and has been removed to fix the duplicate-symbol
 * build break. */

/* ── Notification Display in Status Bar ─────────────────────────────────── */

#define TUI_MAX_NOTIFICATIONS 16
#define TUI_NOTIFICATION_LEN 256

typedef struct {
    char text[TUI_NOTIFICATION_LEN];
    int color_pair;
    time_t timestamp;
    int duration_sec;
} tui_notification_t;

static tui_notification_t tui_notifications[TUI_MAX_NOTIFICATIONS];
static int tui_notification_count = 0;

static void tui_notification_add(const char *text, int color_pair, int duration_sec) {
    if (!text || tui_notification_count >= TUI_MAX_NOTIFICATIONS) return;
    tui_notification_t *n = &tui_notifications[tui_notification_count++];
    strncpy(n->text, text, TUI_NOTIFICATION_LEN - 1);
    n->color_pair = color_pair;
    n->timestamp = time(NULL);
    n->duration_sec = duration_sec;
}

static void tui_notification_expire(void) {
    time_t now = time(NULL);
    int write = 0;
    for (int i = 0; i < tui_notification_count; i++) {
        if (now - tui_notifications[i].timestamp < tui_notifications[i].duration_sec) {
            tui_notifications[write++] = tui_notifications[i];
        }
    }
    tui_notification_count = write;
}

/* Get the most recent notification, or NULL if none */
static const char *tui_notification_latest(void) {
    if (tui_notification_count <= 0) return NULL;
    return tui_notifications[tui_notification_count - 1].text;
}

/* Forward declarations for TUI subsystems (defined later, used earlier) */
static void tui_slash_init_all(void);
static void tui_model_picker_init(void);
static void tui_draw_model_picker(void);
static bool tui_gateway_connect(void);
static void tui_gateway_send(const char *method, const char *params);
static void tui_gateway_process_message(const char *msg);

/* ==================================================================
 *  P198: THEME ENGINE — color scheme definitions and skin files
 * ================================================================== */

#define TUI_MAX_THEMES 16
#define TUI_THEME_NAME_MAX 64
#define TUI_SKIN_DIR "skins"

/* Theme: defines all color pairs used by the TUI */
typedef struct {
    char name[TUI_THEME_NAME_MAX];

    /* Color pair indices */
    int status_bg, status_fg;       /* status bar */
    int prompt_bg, prompt_fg;       /* prompt text */
    int user_bg, user_fg;           /* user messages */
    int assistant_bg, assistant_fg; /* assistant messages */
    int system_bg, system_fg;       /* system messages */
    int tool_bg, tool_fg;           /* tool output */
    int error_bg, error_fg;         /* errors */
    int warn_bg, warn_fg;           /* warnings */
    int hl_bg, hl_fg;               /* syntax highlight */
    int dim_bg, dim_fg;             /* dim/less important */
    int border_bg, border_fg;       /* borders */
    int tool_feed_bg, tool_feed_fg; /* tool feed pane */
    int sel_bg, sel_fg;             /* selection highlight */

    /* Symbols */
    char prompt_sym[8];
    char tool_sym[8];
    char think_sym[8];
    char error_sym[8];
    char ok_sym[8];
} tui_theme_t;

/* Built-in themes */
static const tui_theme_t tui_theme_default = {
    .name = "default",
    .status_bg = COLOR_BLUE, .status_fg = COLOR_WHITE,
    .prompt_bg = COLOR_BLACK, .prompt_fg = COLOR_GREEN,
    .user_bg = COLOR_BLACK, .user_fg = COLOR_CYAN,
    .assistant_bg = COLOR_BLACK, .assistant_fg = COLOR_WHITE,
    .system_bg = COLOR_BLACK, .system_fg = COLOR_MAGENTA,
    .tool_bg = COLOR_BLACK, .tool_fg = COLOR_YELLOW,
    .error_bg = COLOR_BLACK, .error_fg = COLOR_RED,
    .warn_bg = COLOR_BLACK, .warn_fg = COLOR_YELLOW,
    .hl_bg = COLOR_BLACK, .hl_fg = COLOR_GREEN,
    .dim_bg = COLOR_BLACK, .dim_fg = COLOR_WHITE,
    .border_bg = COLOR_BLUE, .border_fg = COLOR_CYAN,
    .tool_feed_bg = COLOR_BLACK, .tool_feed_fg = COLOR_YELLOW,
    .sel_bg = COLOR_CYAN, .sel_fg = COLOR_BLACK,
    .prompt_sym = ">",
    .tool_sym = "\u2699",
    .think_sym = "\u22EF",
    .error_sym = "\u2716",
    .ok_sym = "\u2714",
};

static const tui_theme_t tui_theme_dark = {
    .name = "dark",
    .status_bg = COLOR_BLACK, .status_fg = COLOR_WHITE,
    .prompt_bg = COLOR_BLACK, .prompt_fg = COLOR_GREEN,
    .user_bg = COLOR_BLACK, .user_fg = COLOR_BLUE,
    .assistant_bg = COLOR_BLACK, .assistant_fg = COLOR_WHITE,
    .system_bg = COLOR_BLACK, .system_fg = COLOR_MAGENTA,
    .tool_bg = COLOR_BLACK, .tool_fg = COLOR_YELLOW,
    .error_bg = COLOR_BLACK, .error_fg = COLOR_RED,
    .warn_bg = COLOR_BLACK, .warn_fg = COLOR_YELLOW,
    .hl_bg = COLOR_BLACK, .hl_fg = COLOR_GREEN,
    .dim_bg = COLOR_BLACK, .dim_fg = COLOR_WHITE,
    .border_bg = COLOR_BLACK, .border_fg = COLOR_CYAN,
    .tool_feed_bg = COLOR_BLACK, .tool_feed_fg = COLOR_YELLOW,
    .sel_bg = COLOR_BLUE, .sel_fg = COLOR_WHITE,
    .prompt_sym = "\u03BB",
    .tool_sym = "\u2699",
    .think_sym = "\u22EF",
    .error_sym = "\u2716",
    .ok_sym = "\u2714",
};

static const tui_theme_t tui_theme_light = {
    .name = "light",
    .status_bg = COLOR_WHITE, .status_fg = COLOR_BLACK,
    .prompt_bg = COLOR_WHITE, .prompt_fg = COLOR_GREEN,
    .user_bg = COLOR_WHITE, .user_fg = COLOR_BLUE,
    .assistant_bg = COLOR_WHITE, .assistant_fg = COLOR_BLACK,
    .system_bg = COLOR_WHITE, .system_fg = COLOR_MAGENTA,
    .tool_bg = COLOR_WHITE, .tool_fg = COLOR_YELLOW,
    .error_bg = COLOR_WHITE, .error_fg = COLOR_RED,
    .warn_bg = COLOR_WHITE, .warn_fg = COLOR_MAGENTA,
    .hl_bg = COLOR_WHITE, .hl_fg = COLOR_GREEN,
    .dim_bg = COLOR_WHITE, .dim_fg = COLOR_BLACK,
    .border_bg = COLOR_WHITE, .border_fg = COLOR_CYAN,
    .tool_feed_bg = COLOR_WHITE, .tool_feed_fg = COLOR_YELLOW,
    .sel_bg = COLOR_CYAN, .sel_fg = COLOR_WHITE,
    .prompt_sym = ">",
    .tool_sym = "*",
    .think_sym = "...",
    .error_sym = "X",
    .ok_sym = "OK",
};

static const tui_theme_t tui_theme_mono = {
    .name = "mono",
    .status_bg = COLOR_BLACK, .status_fg = COLOR_WHITE,
    .prompt_bg = COLOR_BLACK, .prompt_fg = COLOR_WHITE,
    .user_bg = COLOR_BLACK, .user_fg = COLOR_WHITE,
    .assistant_bg = COLOR_BLACK, .assistant_fg = COLOR_WHITE,
    .system_bg = COLOR_BLACK, .system_fg = COLOR_WHITE,
    .tool_bg = COLOR_BLACK, .tool_fg = COLOR_WHITE,
    .error_bg = COLOR_BLACK, .error_fg = COLOR_WHITE,
    .warn_bg = COLOR_BLACK, .warn_fg = COLOR_WHITE,
    .hl_bg = COLOR_BLACK, .hl_fg = COLOR_WHITE,
    .dim_bg = COLOR_BLACK, .dim_fg = COLOR_WHITE,
    .border_bg = COLOR_BLACK, .border_fg = COLOR_WHITE,
    .tool_feed_bg = COLOR_BLACK, .tool_feed_fg = COLOR_WHITE,
    .sel_bg = COLOR_WHITE, .sel_fg = COLOR_BLACK,
    .prompt_sym = ">",
    .tool_sym = ">",
    .think_sym = "...",
    .error_sym = "!",
    .ok_sym = "v",
};

/* Theme registry */
static const tui_theme_t *tui_themes[TUI_MAX_THEMES];
static int tui_theme_count = 0;
static int tui_current_theme = 0; /* index into themes array */

/* Color pair tracking — next available pair */
static int tui_next_pair = 100;

/* Initialize theme system */
static void tui_theme_init(void) {
    tui_theme_count = 0;
    tui_themes[tui_theme_count++] = &tui_theme_default;
    tui_themes[tui_theme_count++] = &tui_theme_dark;
    tui_themes[tui_theme_count++] = &tui_theme_light;
    tui_themes[tui_theme_count++] = &tui_theme_mono;
    tui_current_theme = 0;
}

/* Allocate color pair */
static int tui_alloc_pair(int fg, int bg) {
    int pair = tui_next_pair++;
    if (pair > COLOR_PAIRS - 1) pair = 100; /* wrap */
    init_pair(pair, fg, bg);
    return pair;
}

/* Apply a theme: register all color pairs using allocator */
static void tui_apply_theme(const tui_theme_t *th) {
    if (!th) return;
    /* Reset allocator so pairs 1-13 map to known conventions */
    tui_next_pair = 1;
    tui_alloc_pair(th->status_fg, th->status_bg);       /* 1 = status */
    tui_alloc_pair(th->prompt_fg, th->prompt_bg);       /* 2 = prompt */
    tui_alloc_pair(th->user_fg, th->user_bg);           /* 3 = user */
    tui_alloc_pair(th->assistant_fg, th->assistant_bg); /* 4 = assistant */
    tui_alloc_pair(th->system_fg, th->system_bg);       /* 5 = system */
    tui_alloc_pair(th->tool_fg, th->tool_bg);           /* 6 = tool */
    tui_alloc_pair(th->error_fg, th->error_bg);         /* 7 = error */
    tui_alloc_pair(th->warn_fg, th->warn_bg);           /* 8 = warn */
    tui_alloc_pair(th->hl_fg, th->hl_bg);               /* 9 = hl */
    tui_alloc_pair(th->dim_fg, th->dim_bg);             /* 10 = dim */
    tui_alloc_pair(th->border_fg, th->border_bg);       /* 11 = border */
    tui_alloc_pair(th->tool_feed_fg, th->tool_feed_bg); /* 12 = tool_feed */
    tui_alloc_pair(th->sel_fg, th->sel_bg);             /* 13 = selection */
    if (tui_next_pair < 14) tui_next_pair = 14; /* ensure next free pair */
}

/* Load skin from JSON file */
static bool tui_load_skin_file(const char *path, tui_theme_t *th) {
    if (!path || !th) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;

    /* Parse simple JSON skin file */
    char line[512];
    th->name[0] = '\0';
    /* Copy defaults first */
    *th = tui_theme_default;

    while (fgets(line, sizeof(line), f)) {
        char key[128], val[128];
        if (sscanf(line, " \"%127[^\"]\" : \"%127[^\"]\"", key, val) == 2) {
            /* Map JSON keys to theme fields */
            if (strcmp(key, "name") == 0)
                strncpy(th->name, val, TUI_THEME_NAME_MAX - 1);
            else if (strcmp(key, "colors.status_bg") == 0) th->status_bg = atoi(val);
            else if (strcmp(key, "colors.status_fg") == 0) th->status_fg = atoi(val);
            else if (strcmp(key, "colors.prompt_bg") == 0) th->prompt_bg = atoi(val);
            else if (strcmp(key, "colors.prompt_fg") == 0) th->prompt_fg = atoi(val);
            else if (strcmp(key, "colors.user_bg") == 0) th->user_bg = atoi(val);
            else if (strcmp(key, "colors.user_fg") == 0) th->user_fg = atoi(val);
            else if (strcmp(key, "colors.assistant_bg") == 0) th->assistant_bg = atoi(val);
            else if (strcmp(key, "colors.assistant_fg") == 0) th->assistant_fg = atoi(val);
            else if (strcmp(key, "colors.error_bg") == 0) th->error_bg = atoi(val);
            else if (strcmp(key, "colors.error_fg") == 0) th->error_fg = atoi(val);
            else if (strcmp(key, "colors.warn_bg") == 0) th->warn_bg = atoi(val);
            else if (strcmp(key, "colors.warn_fg") == 0) th->warn_fg = atoi(val);
            else if (strcmp(key, "colors.tool_bg") == 0) th->tool_bg = atoi(val);
            else if (strcmp(key, "colors.tool_fg") == 0) th->tool_fg = atoi(val);
            else if (strcmp(key, "colors.border_bg") == 0) th->border_bg = atoi(val);
            else if (strcmp(key, "colors.border_fg") == 0) th->border_fg = atoi(val);
            else if (strcmp(key, "colors.sel_bg") == 0) th->sel_bg = atoi(val);
            else if (strcmp(key, "colors.sel_fg") == 0) th->sel_fg = atoi(val);
            else if (strcmp(key, "symbols.prompt") == 0)
                strncpy(th->prompt_sym, val, sizeof(th->prompt_sym) - 1);
            else if (strcmp(key, "symbols.tool") == 0)
                strncpy(th->tool_sym, val, sizeof(th->tool_sym) - 1);
            else if (strcmp(key, "symbols.thinking") == 0)
                strncpy(th->think_sym, val, sizeof(th->think_sym) - 1);
        }
    }
    fclose(f);
    return th->name[0] != '\0';
}

/* Load external skin files from ~/.slermes/skins/ */
static void tui_load_external_skins(void) {
    char skin_dir[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(skin_dir, sizeof(skin_dir), "%s/%s", home, TUI_SKIN_DIR);

    DIR *d = opendir(skin_dir);
    if (!d) {
        /* Try ~/.slermes/skins/ */
        if (!home) home = ".";
        snprintf(skin_dir, sizeof(skin_dir), "%s/.slermes/%s", home, TUI_SKIN_DIR);
        d = opendir(skin_dir);
        if (!d) return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && tui_theme_count < TUI_MAX_THEMES) {
        size_t len = strlen(entry->d_name);
        if (len < 6 || strcmp(entry->d_name + len - 5, ".json") != 0)
            continue;

        char path[HERMES_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", skin_dir, entry->d_name);

        tui_theme_t *th = (tui_theme_t *)calloc(1, sizeof(tui_theme_t));
        if (th && tui_load_skin_file(path, th)) {
            tui_themes[tui_theme_count++] = th;
        } else {
            free(th);
        }
    }
    closedir(d);
}

/* P198: reload theme by name */
void tui_fullscreen_theme_reload(const char *skin_name) {
    if (!skin_name) return;
    for (int i = 0; i < tui_theme_count; i++) {
        if (strcmp(tui_themes[i]->name, skin_name) == 0) {
            tui_current_theme = i;
            tui_apply_theme(tui_themes[i]);
            return;
        }
    }
}

/* Get current theme */
static const tui_theme_t *tui_get_theme(void) {
    if (tui_current_theme < 0 || tui_current_theme >= tui_theme_count)
        return &tui_theme_default;
    return tui_themes[tui_current_theme];
}

/* ==================================================================
 *  P200: MOBILE MODE — responsive layout, touch input, compact status
 * ================================================================== */
/* Port of Python: ui-tui (TypeScript) — responsive mobile layout */

typedef enum {
    TUI_LAYOUT_NORMAL,
    TUI_LAYOUT_MOBILE,
    TUI_LAYOUT_COMPACT,
} tui_layout_mode_t;

/* ==================================================================
 *  P189: LAYOUT — split pane management
 * ================================================================== */

/* Pane identifiers */
typedef enum {
    PANE_HISTORY = 0,       /* message history (top-left, large) */
    PANE_TOOL_FEED,         /* tool feed (top-right, medium) */
    PANE_INPUT,             /* input (bottom) */
    PANE_STATUS,            /* status bar (very bottom) */
    PANE_COUNT,
} pane_id_t;

/* Pane state */
typedef struct {
    WINDOW *win;
    int     rows, cols;
    int     y, x;
    bool    visible;
    bool    scrollable;
    int     scroll_pos;
    int     line_count;
} pane_t;

/* ==================================================================
 *  P190: INPUT — multi-line input buffer
 * ================================================================== */

#define INPUT_BUF_SIZE 65536
#define INPUT_HISTORY_MAX 512
#define AUTOCOMPLETE_MAX 32
#define EMOJI_MAX 50

/* Slash command definitions for autocomplete (slash_cmd_t hoisted above) */
static const tui_slash_cmd_t slash_commands[] = {
    {"/help",    "Show help message", ""},
    {"/quit",    "Exit the TUI", ""},
    {"/exit",    "Exit the TUI", ""},
    {"/clear",   "Clear output pane", ""},
    {"/theme",   "Set theme (default/dark/light/mono)", "<name>"},
    {"/model",   "Show/set current model", "[model_name]"},
    {"/tokens",  "Show token usage", ""},
    {"/budget",  "Show budget status", ""},
    {"/sessions","Open session browser", ""},
    {"/config",  "Open config editor", ""},
    {"/save",    "Save current session", "[name]"},
    {"/load",    "Load a session", "<session_id>"},
    {"/export",  "Export session as JSON/Markdown", "<format>"},
    {"/delete",  "Delete current session", ""},
    {"/verbose", "Set verbose level (0/1/2)", "<level>"},
    {"/yolo",    "Toggle yolo mode", ""},
    {"/fast",    "Toggle fast mode", ""},
    {"/mobile",  "Toggle mobile/compact layout", ""},
    {"/reset",   "Reset conversation", ""},
    {"/undo",    "Undo last turn", ""},
    {"/redraw",  "Force screen redraw", ""},
    {"/skin",    "List/reload skins", ""},
    {"/image",   "Display an image file", "<path>"},
    {"/gateway", "Show gateway status dashboard", ""},
    {"/cron",    "Show cron job viewer", ""},
    {"/logs",    "Show log viewer", ""},
    {"/skills",  "Browse available skills", ""},
    {"/todos",   "Show todo/kanban board", ""},
/* PoP: info @ hermes_cli/mcp_config.py:_info */
    {"/agent",   "Show agent info (model, provider, tokens)", ""},
    {NULL, NULL, NULL}
};

/* Input state — P190: multi-line editing */
typedef struct {
    char    buf[INPUT_BUF_SIZE];
    int     pos;                /* cursor position in buf */
    int     len;                /* current length */
    int     scroll_col;         /* horizontal scroll offset */
    int     cursor_row;         /* visual row within multi-line input */

    /* History */
    char   *history[INPUT_HISTORY_MAX];
    int     history_count;
    int     history_pos;        /* -1 = new input, 0..N-1 = history */

    /* Autocomplete — P190 */
    char    autocomplete_word[256];
    int     autocomplete_count;
    char    autocomplete_matches[AUTOCOMPLETE_MAX][256];
    int     autocomplete_sel;   /* selected autocomplete index */
    bool    autocomplete_active;

    /* Emoji — P190 */
    bool    emoji_picker_active;
    int     emoji_sel;
    char    emoji_results[EMOJI_MAX][16];
    int     emoji_count;

    /* State */
    bool    active;
    bool    echo;               /* echo typed characters */
    bool    multiline;          /* true if input spans multiple rows */
} input_state_t;

/* Common emoji list */
static const char *emoji_list[][2] = {
    {"\u2764", "heart"},
    {"\U0001F600", "grinning"},
    {"\U0001F603", "smile"},
    {"\U0001F60A", "blush"},
    {"\U0001F60E", "sunglasses"},
    {"\U0001F389", "party"},
    {"\U0001F44D", "thumbsup"},
    {"\U0001F44E", "thumbsdown"},
    {"\U0001F4AC", "speech"},
    {"\U0001F4A1", "bulb"},
    {"\u2705", "check"},
    {"\u274C", "cross"},
    {"\u2B50", "star"},
    {"\U0001F525", "fire"},
    {"\U0001F4A6", "sweat"},
    {"\U0001F4AA", "muscle"},
    {"\U0001F4AD", "thought"},
    {"\u26A1", "zap"},
    {"\u2192", "arrow"},
    {"\u2190", "back"},
    {NULL, NULL}
};

/* ==================================================================
 *  P191: MESSAGE DISPLAY — role-colored output with syntax highlighting
 * ================================================================== */
/* Port of Python: tui_gateway.render — role-colored message rendering */

#define MAX_LINE_LENGTH 4096

typedef enum {
    MSG_ROLE_USER,
    MSG_ROLE_ASSISTANT,
    MSG_ROLE_SYSTEM,
    MSG_ROLE_TOOL,
    MSG_ROLE_ERROR,
    MSG_ROLE_WARN,
    MSG_ROLE_INFO,
} msg_role_t;

typedef struct {
    msg_role_t role;
    char      text[MAX_LINE_LENGTH];
    int       color_pair;       /* ncurses color pair to use */
    bool      bold;
    bool      dim;
    bool      code_block;       /* in code fence */
    char      code_lang[32];    /* language hint */
} display_line_t;

/* Message history ring buffer */
typedef struct {
    display_line_t lines[MAX_MESSAGES_DISPLAY];
    int count;
    int head; /* ring buffer head */
} message_history_t;

/* ==================================================================
 *  P192: STREAMING — token stream display and counter
 * ================================================================== */

#define TYPE_AHEAD_BUF_SIZE 1024

typedef struct {
    bool    active;
    bool    abort_requested;    /* Ctrl+C during streaming */
    char    current_line[MAX_LINE_LENGTH];
    int     current_pos;
    int     token_count;
    double  start_time;
    double  last_token_time;
    double  tokens_per_sec;
    int     bytes_received;
    bool    first_token;
    /* Type-ahead buffer: captures keystrokes during streaming (T18) */
    char    type_ahead_buf[TYPE_AHEAD_BUF_SIZE];
    int     type_ahead_len;
} stream_state_t;

/* ==================================================================
 *  P193: TOOL FEED — live tool call display
 * ================================================================== */

#define MAX_TOOL_CALLS 32

typedef enum {
    TOOL_STATUS_PENDING,
    TOOL_STATUS_RUNNING,
    TOOL_STATUS_DONE,
    TOOL_STATUS_ERROR,
} tool_status_t;

typedef struct {
    char          name[64];
    char          args[256];
    tool_status_t status;
    int           progress;     /* 0-100 */
    int           total_steps;
    char          result_preview[256];
    char          shelf[48];   /* group heading (e.g. "File Ops", "Code", "Web") */
    bool          shelf_header;/* true if this is the first in its shelf */
} tool_call_entry_t;

typedef struct {
    tool_call_entry_t calls[MAX_TOOL_CALLS];
    int count;
    int active_count;
} tool_feed_state_t;

/* ==================================================================
 *  P194: STATUS BAR — model, tokens, iterations, budget
 * ================================================================== */

typedef struct {
    char    model[128];
    char    provider[64];
    int     iteration;
    int     max_iterations;
    int     tokens_in;
    int     tokens_out;
    double  budget_remaining;
    char    mode[32];           /* chat, streaming, idle */
    bool    dirty;              /* needs redraw */
} status_bar_state_t;

/* ==================================================================
 *  P195: SESSION BROWSER
 * ================================================================== */

#define SESSION_LIST_MAX 256
#define SESSION_SEARCH_MAX 128

typedef struct {
    char sessions[SESSION_LIST_MAX][64]; /* session IDs */
    session_meta_t meta[SESSION_LIST_MAX]; /* session metadata */
    int  count;
    int  selected;
    int  scroll_offset;
    char search[SESSION_SEARCH_MAX];
    bool active;
    bool search_mode;
} session_browser_state_t;

/* ==================================================================
 *  P196: CONFIG EDITOR
 * ================================================================== */

#define CONFIG_KEY_MAX 64
#define CONFIG_VALUE_MAX 256

typedef struct {
    char key[CONFIG_KEY_MAX];
    char value[CONFIG_VALUE_MAX];
    char description[256];
} config_entry_t;

#define CONFIG_SEARCH_MAX 128

typedef struct {
    config_entry_t entries[CONFIG_KEY_MAX];
    int count;
    int selected;
    int scroll_offset;
    bool active;
    bool edit_mode;         /* editing value */
    char edit_buf[256];
    int edit_pos;
    char search[CONFIG_SEARCH_MAX];
    bool search_mode;
} config_editor_state_t;

/* ==================================================================
 *  P197: IMAGE VIEWER — sixel/kitty inline display, zoom/pan
 * ================================================================== */
/* Port of Python: tui_gateway.server._image_meta — image sixel/kitty display */

typedef struct {
    bool    sixel_supported;
    bool    kitty_supported;
    bool    image_displayed;
    char    image_path[512];
    int     zoom_level;       /* 100 = normal */
    int     pan_x, pan_y;
} image_viewer_state_t;

/* ==================================================================
 *  T13: MODEL PICKER — interactive model selection overlay
 * ================================================================== */

/* MODEL_PICKER_MAX / MODEL_NAME_MAX / model_picker_state_t hoisted above. */

/* T15: Todo panel state */
#define TODO_PANEL_MAX 128
#define TODO_TITLE_MAX 96
#define TODO_ID_MAX 48
#define TODO_STATUS_MAX 16

typedef struct {
    char  id[TODO_PANEL_MAX][TODO_ID_MAX];
    char  title[TODO_PANEL_MAX][TODO_TITLE_MAX];
    char  status[TODO_PANEL_MAX][TODO_STATUS_MAX];
    char  assignee[TODO_PANEL_MAX][TODO_STATUS_MAX];
    int   count;
    int   selected;
    int   scroll_offset;
    int   filter_idx;
    bool  active;
} todo_panel_state_t;

/* ==================================================================
 *  P199: GATEWAY — JSON-RPC backend via FIFO
 * ================================================================== */

#define RPC_FIFO_PATH "/tmp/slermes-tui-rpc"
#define RPC_BUF_SIZE 65536
#define RPC_LINE_MAX 32768

typedef enum {
    RPC_IDLE,
    RPC_CONNECTED,
} rpc_state_t;

typedef struct {
    rpc_state_t state;
    int         fifo_fd;
    char        read_buf[RPC_BUF_SIZE];
    int         read_pos;
} gateway_state_t;

/* ==================================================================
 *  MAIN TUI STATE — all subsystems combined
 * ================================================================== */

typedef struct {
    /* Terminal */
    int rows, cols;
    bool running;
    bool initialized;
    tui_layout_mode_t layout_mode;

    /* Panes (P189) */
    pane_t panes[PANE_COUNT];

    /* Subsystems */
    input_state_t         input;          /* P190 */
    message_history_t     history;        /* P191 */
    stream_state_t        stream;         /* P192 */
    tool_feed_state_t     tool_feed;      /* P193 */
    status_bar_state_t    status;         /* P194 */
    session_browser_state_t sessions;     /* P195 */
    config_editor_state_t config_editor;  /* P196 */
    image_viewer_state_t  image_viewer;   /* P197 */
    gateway_state_t       gateway;        /* P199 */
    model_picker_state_t  model_picker;   /* T13: Model picker */
    todo_panel_state_t    todo_panel;     /* T15: Todo/kanban panel */

    /* FPS overlay — Ctrl+P toggle */
    int     frame_count;
    double  fps_start_time;
    bool    fps_visible;

    /* Plugin hub state */
    struct {
        int count;
        int selected;
        int scroll_offset;
        char names[256][64];
        char types[256][32];
        char versions[256][16];
        bool initialized[256];
        bool active;
    } plugin_hub;

    /* Saved stderr fd for TUI mode stderr redirection */
    int saved_stderr;

    /* Agent reference */
    agent_state_t        *agent;

    /* Modal overlay state */
    int     think_frame;        /* P192 animation frame counter */
    enum {
        MODE_NORMAL,
        MODE_SESSION_BROWSE,
        MODE_CONFIG_EDIT,
        MODE_IMAGE_VIEW,
        MODE_GATEWAY_STATUS,
        MODE_CRON_VIEW,
        MODE_LOG_VIEW,
        MODE_SKILL_BROWSE,
        MODE_HELP,
        MODE_MODEL_PICK,        /* T13: Model picker overlay */
        MODE_TODO_PANEL,        /* T15: Todo/kanban panel */
        MODE_AGENT_INFO,        /* T14: Agent info overlay */
        MODE_PLUGIN_HUB,        /* Plugin hub display */
    } modal_mode;
} tui_global_state_t;

/* The single TUI instance. Defined in tui_fullscreen.c; extern here so any
 * subsystem split can reach the one shared state object. */
extern tui_global_state_t tui;


/* Shared render-core interface (history ring + redraw). These are the two
 * entry points a subsystem overlay uses to append/render history; their
 * internals (markdown, roles, eventpub) stay private to the render core. */
void tui_history_add(msg_role_t role, const char *text, bool bold);
void tui_redraw_history(void);


/* todo/kanban overlay subsystem (tui_todo_panel.c) */
void tui_todo_panel_init(void);
void tui_draw_todo_panel(void);
int  tui_todo_panel_handle(int ch);


/* image-viewer overlay subsystem (tui_image_viewer.c) */
void tui_image_viewer_init(void);
bool tui_display_image(const char *path);
bool tui_display_image_sixel(const char *path);
bool tui_display_image_kitty(const char *path, int max_width, int max_height);
int  tui_image_viewer_handle(int ch);
void tui_draw_image_viewer(void);


/* model-picker overlay subsystem (tui_model_picker.c) */
void tui_model_picker_init(void);
void tui_draw_model_picker(void);
int  tui_model_picker_handle(int ch);

/* render-core entry point: status line */
void tui_redraw_status(void);

#endif /* TUI_STATE_H */
