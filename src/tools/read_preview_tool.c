/*
 * read_preview_tool.c — C11 port of tools/read_preview_tool.py
 *
 * Read the in-app browser / preview pane in the Hermes desktop GUI. The
 * preview's content lives in the desktop renderer, so this tool round-trips
 * through the gateway's blocking-prompt bridge via a platform-injected
 * callback — this module is just schema + a thin dispatcher over it.
 */

#define _POSIX_C_SOURCE 200809L

#include "read_preview_tool.h"
#include "registry.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: from tools.registry import registry, tool_error */
static char *preview_tool_error(const char *message) {
    json_t *o = json_new_object();
    if (!o) return strdup("{\"error\":\"\"}");
    json_object_set(o, "error", json_new_string(message ? message : ""));
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

/* Mirror Python int(val) for the types the gateway can hand us:
 *   number  -> truncate toward zero          (int(5.9) == 5, int(-5.9) == -5)
 *   bool    -> 1/0                           (bool is an int subclass)
 *   string  -> strict base-10 parse of the whole stripped string
 *   others  -> TypeError/ValueError          -> -1
 * Returns 0 on success and stores the value in *out. */
static int py_int_val(const json_t *node, long *out) {
    if (!node || !out) return -1;
    switch (node->type) {
    case JSON_NUMBER:
        *out = (long)node->num_val;  /* C truncation == Python int() */
        return 0;
    case JSON_BOOL:
        *out = node->bool_val ? 1 : 0;
        return 0;
    case JSON_STRING: {
        if (!node->str_val) return -1;
        const char *s = node->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (!*s) return -1;
        errno = 0;
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (errno != 0 || end == s) return -1;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')
            end++;
        if (*end != '\0') return -1;
        *out = v;
        return 0;
    }
    default:
        return -1;
    }
}

/* PoP: read_preview_tool @ tools/read_preview_tool.py:read_preview_tool */
char *read_preview_tool(const json_t *start_node, const json_t *count_node,
                        read_preview_cb callback, void *ctx) {
    if (!callback) {
        return preview_tool_error(
            "read_preview is only available in the Hermes desktop app.");
    }

    /* window = {key: max(floor, int(val)) for key, val, floor in
     *   (("start", start, 0), ("count", count, 1)) if val is not None}
     * except (TypeError, ValueError): "start and count must be integers." */
    int has_start = 0, has_count = 0;
    long start = 0, count = 0;
    if (start_node && start_node->type != JSON_NULL) {
        if (py_int_val(start_node, &start) != 0)
            return preview_tool_error("start and count must be integers.");
        has_start = 1;
        if (start < 0) start = 0;      /* max(0, int(start)) */
    }
    if (count_node && count_node->type != JSON_NULL) {
        if (py_int_val(count_node, &count) != 0)
            return preview_tool_error("start and count must be integers.");
        has_count = 1;
        if (count < 1) count = 1;      /* max(1, int(count)) */
    }

    char *raw = NULL;
    char *err = NULL;
    int rc = callback(ctx, has_start, start, has_count, count, &raw, &err);
    if (rc != 0) {
        /* except Exception as exc: tool_error(f"Failed to read the preview
         * pane: {exc}") */
        size_t need = snprintf(NULL, 0, "Failed to read the preview pane: %s",
                               err ? err : "");
        char *msg = malloc(need + 1);
        if (msg) snprintf(msg, need + 1, "Failed to read the preview pane: %s",
                          err ? err : "");
        free(err);
        char *out = preview_tool_error(msg ? msg : "");
        free(msg);
        return out;
    }
    free(err);

    /* if not raw: no tab open / read timed out */
    if (!raw || !*raw) {
        free(raw);
        return preview_tool_error(
            "No preview tab is open, or the read timed out.");
    }

    /* Desktop answers with a JSON object; pass it through, else wrap. */
    json_t *parsed = json_parse(raw, NULL);
    if (parsed) {
        char *out = json_serialize(parsed);
        json_free(parsed);
        free(raw);
        if (out) return out;
        return preview_tool_error("");
    }
    json_t *wrap = json_new_object();
    json_object_set(wrap, "text", json_new_string(raw));
    char *out = wrap ? json_serialize(wrap) : NULL;
    json_free(wrap);
    free(raw);
    if (out) return out;
    return preview_tool_error("");
}

/* PoP: READ_PREVIEW_SCHEMA @ tools/read_preview_tool.py */
char *read_preview_schema(void) {
    static const char *SCHEMA =
        "{"
        "\"name\": \"read_preview\", "
        "\"description\": "
        "\"Read what's currently shown in the in-app browser / preview pane of the "
        "Hermes desktop GUI (the pane open_preview opens beside this chat). Call "
        "with no arguments for the first window of the active tab's content. "
        "Returns JSON {kind, url, title, text, start, end, total_chars, note?}: "
        "a URL (Browser) tab's text is the rendered page's visible text — page "
        "through longer pages with `start`/`count` (character offsets, capped "
        "per read); a file tab answers identity only (read the file with "
        "read_file); an artifact tab points back at the conversation. Use after "
        "open_preview, or whenever the user refers to what's on screen in the "
        "browser ('what does this page say?'). To close the pane, use close_preview.\", "
        "\"parameters\": {"
        "\"type\": \"object\", "
        "\"properties\": {"
        "\"start\": {"
        "\"type\": \"integer\", "
        "\"description\": \"0-indexed character offset into the page text. Omit for the start.\""
        "}, "
        "\"count\": {"
        "\"type\": \"integer\", "
        "\"description\": \"Characters to return from start. Defaults to (and is capped at) the per-read maximum.\""
        "}"
        "}"
        "}"
        "}";
    return strdup(SCHEMA);
}

/* Platform callback injection seam (see header). */
static read_preview_cb g_callback;
static void *g_callback_ctx;

void read_preview_tool_set_callback(read_preview_cb callback, void *ctx) {
    g_callback = callback;
    g_callback_ctx = ctx;
}

/* handler=lambda args, **kw: read_preview_tool(start=args.get("start"),
 *   count=args.get("count"), callback=kw.get("callback")) */
static char *read_preview_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *start_n = NULL, *count_n = NULL;
    if (args_json) {
        json_t *args = json_parse(args_json, NULL);
        if (args) {
            start_n = json_object_get(args, "start");
            count_n = json_object_get(args, "count");
        }
    }
    return read_preview_tool(start_n, count_n, g_callback, g_callback_ctx);
}

/* PoP: module-level registry.register @ tools/read_preview_tool.py.
 * Note: emoji="🔍" is dropped — the C registry port has no emoji setter. */
bool read_preview_tool_register(void) {
    char *schema = read_preview_schema();
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
        "read_preview",
        desc ? desc : "",
        schema,
        "desktop_ui",
        read_preview_handler);
    free(schema);
    free(desc);
    return ok;
}
