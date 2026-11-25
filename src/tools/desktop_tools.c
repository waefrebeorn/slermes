/*
 * desktop_tools.c - the desktop_ui tool family: set_theme, focus_pane, read_terminal.
 *
 * Ports of tools/set_theme_tool.py, tools/focus_pane_tool.py and tools/read_terminal_tool.py.
 *
 * These tools act on the desktop renderer, not on files: upstream each one validates its arguments,
 * then asks the renderer over the gateway bridge and reports what the *renderer* applied. A C
 * process that is not the desktop app has no renderer to ask, and upstream returns exactly that
 * failure ("only available in the Hermes desktop app") rather than pretending. That is what these
 * handlers do: the same validation (an unknown accent, mode, layout or pane is rejected here, with
 * the same lists) and the same honest error when nothing can answer. When the theme request *does*
 * carry an answer, the ported bridge logic in src/gateway/local_desktop_ui.c applies it.
 *
 * Registering them is what makes the contract visible to a model: an unregistered tool reads as
 * "unknown tool", which hides the real cause from the user.
 */
#include "hermes_json.h"
#include "hermes_core_types.h"
#include "registry.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* src/gateway/local_desktop_ui.c -- theme.apply / theme.pick over an injected answer. */
json_t *w10_localdeskt_b6c610_theme(const json_t *in);

#define ACCENT_SWATCHES  "mono", "green", "cyan", "nous", "violet", "pink", "red", "orange"
#define MODES            "light", "dark", "system"
#define PICKERS          "accent", "layout"
#define SIMPLE_LAYOUTS   "sidebar-left", "sidebar-right"
#define ADVANCED_LAYOUTS "default", "basic", "focus", "terminal-deck", "quad"
#define PANES            "chat", "files", "terminal", "review", "sessions"

static bool one_of(const char *v, const char *const *list, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(v, list[i]) == 0) return true;
    return false;
}

static char *json_text(json_t *o) {
    char *txt = json_serialize(o);
    json_free(o);
    return txt;
}

static char *err_json(const char *msg) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : "error"));
    return json_text(o);
}

/* "#rrggbb" only: six hex digits, nothing else. */
static bool is_hex_colour(const char *s) {
    if (!s || s[0] != '#' || strlen(s) != 7) return false;
    for (int i = 1; i < 7; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

/* PoP: set_theme_tool @ tools/set_theme_tool.py:set_theme_tool */
static char *handle_set_theme(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const swatches[] = { ACCENT_SWATCHES, "theme" };
    static const char *const modes[]    = { MODES };
    static const char *const pickers[]  = { PICKERS };
    static const char *const simple[]   = { SIMPLE_LAYOUTS };
    static const char *const advanced[] = { ADVANCED_LAYOUTS };

    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *accent = args ? json_get_str(args, "accent", "") : "";
    const char *mode   = args ? json_get_str(args, "mode", "") : "";
    const char *layout = args ? json_get_str(args, "layout", "") : "";
    const char *picker = args ? json_get_str(args, "picker", "") : "";

    /* json_len() counts array elements; on an object it is always 0, which is why the first version
     * rejected every call with "Pass at least one of...". Count the keys explicitly. */
    int n_values = 0;
    json_t *values = json_object();
    if (accent && *accent) { json_set(values, "accent", json_string(accent)); n_values++; }
    if (mode && *mode)     { json_set(values, "mode", json_string(mode));     n_values++; }
    if (layout && *layout) { json_set(values, "layout", json_string(layout)); n_values++; }

    if (picker && *picker) {
        if (!one_of(picker, pickers, sizeof pickers / sizeof *pickers)) {
            json_free(values); json_free(args);
            return err_json("picker must be one of: accent, layout.");
        }
        if (n_values > 0) {
            json_free(values); json_free(args);
            return err_json("picker shows a card for the user to choose; pass picker alone, without values.");
        }
        json_set(values, "picker", json_string(picker));
    } else if (n_values == 0) {
        json_free(values); json_free(args);
        return err_json("Pass at least one of accent, mode or layout, or picker to let the user choose.");
    }

    if (mode && *mode && !one_of(mode, modes, sizeof modes / sizeof *modes)) {
        json_free(values); json_free(args);
        return err_json("Unknown mode: use light, dark or system.");
    }
    if (accent && *accent && !is_hex_colour(accent) &&
        !one_of(accent, swatches, sizeof swatches / sizeof *swatches)) {
        json_free(values); json_free(args);
        return err_json("Unknown accent: use a swatch id, 'theme' or #rrggbb.");
    }
    if (layout && *layout &&
        !one_of(layout, simple, sizeof simple / sizeof *simple) &&
        !one_of(layout, advanced, sizeof advanced / sizeof *advanced)) {
        json_free(values); json_free(args);
        return err_json("Unknown layout: use sidebar-left, sidebar-right, default, basic, focus, terminal-deck or quad.");
    }

    /* Ask the bridge: with no desktop answer injected it reports that no window replied, which is
     * the desktop-absent case. */
    json_t *req = json_object();
    json_set(req, "payload", values);        /* takes ownership of values */
    json_t *res = w10_localdeskt_b6c610_theme(req);
    json_free(req);
    json_free(args);
    return res ? json_text(res) : err_json("No Hermes Desktop window answered the theme request.");
}

/* PoP: focus_pane_tool @ tools/focus_pane_tool.py:focus_pane_tool */
static char *handle_focus_pane(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const panes[] = { PANES };
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *pane = args ? json_get_str(args, "pane", "") : "";
    if (!pane || !*pane || !one_of(pane, panes, sizeof panes / sizeof *panes)) {
        json_free(args);
        return err_json("pane must be one of: chat, files, terminal, review, sessions.");
    }
    json_free(args);
    return err_json("Pane focus is only available in the Hermes desktop app.");
}

/* PoP: read_terminal_tool @ tools/read_terminal_tool.py:read_terminal_tool */
static char *handle_read_terminal(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const json_t *start = args ? json_obj_get(args, "start_line") : NULL;
    const json_t *count = args ? json_obj_get(args, "count") : NULL;
    bool bad_type = false;
    if (start && start->type != JSON_NUMBER) bad_type = true;
    if (count && count->type != JSON_NUMBER) bad_type = true;
    json_free(args);
    if (bad_type) return err_json("start_line and count must be integers.");
    return err_json("read_terminal is only available in the Hermes desktop app.");
}

void registry_init_desktop_ui(void) {
    registry_register("set_theme",
        "Set the Hermes desktop app's accent colour, light/dark mode and layout, or show a picker card. "
        "The renderer applies the values and answers with what it applied.",
        "{\"type\":\"object\",\"properties\":{"
          "\"accent\":{\"type\":\"string\",\"description\":\"Swatch id (mono, green, cyan, nous, violet, pink, red, orange), 'theme', or #rrggbb.\"},"
          "\"mode\":{\"type\":\"string\",\"enum\":[\"light\",\"dark\",\"system\"]},"
          "\"layout\":{\"type\":\"string\",\"enum\":[\"sidebar-left\",\"sidebar-right\",\"default\",\"basic\",\"focus\",\"terminal-deck\",\"quad\"]},"
          "\"picker\":{\"type\":\"string\",\"enum\":[\"accent\",\"layout\"],\"description\":\"Show a picker card instead of applying values.\"}},"
          "\"required\":[]}",
        handle_set_theme);
    registry_register("focus_pane",
        "Show a desktop pane: chat, files, terminal, review (git diff) or sessions. Use when the user asks to see one.",
        "{\"type\":\"object\",\"properties\":{"
          "\"pane\":{\"type\":\"string\",\"enum\":[\"chat\",\"files\",\"terminal\",\"review\",\"sessions\"],\"description\":\"Which pane to reveal.\"}},"
          "\"required\":[\"pane\"]}",
        handle_focus_pane);
    registry_register("read_terminal",
        "Read the in-app terminal pane beside this chat. No args = visible screen + total_lines; page scrollback with start_line (0 = oldest) + count.",
        "{\"type\":\"object\",\"properties\":{"
          "\"start_line\":{\"type\":\"integer\",\"description\":\"First line to return (0 = oldest).\"},"
          "\"count\":{\"type\":\"integer\",\"description\":\"How many lines to return (minimum 1).\"}},"
          "\"required\":[]}",
        handle_read_terminal);
}
