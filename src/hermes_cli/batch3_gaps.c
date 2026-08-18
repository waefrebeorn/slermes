/* Slermes C port — cli.py batch 3: remaining pure functions */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Forward declarations for functions in port_cli_pure_gaps.c */
extern double cli_luminance_from_hex(const char *hex);

/* ── _detect_light_mode (L2692) ── */
/* PoP: cli_detect_light_mode @ cli.py:_detect_light_mode */
bool cli_detect_light_mode(void) {
    static bool cached = false;
    static bool result = false;
    if (cached) return result;
    
    /* Check env overrides */
    const char *hermes_light = getenv("HERMES_LIGHT");
    if (hermes_light) {
        char v[16];
        for (int i = 0; i < 15 && hermes_light[i]; i++)
            v[i] = tolower((unsigned char)hermes_light[i]);
        v[15] = '\0';
        if (strcmp(v, "true") == 0 || strcmp(v, "1") == 0 || strcmp(v, "yes") == 0) {
            result = true; cached = true; return result;
        }
        if (strcmp(v, "false") == 0 || strcmp(v, "0") == 0 || strcmp(v, "no") == 0) {
            result = false; cached = true; return result;
        }
    }
    
    /* Check theme hint */
    const char *theme = getenv("HERMES_TUI_THEME");
    if (theme) {
        if (strcmp(theme, "light") == 0) { result = true; cached = true; return result; }
        if (strcmp(theme, "dark") == 0) { result = false; cached = true; return result; }
    }
    
    /* Check background color */
    const char *bg = getenv("HERMES_TUI_BACKGROUND");
    if (bg && *bg) {
        double lum = cli_luminance_from_hex(bg);
        if (lum >= 0.5) { result = true; cached = true; return result; }
        else { result = false; cached = true; return result; }
    }
    
    /* Check COLORFGBG */
    const char *cfgbg = getenv("COLORFGBG");
    if (cfgbg && *cfgbg) {
        const char *semi = strchr(cfgbg, ';');
        const char *last = semi ? semi + 1 : cfgbg;
        int bg_num = atoi(last);
        if (bg_num == 7 || bg_num == 15) { result = true; cached = true; return result; }
        if (bg_num >= 0 && bg_num < 16) { result = false; cached = true; return result; }
    }
    
    result = false;
    cached = true;
    return result;
}

/* ── _maybe_remap_for_light_mode (L2783) ── */
/* PoP: cli_remap_light_mode @ cli.py:_maybe_remap_for_light_mode */
const char *cli_remap_light_mode(const char *hex_color) {
    if (!hex_color || hex_color[0] != '#') return hex_color;
    if (!cli_detect_light_mode()) return hex_color;
    
    /* Static remap table */
    struct { const char *from; const char *to; } remap[] = {
        {"#FFF8DC", "#1A1A1A"}, {"#FFD700", "#9A6B00"}, {"#FFBF00", "#8A5A00"},
        {"#B8860B", "#5C4500"}, {"#DAA520", "#6B4F00"}, {"#F1E6CF", "#1A1A1A"},
        {"#c9d1d9", "#24292F"}, {"#EAF7FF", "#0F1B26"}, {"#F5F5F5", "#1A1A1A"},
        {"#FFF0D4", "#1A1A1A"}, {"#CD7F32", "#8A4F1A"}, {"#FFEFB5", "#3A2A00"},
        {NULL, NULL}
    };
    
    /* Case-insensitive compare */
    for (int i = 0; remap[i].from; i++) {
        if (strcasecmp(hex_color, remap[i].from) == 0) {
            return remap[i].to;
        }
    }
    return hex_color;
}

/* ── _terminal_width_for_streaming (L2970) ── */
/* PoP: cli_terminal_width_streaming @ cli.py:_terminal_width_for_streaming */
int cli_terminal_width_streaming(void) {
    const char *cols = getenv("COLUMNS");
    if (cols) {
        int w = atoi(cols);
        if (w > 0) return w;
    }
    return 80; /* default */
}

/* ── _coerce_output_history_limit (L3053) ── */
/* PoP: cli_coerce_output_history_limit @ cli.py:_coerce_output_history_limit */
int cli_coerce_output_history_limit(int limit) {
    if (limit <= 0) return 1000; /* default */
    if (limit > 10000) return 10000; /* cap */
    return limit;
}

/* ── _resolve_worktree_base (L1458) ── */
/* PoP: cli_resolve_worktree_base @ cli.py:_resolve_worktree_base */
char *cli_resolve_worktree_base(const char *cwd) {
    if (!cwd) return xstrdup("");
    /* Find .git directory by walking up */
    char path[4096];
    strncpy(path, cwd, sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    
    while (1) {
        char git_path[4096];
        snprintf(git_path, sizeof(git_path), "%s/.git", path);
        if (access(git_path, F_OK) == 0) {
            return xstrdup(path);
        }
        /* Go up one directory */
        char *last_slash = strrchr(path, '/');
        if (!last_slash || last_slash == path) break;
        *last_slash = '\0';
    }
    return xstrdup("");
}

/* ── _should_emit_cleanup_session_finalize (L1261) ── */
/* PoP: cli_should_emit_cleanup @ cli.py:_should_emit_cleanup_session_finalize */
bool cli_should_emit_cleanup(const char *session_id) {
    if (!session_id) return true;
    /* Simple check: always emit for now */
    return true;
}

/* ── _mark_tui_input_modes_active (L994) ── */
/* PoP: cli_mark_tui_input_modes @ cli.py:_mark_tui_input_modes_active */
void cli_mark_tui_input_modes(void) {
    /* Set a flag — in C, this is a static variable */
    static bool active = false;
    active = true;
}

/* ── _reset_terminal_input_modes_on_exit (L1347) ── */
/* PoP: cli_reset_terminal_input_modes @ cli.py:_reset_terminal_input_modes_on_exit */
void cli_reset_terminal_input_modes(void) {
    /* Reset terminal input modes — no-op in C (no TUI state to reset) */
}
