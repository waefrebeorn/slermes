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

#include <sqlite3.h>
#include <limits.h>

/* src/persistence/sqlite/_state/hermes_state_msgs.c -- the session store's message row helpers. */
void hermes_state_set_message_reaction(sqlite3 *conn, const char *session_id,
                                       long long row_id, const char *reaction);
char *hermes_state_get_message_role(sqlite3 *conn, const char *session_id, long long row_id);

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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


/* ---------------------------------------------------------------- */
/* show_tip, gui_tour, read_window_below, desktop_preview            */
/* ---------------------------------------------------------------- */

static char *ok_json(const char *key, const char *value) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(true));
    if (key) json_set(o, key, json_string(value ? value : ""));
    return json_text(o);
}

/* PoP: tip_tool @ tools/tip_tool.py:tip_tool */
static char *handle_show_tip(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const sides[] = { "top", "right", "bottom", "left" };
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *text = args ? json_get_str(args, "text", "") : "";
    const char *selector = args ? json_get_str(args, "selector", "") : "";
    const char *side = args ? json_get_str(args, "side", "") : "";

    if (!text || !*text) { json_free(args); return err_json("tip needs text - the one line the bubble says."); }
    if (!selector || !*selector) {
        json_free(args);
        return err_json("tip needs a selector to point at. Call tour(action='targets') to see what's on screen.");
    }
    if (side && *side && !one_of(side, sides, sizeof sides / sizeof *sides)) {
        json_free(args); return err_json("side must be one of: top, right, bottom, left.");
    }
    json_free(args);
    return err_json("tip is only available in the Hermes desktop app.");
}

/* PoP: tour_tool @ tools/tour_tool.py:tour_tool */
static char *handle_gui_tour(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const actions[]  = { "targets", "show", "start", "next", "prev", "stop" };
    static const char *const surfaces[] = { "app", "preview" };
    static const char *const sides[]    = { "top", "right", "bottom", "left" };

    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *action  = args ? json_get_str(args, "action", "") : "";
    const char *surface = args ? json_get_str(args, "surface", "") : "";
    const char *side    = args ? json_get_str(args, "side", "") : "";
    const json_t *steps = args ? json_obj_get(args, "steps") : NULL;
    bool has_point = false;

    if (!action || !one_of(action, actions, sizeof actions / sizeof *actions)) {
        json_free(args);
        return err_json("action must be one of: targets, show, start, next, prev, stop.");
    }
    if (surface && *surface && !one_of(surface, surfaces, sizeof surfaces / sizeof *surfaces)) {
        json_free(args); return err_json("surface must be one of: app, preview.");
    }
    if (side && *side && !one_of(side, sides, sizeof sides / sizeof *sides)) {
        json_free(args); return err_json("side must be one of: top, right, bottom, left.");
    }
    if (args) {
        const char *sel = json_get_str(args, "selector", "");
        const char *tit = json_get_str(args, "title", "");
        const char *txt = json_get_str(args, "text", "");
        has_point = (sel && *sel) || (tit && *tit) || (txt && *txt);
    }
    if (strcmp(action, "show") == 0 && !has_point) {
        json_free(args);
        return err_json("show needs a selector (and/or title/text for the popover).");
    }
    if (strcmp(action, "start") == 0 && steps == NULL &&
        surface && *surface && strcmp(surface, "app") != 0) {
        json_free(args);
        return err_json("The built-in tour runs on surface='app'; pass steps for a preview tour.");
    }
    if (strcmp(action, "start") == 0 && steps != NULL) {
        if (steps->type != JSON_ARRAY || json_len((json_t *)steps) == 0) {
            json_free(args);
            return err_json("start needs a non-empty steps array, or no steps for the built-in tour.");
        }
    }
    json_free(args);
    if (strcmp(action, "targets") == 0)
        return err_json("tour is only available in the Hermes desktop app.");
    return err_json("tour is only available in the Hermes desktop app.");
}

/* PoP: read_window_below_tool @ tools/read_window_tool.py:read_window_below_tool */
static char *handle_read_window_below(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    return err_json("read_window_below is only available in the Hermes desktop app.");
}

/* Bare domains become https:// URLs; an existing path is passed through as a file target. */
static void normalise_target(const char *in, char *out, size_t sz) {
    out[0] = '\0';
    if (!in) return;
    while (*in == ' ' || *in == '\t') in++;
    size_t n = strlen(in);
    while (n && (in[n - 1] == ' ' || in[n - 1] == '\t' || in[n - 1] == '\r' || in[n - 1] == '\n')) n--;
    if (!n || n >= sz) return;

    char tmp[1024];
    snprintf(tmp, sizeof tmp, "%.*s", (int)n, in);
    if (strstr(tmp, "://") || tmp[0] == '/' || tmp[0] == '.' || tmp[0] == '~' ||
        strchr(tmp, '\\') || strchr(tmp, ':') || strchr(tmp, '/')) {
        snprintf(out, sz, "%s", tmp);
    } else {
        snprintf(out, sz, "https://%s", tmp);
    }
}

/* PoP: _handle_preview @ tools/preview_tool.py:_handle_preview */
static char *handle_desktop_preview(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *action = args ? json_get_str(args, "action", "") : "";
    const char *url = args ? json_get_str(args, "url", "") : "";

    if (!action || (strcmp(action, "open") && strcmp(action, "close") && strcmp(action, "read"))) {
        json_free(args);
        return err_json("action must be one of: open, close, read.");
    }
    if (strcmp(action, "read") == 0) {
        json_free(args);
        return err_json("preview read must run inside a desktop session (no GUI callback here).");
    }
    if (strcmp(action, "open") == 0) {
        char target[1024];
        normalise_target(url, target, sizeof target);
        if (!target[0]) {
            json_free(args);
            return err_json("url is required - a web URL (https://...), a localhost dev server, or a file path to show in the preview pane.");
        }
        struct stat st;
        if (stat(target, &st) == 0 && (st.st_mode & S_IFDIR)) {
            json_free(args);
            return err_json("directories are not previewable - pass a file path or a URL.");
        }
    }
    json_free(args);
    return err_json("desktop_preview is only available in the Hermes desktop app.");
}


/* PoP: annotate_preview_tool @ tools/annotate_preview_tool.py:annotate_preview_tool */
static char *handle_annotate_preview(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const actions[] = { "add", "hold", "remove", "clear" };
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *action = args ? json_get_str(args, "action", "add") : "add";
    const char *ref = args ? json_get_str(args, "ref", "") : "";
    const char *selector = args ? json_get_str(args, "selector", "") : "";

    if (!action || !*action) action = "add";
    if (!one_of(action, actions, sizeof actions / sizeof *actions)) {
        json_free(args);
        return err_json("action must be one of: add, hold, remove, clear.");
    }
    /* add/hold/remove need something to point at; clear does not. */
    if (strcmp(action, "clear") != 0 && (!ref || !*ref) && (!selector || !*selector)) {
        json_free(args);
        return err_json("add needs a ref from drive_preview action='elements' (e.g. 'btn-sign-in') or a CSS selector.");
    }
    json_free(args);
    return err_json("annotate_preview is only available in the Hermes desktop app.");
}

/* PoP: drive_preview_tool @ tools/drive_preview_tool.py:drive_preview_tool */
static char *handle_drive_preview(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const actions[] = { "elements", "click", "hover", "type", "scroll", "press",
                                           "strobe", "back", "forward", "reload" };
    static const char *const needs_target[] = { "click", "hover", "type", "press" };
    static const char *const scroll_to[] = { "top", "bottom" };

    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *action = args ? json_get_str(args, "action", "") : "";
    const char *ref = args ? json_get_str(args, "ref", "") : "";
    const char *selector = args ? json_get_str(args, "selector", "") : "";
    const char *key = args ? json_get_str(args, "key", "") : "";
    const char *to = args ? json_get_str(args, "to", "") : "";
    const json_t *text = args ? json_obj_get(args, "text") : NULL;
    const json_t *amount = args ? json_obj_get(args, "amount") : NULL;
    const json_t *limit = args ? json_obj_get(args, "limit") : NULL;
    char msg[160];

    if (!action || !one_of(action, actions, sizeof actions / sizeof *actions)) {
        json_free(args);
        return err_json("action must be one of: elements, click, hover, type, scroll, press, strobe, back, forward, reload.");
    }
    if (one_of(action, needs_target, sizeof needs_target / sizeof *needs_target) &&
        (!ref || !*ref) && (!selector || !*selector)) {
        snprintf(msg, sizeof msg, "%s needs a ref from action='elements' (e.g. 'btn-sign-in') or a CSS selector.", action);
        json_free(args);
        return err_json(msg);
    }
    if (strcmp(action, "type") == 0 && !text) {
        json_free(args); return err_json("type needs the text to enter.");
    }
    if (strcmp(action, "press") == 0 && (!key || !*key)) {
        json_free(args); return err_json("press needs a key, e.g. 'Enter' or 'Escape'.");
    }
    if (to && *to && !one_of(to, scroll_to, sizeof scroll_to / sizeof *scroll_to)) {
        json_free(args); return err_json("to must be one of: top, bottom.");
    }
    if ((amount && amount->type != JSON_NUMBER) || (limit && limit->type != JSON_NUMBER)) {
        json_free(args); return err_json("amount and max must be integers.");
    }
    json_free(args);
    return err_json("drive_preview is only available in the Hermes desktop app.");
}


/* PoP: react_to_message_tool @ tools/react_to_message_tool.py:react_to_message_tool
 *
 * The reaction is persisted in the session store (messages.reaction, author "agent"); the live
 * repaint through the desktop bridge is best effort upstream and is absent here, which is not an
 * error -- the row is what the user sees on reload. */
static char *handle_react_to_message(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *emoji = args ? json_get_str(args, "emoji", "") : "";
    const json_t *row_arg = args ? json_obj_get(args, "message_row_id") : NULL;
    const json_t *back_arg = args ? json_obj_get(args, "messages_back") : NULL;

    const char *session_key = getenv("HERMES_SESSION_ID");
    if (!session_key || !*session_key) session_key = getenv("HERMES_SESSION_KEY");
    if (!session_key || !*session_key) {
        json_free(args);
        return err_json("No session is attached to this turn, so there is no message to react to.");
    }

    char home[PATH_MAX], dbpath[PATH_MAX + 32];
    hermes_get_home(home, sizeof home);
    snprintf(dbpath, sizeof dbpath, "%s/state.db", home);

    sqlite3 *conn = NULL;
    if (sqlite3_open_v2(dbpath, &conn, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK || !conn) {
        if (conn) sqlite3_close(conn);
        json_free(args);
        return err_json("The session store is not available, so there is no message to react to.");
    }

    long long row_id = -1;
    if (row_arg && row_arg->type == JSON_NUMBER) {
        row_id = (long long)row_arg->num_val;
    } else {
        long long back = 0;
        if (back_arg && back_arg->type == JSON_NUMBER) back = (long long)back_arg->num_val;
        if (back < 0) back = 0;
        sqlite3_stmt *st = NULL;
        if (sqlite3_prepare_v2(conn,
                "SELECT id FROM messages WHERE session_id = ? AND role = 'user' "
                "ORDER BY id DESC LIMIT 1 OFFSET ?", -1, &st, NULL) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, session_key, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(st, 2, back);
            if (sqlite3_step(st) == SQLITE_ROW) row_id = sqlite3_column_int64(st, 0);
            sqlite3_finalize(st);
        }
        if (row_id < 0) {
            char msg[128];
            if (back) snprintf(msg, sizeof msg, "No user message found %lld back.", back);
            else      snprintf(msg, sizeof msg, "No user message to react to yet.");
            sqlite3_close(conn);
            json_free(args);
            return err_json(msg);
        }
    }

    /* A row outside this session must not be reacted to. */
    char *role = hermes_state_get_message_role(conn, session_key, row_id);
    if (!role) {
        sqlite3_close(conn);
        json_free(args);
        char msg[96];
        snprintf(msg, sizeof msg, "Message %lld is not part of this conversation.", row_id);
        return err_json(msg);
    }
    free(role);

    /* An empty emoji retracts the reaction. */
    hermes_state_set_message_reaction(conn, session_key, row_id, (emoji && *emoji) ? emoji : NULL);
    sqlite3_close(conn);

    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "row_id", json_number((double)row_id));
    json_t *reactions = json_array();
    if (emoji && *emoji) {
        json_t *r = json_object();
        json_set(r, "emoji", json_string(emoji));
        json_set(r, "author", json_string("agent"));
        json_append(reactions, r);
    }
    json_set(out, "reactions", reactions);
    json_free(args);
    return json_text(out);
}


/* PoP: manage_catalog @ tools/connectors/catalog_tool.py:manage_catalog
 *
 * Order matters upstream: argument validation runs first, and only then the desktop-only guard, so
 * a caller with a bad action hears about the action rather than about the missing window. */
static char *handle_manage_catalog(const char *args_json, const char *task_id) {
    (void)task_id;
    static const char *const actions[] = { "search", "install" };
    static const char *const kinds[]   = { "plugin", "skill" };
    static const char *const top_keys[] = { "action", "kind", "query", "items", "reason" };

    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *action = args ? json_get_str(args, "action", "") : "";
    const char *kind = args ? json_get_str(args, "kind", "") : "";
    const json_t *items = args ? json_obj_get(args, "items") : NULL;

    if (args && args->type == JSON_OBJECT) {
        /* unknown parameter(s) ... */
        for (size_t i = 0; i < json_object_size(args); i++) {
            const char *k = json_object_get_key_at(args, i);
            if (!k) continue;
            bool known = false;
            for (size_t j = 0; j < sizeof top_keys / sizeof *top_keys; j++)
                if (strcmp(k, top_keys[j]) == 0) { known = true; break; }
            if (!known) {
                char msg[256];
                snprintf(msg, sizeof msg,
                         "unknown parameter '%s': manage_catalog takes action, kind, query, items and reason. "
                         "The host decides source, version, profile and settings.", k);
                json_free(args);
                return err_json(msg);
            }
        }
    }
    if (!action || !*action || !one_of(action, actions, sizeof actions / sizeof *actions)) {
        json_free(args);
        return err_json("action must be one of search, install.");
    }
    if (kind && *kind && !one_of(kind, kinds, sizeof kinds / sizeof *kinds)) {
        json_free(args);
        return err_json("kind must be one of plugin, skill.");
    }
    if (strcmp(action, "install") == 0) {
        if (!items || items->type != JSON_ARRAY || json_len((json_t *)items) == 0) {
            json_free(args);
            return err_json("install needs 'items': [{\"kind\": \"plugin\", \"id\": \"<catalog id>\"}, ...].");
        }
    }
    json_free(args);
    return err_json("manage_catalog is available only in the Hermes desktop app, where the approval card "
                    "can be drawn. Tell the user to install from a terminal instead: `hermes plugins "
                    "install <id>` for a plugin, `hermes skills install <id>` for a skill.");
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
    registry_register("show_tip",
        "Point at something in the Hermes desktop GUI and say one line about it. Fire-and-forget: a tip is not a question.",
        "{\"type\":\"object\",\"properties\":{"
          "\"text\":{\"type\":\"string\",\"description\":\"The one line the bubble says.\"},"
          "\"selector\":{\"type\":\"string\",\"description\":\"data-tour handle or CSS selector to point at.\"},"
          "\"title\":{\"type\":\"string\",\"description\":\"Optional bubble title.\"},"
          "\"side\":{\"type\":\"string\",\"enum\":[\"top\",\"right\",\"bottom\",\"left\"]}},"
          "\"required\":[\"text\",\"selector\"]}",
        handle_show_tip);
    registry_register("gui_tour",
        "Run or query a guided tour of the desktop GUI: list targets, show one step, start/next/prev/stop a tour.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"targets\",\"show\",\"start\",\"next\",\"prev\",\"stop\"]},"
          "\"surface\":{\"type\":\"string\",\"enum\":[\"app\",\"preview\"]},"
          "\"selector\":{\"type\":\"string\"},"
          "\"title\":{\"type\":\"string\"},"
          "\"text\":{\"type\":\"string\"},"
          "\"side\":{\"type\":\"string\",\"enum\":[\"top\",\"right\",\"bottom\",\"left\"]},"
          "\"steps\":{\"type\":\"array\",\"items\":{\"type\":\"object\"}},"
          "\"preset\":{\"type\":\"string\"}},"
          "\"required\":[\"action\"]}",
        handle_gui_tour);
    registry_register("read_window_below",
        "Identify the app window behind the Hermes desktop window (what the user is in). Metadata only; never captures pixels.",
        "{\"type\":\"object\",\"properties\":{},\"required\":[]}",
        handle_read_window_below);
    registry_register("desktop_preview",
        "Open, close, or read the preview pane beside the chat: a web URL, a localhost dev server, or a file path (HTML renders live).",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"open\",\"close\",\"read\"]},"
          "\"url\":{\"type\":\"string\",\"description\":\"Web URL (bare domains fine), localhost dev server, or file path.\"},"
          "\"label\":{\"type\":\"string\",\"description\":\"Optional tab label.\"}},"
          "\"required\":[\"action\"]}",
        handle_desktop_preview);
    registry_register("annotate_preview",
        "Put one annotation up in the in-app browser preview, take one down, or clear them all. add/hold/remove need a ref or selector; clear takes them all down.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"add\",\"hold\",\"remove\",\"clear\"]},"
          "\"ref\":{\"type\":\"string\",\"description\":\"Ref from drive_preview action='elements'.\"},"
          "\"selector\":{\"type\":\"string\",\"description\":\"CSS selector to annotate.\"},"
          "\"label\":{\"type\":\"string\",\"description\":\"Optional label on the annotation.\"}},"
          "\"required\":[]}",
        handle_annotate_preview);
    registry_register("drive_preview",
        "Drive the in-app browser preview: inventory elements, click, hover, type, scroll, press a key, or navigate back/forward/reload.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"elements\",\"click\",\"hover\",\"type\",\"scroll\",\"press\",\"strobe\",\"back\",\"forward\",\"reload\"]},"
          "\"ref\":{\"type\":\"string\"},"
          "\"selector\":{\"type\":\"string\"},"
          "\"text\":{\"type\":\"string\",\"description\":\"Text to enter for action='type'.\"},"
          "\"key\":{\"type\":\"string\",\"description\":\"Key for action='press', e.g. Enter.\"},"
          "\"to\":{\"type\":\"string\",\"enum\":[\"top\",\"bottom\"]},"
          "\"amount\":{\"type\":\"integer\"},"
          "\"limit\":{\"type\":\"integer\"}},"
          "\"required\":[\"action\"]}",
        handle_drive_preview);
    registry_register("react_to_message",
        "React to a message in the conversation with an emoji (an empty emoji retracts it). Defaults to the latest user message.",
        "{\"type\":\"object\",\"properties\":{"
          "\"emoji\":{\"type\":\"string\",\"description\":\"Emoji to react with; empty string retracts.\"},"
          "\"message_row_id\":{\"type\":\"integer\",\"description\":\"Message row id; defaults to the latest user message.\"},"
          "\"messages_back\":{\"type\":\"integer\",\"description\":\"How many user messages back to react to (0 = the latest).\"}},"
          "\"required\":[\"emoji\"]}",
        handle_react_to_message);
    registry_register("manage_catalog",
        "Find and install Hermes catalog plugins and hub skills for the user. 'search' lists matches and changes nothing; 'install' shows the user an approval card.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"search\",\"install\"]},"
          "\"kind\":{\"type\":\"string\",\"enum\":[\"plugin\",\"skill\"]},"
          "\"query\":{\"type\":\"string\",\"description\":\"search: text to match.\"},"
          "\"items\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"properties\":{\"kind\":{\"type\":\"string\"},\"id\":{\"type\":\"string\"}}}},"
          "\"reason\":{\"type\":\"string\"}},"
          "\"required\":[\"action\"]}",
        handle_manage_catalog);
    registry_register("read_terminal",
        "Read the in-app terminal pane beside this chat. No args = visible screen + total_lines; page scrollback with start_line (0 = oldest) + count.",
        "{\"type\":\"object\",\"properties\":{"
          "\"start_line\":{\"type\":\"integer\",\"description\":\"First line to return (0 = oldest).\"},"
          "\"count\":{\"type\":\"integer\",\"description\":\"How many lines to return (minimum 1).\"}},"
          "\"required\":[]}",
        handle_read_terminal);
}
