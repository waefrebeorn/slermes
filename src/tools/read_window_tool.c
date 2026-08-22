/*
 * read_window_tool.c — C11 port of tools/read_window_tool.py
 *
 * Read which OS window sits directly underneath the Hermes desktop window.
 * The window list lives with the OS, so this tool round-trips through the
 * gateway's blocking-prompt bridge via a platform-injected callback — this
 * module is just schema + a thin dispatcher over that callback.
 */

#define _POSIX_C_SOURCE 200809L

#include "read_window_tool.h"
#include "registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: from tools.registry import registry, tool_error */
char *read_window_tool_error(const char *message) {
    json_t *o = json_new_object();
    if (!o) return strdup("{\"error\":\"\"}");
    json_object_set(o, "error", json_new_string(message ? message : ""));
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* PoP: read_window_below_tool @ tools/read_window_tool.py:read_window_below_tool */
char *read_window_below_tool(read_window_below_cb callback, void *ctx) {
    if (!callback) {
        return read_window_tool_error(
            "read_window_below is only available in the Hermes desktop app.");
    }

    char *raw = NULL;
    char *err = NULL;
    int rc = callback(ctx, &raw, &err);
    if (rc != 0) {
        /* except Exception as exc: tool_error(f"Failed to read the window
         * below: {exc}") */
        size_t need = snprintf(NULL, 0, "Failed to read the window below: %s",
                               err ? err : "");
        char *msg = malloc(need + 1);
        if (msg) snprintf(msg, need + 1, "Failed to read the window below: %s",
                          err ? err : "");
        free(err);
        char *out = read_window_tool_error(msg ? msg : "");
        free(msg);
        return out;
    }
    free(err);

    /* if not raw: the desktop app did not answer */
    if (!raw || !*raw) {
        free(raw);
        return read_window_tool_error(
            "Could not determine the window underneath (the desktop app did "
            "not answer, or window enumeration is unavailable on this system).");
    }

    /* Desktop answers with a JSON object; pass it through, else wrap the raw
     * text: try json.dumps(json.loads(raw)) except -> {"text": str(raw)}. */
    json_t *parsed = json_parse(raw, NULL);
    if (parsed) {
        char *out = json_serialize(parsed);
        json_free(parsed);
        free(raw);
        if (out) return out;
        return read_window_tool_error("");
    }
    json_t *wrap = json_new_object();
    json_object_set(wrap, "text", json_new_string(raw));
    char *out = wrap ? json_serialize(wrap) : NULL;
    json_free(wrap);
    free(raw);
    if (out) return out;
    return read_window_tool_error("");
}

/* PoP: READ_WINDOW_BELOW_SCHEMA @ tools/read_window_tool.py */
char *read_window_below_schema(void) {
    static const char *SCHEMA =
        "{"
        "\"name\": \"read_window_below\", "
        "\"description\": "
        "\"Identify the application window directly underneath (behind) the "
        "Hermes desktop window — what the user is working in behind this app. "
        "Returns JSON: {window: {app, title, bounds{x,y,width,height}, id}, "
        "frontmost: {app, title}, platform}. `title` may be empty when the OS "
        "withholds window titles (e.g. macOS without the Screen Recording "
        "permission — never prompted for, noted in `note`). Other Hermes "
        "windows are skipped: the nearest non-Hermes window is reported. "
        "Returns {error, platform} instead where the OS cannot enumerate "
        "windows at all (e.g. a Wayland session); `error` says what would fix "
        "it, so relay it rather than retrying. "
        "Metadata only; this never captures pixels or content of other windows.\", "
        "\"parameters\": {"
        "\"type\": \"object\", "
        "\"properties\": {}"
        "}"
        "}";
    return strdup(SCHEMA);
}

/* Platform callback injection seam (see header). */
static read_window_below_cb g_callback;
static void *g_callback_ctx;

void read_window_tool_set_callback(read_window_below_cb callback, void *ctx) {
    g_callback = callback;
    g_callback_ctx = ctx;
}

/* handler=lambda args, **kw: read_window_below_tool(callback=kw.get("callback")) */
static char *read_window_below_handler(const char *args_json, const char *task_id) {
    (void)args_json; (void)task_id;
    return read_window_below_tool(g_callback, g_callback_ctx);
}

/* PoP: module-level registry.register @ tools/read_window_tool.py.
 * Note: the Python registration passes emoji="🪟"; the C registry port has
 * no emoji setter, so the emoji is not carried (display-only metadata). */
bool read_window_tool_register(void) {
    char *schema = read_window_below_schema();
    if (!schema) return false;
    /* Python registry.register defaults description to schema["description"]. */
    char *desc = NULL;
    json_t *sn = json_parse(schema, NULL);
    if (sn) {
        json_t *d = json_object_get(sn, "description");
        if (d && d->type == JSON_STRING) desc = strdup(d->str_val);
        json_free(sn);
    }
    bool ok = registry_register_ex(
        "read_window_below",
        desc ? desc : "",
        schema,
        "desktop_ui",
        read_window_below_handler);
    free(schema);
    free(desc);
    return ok;
}
