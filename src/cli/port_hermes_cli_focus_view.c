/*
 * port_hermes_cli_focus_view.c — C11 port of hermes_cli/focus_view.py.
 *
 * Focus view — a display-only reduced-output mode.
 * Pure formatting and state logic. No I/O.
 *
 * This is a cohesive PoP port of ONE Python module (8 functions).
 * It is NOT a monolith — do not split speculatively.
 */

#define _POSIX_C_SOURCE 200809L

#include "port_hermes_cli_focus_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════ */

/* PoP: fv_tool_progress_visible_modes @ hermes_cli/focus_view.py:TOOL_PROGRESS_VISIBLE_MODES */
static const char *FV_VISIBLE_MODES[] = {"new", "all", "verbose"};
#define FV_VISIBLE_MODES_COUNT 3

/* PoP: fv_tool_progress_modes @ hermes_cli/focus_view.py:TOOL_PROGRESS_MODES */
static const char *FV_ALL_MODES[] = {"off", "new", "all", "verbose", "log"};
#define FV_ALL_MODES_COUNT 5

/* PoP: fv_on_words @ hermes_cli/focus_view.py:_ON_WORDS */
static const char *FV_ON_WORDS[] = {"on", "enable", "enabled", "true", "yes", "1"};
#define FV_ON_WORDS_COUNT 6

/* PoP: fv_off_words @ hermes_cli/focus_view.py:_OFF_WORDS */
static const char *FV_OFF_WORDS[] = {"off", "disable", "disabled", "false", "no", "0"};
#define FV_OFF_WORDS_COUNT 6

/* PoP: fv_status_words @ hermes_cli/focus_view.py:_STATUS_WORDS */
static const char *FV_STATUS_WORDS[] = {"status", "show", "?"};
#define FV_STATUS_WORDS_COUNT 3

/* PoP: fv_toggle_words @ hermes_cli/focus_view.py:_TOGGLE_WORDS */
static const char *FV_TOGGLE_WORDS[] = {"", "toggle"};
#define FV_TOGGLE_WORDS_COUNT 2

/* ── Helpers ───────────────────────────────────────────────────── */

static bool str_in_list(const char *s, const char * const *list, size_t count) {
    if (!s) return false;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(s, list[i]) == 0) return true;
    }
    return false;
}

static char *str_lower_strip(const char *s) {
    if (!s) return NULL;
    while (*s && (unsigned char)*s <= ' ') s++;
    if (!*s) {
        char *r = malloc(1); if (r) r[0] = '\0'; return r;
    }
    size_t len = strlen(s);
    while (len > 0 && (unsigned char)s[len - 1] <= ' ') len--;
    char *r = malloc(len + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < len; i++) r[i] = (char)tolower((unsigned char)s[i]);
    r[len] = '\0';
    return r;
}

/* ═══════════════════════════════════════════════════════════════════
 * Function implementations
 * ═══════════════════════════════════════════════════════════════════ */

/* PoP: fv_normalize_tool_progress_mode @ hermes_cli/focus_view.py:normalize_tool_progress_mode */
const char *fv_normalize_tool_progress_mode(const char *mode_str,
                                             bool mode_is_bool,
                                             bool mode_bool_val,
                                             const char *default_val)
{
    if (mode_is_bool) {
        return mode_bool_val ? "all" : "off";
    }
    if (!mode_str || !*mode_str) {
        return default_val ? default_val : "all";
    }
    char *norm = str_lower_strip(mode_str);
    if (!norm) return default_val ? default_val : "all";

    const char *result = NULL;
    if (strcmp(norm, "off") == 0) result = "off";
    else if (strcmp(norm, "new") == 0) result = "new";
    else if (strcmp(norm, "all") == 0) result = "all";
    else if (strcmp(norm, "verbose") == 0) result = "verbose";
    else if (strcmp(norm, "log") == 0) result = "log";
    else result = default_val ? default_val : "all";

    free(norm);
    return result;
}

/* PoP: fv_resolve_focus_arg @ hermes_cli/focus_view.py:resolve_focus_arg */
const char *fv_resolve_focus_arg(const char *arg, bool current,
                                  bool *out_target)
{
    if (!arg) arg = "";
    char *norm = str_lower_strip(arg);
    if (!norm) { if (out_target) *out_target = false; return "usage"; }

    const char *action = NULL;
    bool target = false;

    if (str_in_list(norm, FV_STATUS_WORDS, FV_STATUS_WORDS_COUNT)) {
        action = "status";
    } else if (str_in_list(norm, FV_ON_WORDS, FV_ON_WORDS_COUNT)) {
        action = "set";
        target = true;
    } else if (str_in_list(norm, FV_OFF_WORDS, FV_OFF_WORDS_COUNT)) {
        action = "set";
        target = false;
    } else if (str_in_list(norm, FV_TOGGLE_WORDS, FV_TOGGLE_WORDS_COUNT)) {
        action = "set";
        target = !current;
    } else {
        action = "usage";
    }

    free(norm);
    if (out_target) *out_target = target;
    return action;
}

/* PoP: fv_effective_tool_progress_mode @ hermes_cli/focus_view.py:effective_tool_progress_mode */
const char *fv_effective_tool_progress_mode(bool focus_enabled,
                                             const char *configured_mode_str,
                                             bool mode_is_bool,
                                             bool mode_bool_val)
{
    if (focus_enabled) {
        return FOCUS_TOOL_PROGRESS_MODE;
    }
    return fv_normalize_tool_progress_mode(
        configured_mode_str, mode_is_bool, mode_bool_val, "all");
}

/* PoP: fv_would_display_tool_line @ hermes_cli/focus_view.py:would_display_tool_line */
bool fv_would_display_tool_line(const char *mode_str,
                                 bool mode_is_bool,
                                 bool mode_bool_val,
                                 const char *function_name,
                                 const char *last_tool_name)
{
    if (!function_name || !*function_name)
        return false;

    const char *normalized = fv_normalize_tool_progress_mode(
        mode_str, mode_is_bool, mode_bool_val, "all");

    if (!str_in_list(normalized, FV_VISIBLE_MODES, FV_VISIBLE_MODES_COUNT))
        return false;

    if (strcmp(normalized, "new") == 0 && last_tool_name) {
        if (strcmp(function_name, last_tool_name) == 0)
            return false;
    }

    return true;
}

/* PoP: fv_format_hidden_line @ hermes_cli/focus_view.py:format_hidden_line */
char *fv_format_hidden_line(int count)
{
    if (count <= 0) return NULL;

    const char *noun = (count == 1) ? "tool line" : "tool lines";
    int needed = snprintf(NULL, 0, "\xe2\x8b\xaf %d %s hidden \xc2\xb7 /focus off to show",
                          count, noun);
    char *result = malloc((size_t)needed + 1);
    if (!result) return NULL;
    snprintf(result, (size_t)needed + 1, "\xe2\x8b\xaf %d %s hidden \xc2\xb7 /focus off to show",
             count, noun);
    return result;
}

/* PoP: fv_focus_statusbar_segment @ hermes_cli/focus_view.py:focus_statusbar_segment */
const char *fv_focus_statusbar_segment(bool enabled)
{
    return enabled ? FOCUS_STATUSBAR_LABEL : "";
}

/* PoP: fv_format_focus_status @ hermes_cli/focus_view.py:format_focus_status */
char *fv_format_focus_status(bool enabled, const char *configured_mode_str,
                              bool mode_is_bool, bool mode_bool_val)
{
    const char *state = enabled ? "ON" : "OFF";
    const char *restore = fv_normalize_tool_progress_mode(
        configured_mode_str, mode_is_bool, mode_bool_val, "all");
    char *restore_upper = strdup(restore);
    if (restore_upper) {
        for (char *p = restore_upper; *p; p++) *p = (char)toupper((unsigned char)*p);
    }

    char *result;
    if (enabled) {
        int needed = snprintf(NULL, 0,
            "Focus view: %s \xe2\x80\x94 only your prompt and the final response.\n"
            "  /focus off restores tool progress: %s",
            state, restore_upper ? restore_upper : restore);
        result = malloc((size_t)needed + 1);
        if (result) {
            snprintf(result, (size_t)needed + 1,
                "Focus view: %s \xe2\x80\x94 only your prompt and the final response.\n"
                "  /focus off restores tool progress: %s",
                state, restore_upper ? restore_upper : restore);
        }
    } else {
        const char *mode_upper = restore_upper ? restore_upper : restore;
        int needed = snprintf(NULL, 0,
            "Focus view: %s \xe2\x80\x94 tool progress: %s",
            state, mode_upper);
        result = malloc((size_t)needed + 1);
        if (result) {
            snprintf(result, (size_t)needed + 1,
                "Focus view: %s \xe2\x80\x94 tool progress: %s",
                state, mode_upper);
        }
    }

    free(restore_upper);
    return result;
}

/* PoP: fv_format_focus_toggle_message @ hermes_cli/focus_view.py:format_focus_toggle_message */
char *fv_format_focus_toggle_message(bool enabled,
                                      const char *configured_mode_str,
                                      bool mode_is_bool,
                                      bool mode_bool_val)
{
    if (enabled) {
        return strdup(
            "Focus view enabled \xe2\x80\x94 just your prompt and the final response");
    }

    const char *mode = fv_normalize_tool_progress_mode(
        configured_mode_str, mode_is_bool, mode_bool_val, "all");
    char *mode_upper = strdup(mode);
    if (mode_upper) {
        for (char *p = mode_upper; *p; p++) *p = (char)toupper((unsigned char)*p);
    }
    int needed = snprintf(NULL, 0,
        "Focus view disabled \xe2\x80\x94 tool progress: %s",
        mode_upper ? mode_upper : mode);
    char *result = malloc((size_t)needed + 1);
    if (result) {
        snprintf(result, (size_t)needed + 1,
            "Focus view disabled \xe2\x80\x94 tool progress: %s",
            mode_upper ? mode_upper : mode);
    }
    free(mode_upper);
    return result;
}