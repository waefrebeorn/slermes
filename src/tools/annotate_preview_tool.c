/*
 * annotate_preview_tool.c — Faithful C11 port of tools/annotate_preview_tool.py.
 *
 * Leave a lasting mark on the page in the desktop GUI's in-app browser.
 * Rides the same preview.act bridge as drive_preview — one more verb on a
 * wire that exists. Thin dispatcher over the platform-injected callback.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "preview_desktop_ui_tools.h"

const char *const ANNOTATE_PREVIEW_ACTIONS[] = { "add", "hold", "remove", "clear" };
const size_t ANNOTATE_PREVIEW_ACTIONS_N = 4;

/* WIRE: verbs the renderer knows, keyed by ours. `clear` is `unpin` with
 * nothing to aim at, which the overlay reads as "all of them". */
static const struct { const char *verb; const char *wire; } ANNOTATE_WIRE[] = {
    { "add", "pin" }, { "hold", "hold" }, { "remove", "unpin" }, { "clear", "unpin" },
};

static char *ap_tool_error(const char *message)
{
    json_t *o = json_object();
    if (!o) return strdup("{\"error\":\"\"}");
    json_set(o, "error", json_string(message ? message : ""));
    char *out = json_serialize(o);
    json_free(o);
    return out;
}

static char *strip_lower_dup(const char *s)
{
    const char *p = s ? s : "";
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (!*p) return strdup("add"); /* Python default action="add" */
    char *out = strdup(p);
    if (!out) return NULL;
    size_t n = strlen(out);
    while (n > 0 && (out[n-1]==' '||out[n-1]=='\t'||out[n-1]=='\n'||out[n-1]=='\r')) n--;
    out[n] = '\0';
    for (char *q = out; *q; q++) *q = (char)tolower((unsigned char)*q);
    return out;
}

/* PoP: annotate_preview_tool @ tools/annotate_preview_tool.py:annotate_preview_tool */
char *annotate_preview_tool(const json_t *args)
{
    void *cb_ctx = NULL;
    preview_bridge_cb cb = preview_bridge_get(&cb_ctx);
    if (!cb)
        return ap_tool_error(
            "annotate_preview is only available in the Hermes desktop app.");

    char *verb = strip_lower_dup(json_get_str(args, "action", "add"));
    if (!verb) return ap_tool_error("alloc failed");
    int in_actions = 0;
    for (size_t i = 0; i < ANNOTATE_PREVIEW_ACTIONS_N; i++)
        if (strcmp(verb, ANNOTATE_PREVIEW_ACTIONS[i]) == 0) { in_actions = 1; break; }
    if (!in_actions) {
        char buf[160];
        snprintf(buf, sizeof buf, "action must be one of: %s, %s, %s, %s.",
                 ANNOTATE_PREVIEW_ACTIONS[0], ANNOTATE_PREVIEW_ACTIONS[1],
                 ANNOTATE_PREVIEW_ACTIONS[2], ANNOTATE_PREVIEW_ACTIONS[3]);
        free(verb);
        return ap_tool_error(buf);
    }

    int needs_target = strcmp(verb, "add") == 0 || strcmp(verb, "remove") == 0;
    json_t *ref = json_obj_get(args, "ref");
    json_t *sel = json_obj_get(args, "selector");
    int has_ref = ref && ref->type == JSON_STRING && ref->str_val[0];
    int has_sel = sel && sel->type == JSON_STRING && sel->str_val[0];
    if (needs_target && !has_ref && !has_sel) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "%s needs a ref from drive_preview action='elements' "
                 "(e.g. 'btn-sign-in') or a CSS selector.", verb);
        free(verb);
        return ap_tool_error(buf);
    }

    /* payload: action=WIRE[verb]; ref/selector dropped for clear/hold;
     * text=label; None values omitted. */
    const char *wire = "unpin";
    for (size_t i = 0; i < sizeof(ANNOTATE_WIRE)/sizeof(ANNOTATE_WIRE[0]); i++)
        if (strcmp(verb, ANNOTATE_WIRE[i].verb) == 0) { wire = ANNOTATE_WIRE[i].wire; break; }

    int aimless = strcmp(verb, "clear") == 0 || strcmp(verb, "hold") == 0;

    json_t *payload = json_object();
    if (!payload) { free(verb); return ap_tool_error("alloc failed"); }
    json_set(payload, "action", json_string(wire));
    if (!aimless) {
        if (ref && ref->type != JSON_NULL)
            json_set(payload, "ref", json_copy(ref));
        if (sel && sel->type != JSON_NULL)
            json_set(payload, "selector", json_copy(sel));
    }
    json_t *label = json_obj_get(args, "label");
    if (label && label->type != JSON_NULL)
        json_set(payload, "text", json_copy(label));
    free(verb);

    char *raw = cb(cb_ctx, payload);
    json_free(payload);

    if (!raw)
        return ap_tool_error(
            "The annotation timed out, or no GUI window answered. "
            "Open a page with open_preview first.");

    json_t *parsed = json_parse(raw, NULL);
    if (parsed) {
        char *out = json_serialize(parsed);
        json_free(parsed);
        free(raw);
        return out;
    }
    json_t *wrap = json_object();
    if (!wrap) { free(raw); return ap_tool_error("alloc failed"); }
    json_set(wrap, "text", json_string(raw));
    free(raw);
    char *out = json_serialize(wrap);
    json_free(wrap);
    return out;
}
