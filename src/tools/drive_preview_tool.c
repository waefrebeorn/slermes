/*
 * drive_preview_tool.c — Faithful C11 port of tools/drive_preview_tool.py.
 *
 * Interact with the in-app browser / preview pane in the Hermes desktop GUI —
 * clicking, typing, scrolling, and history. Thin dispatcher over the
 * platform-injected blocking-prompt bridge callback.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "preview_desktop_ui_tools.h"

const char *const DRIVE_PREVIEW_ACTIONS[] = {
    "elements", "click", "hover", "type", "scroll",
    "press", "strobe", "back", "forward", "reload",
};
const size_t DRIVE_PREVIEW_ACTIONS_N = 10;
const char *const DRIVE_PREVIEW_SCROLL_TO[] = { "top", "bottom" };
const size_t DRIVE_PREVIEW_SCROLL_TO_N = 2;

/* Verbs that need something to act on; bare `scroll` scrolls the page. */
const char *const DRIVE_PREVIEW_NEEDS_TARGET[] = { "click", "hover", "type", "press" };
const size_t DRIVE_PREVIEW_NEEDS_TARGET_N = 4;

static char *dp_tool_error(const char *message)
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
    char *out = strdup(p);
    if (!out) return NULL;
    size_t n = strlen(out);
    while (n > 0 && (out[n-1]==' '||out[n-1]=='\t'||out[n-1]=='\n'||out[n-1]=='\r')) n--;
    out[n] = '\0';
    for (char *q = out; *q; q++) *q = (char)tolower((unsigned char)*q);
    return out;
}

static int str_in_set(const char *s, const char *const *set, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

/* Mirror Python int(val): number truncates toward zero, bool is 1/0, string
 * is a strict base-10 parse of the whole stripped value; else error (-1). */
static int py_int_val(const json_t *node, long *out)
{
    if (!node || !out) return -1;
    switch (node->type) {
    case JSON_NUMBER:
        *out = (long)node->num_val;
        return 0;
    case JSON_BOOL:
        *out = node->bool_val ? 1 : 0;
        return 0;
    case JSON_STRING: {
        const char *s = node->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (!*s) return -1;
        errno = 0;
        char *end = NULL;
        long v = strtol(s, &end, 10);
        if (errno != 0 || end == s) return -1;
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
        if (*end != '\0') return -1;
        *out = v;
        return 0;
    }
    default:
        return -1;
    }
}

/* PoP: drive_preview_tool @ tools/drive_preview_tool.py:drive_preview_tool */
char *drive_preview_tool(const json_t *args)
{
    void *cb_ctx = NULL;
    preview_bridge_cb cb = preview_bridge_get(&cb_ctx);
    if (!cb)
        return dp_tool_error(
            "drive_preview is only available in the Hermes desktop app.");

    char *verb = strip_lower_dup(json_get_str(args, "action", ""));
    if (!verb) return dp_tool_error("alloc failed");
    if (!str_in_set(verb, DRIVE_PREVIEW_ACTIONS, DRIVE_PREVIEW_ACTIONS_N)) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "action must be one of: %s, %s, %s, %s, %s, %s, %s, %s, %s, %s.",
                 DRIVE_PREVIEW_ACTIONS[0], DRIVE_PREVIEW_ACTIONS[1],
                 DRIVE_PREVIEW_ACTIONS[2], DRIVE_PREVIEW_ACTIONS[3],
                 DRIVE_PREVIEW_ACTIONS[4], DRIVE_PREVIEW_ACTIONS[5],
                 DRIVE_PREVIEW_ACTIONS[6], DRIVE_PREVIEW_ACTIONS[7],
                 DRIVE_PREVIEW_ACTIONS[8], DRIVE_PREVIEW_ACTIONS[9]);
        free(verb);
        return dp_tool_error(buf);
    }

    json_t *ref = json_obj_get(args, "ref");
    json_t *sel = json_obj_get(args, "selector");
    int has_ref = ref && ref->type == JSON_STRING && ref->str_val[0];
    int has_sel = sel && sel->type == JSON_STRING && sel->str_val[0];

    int needs_target = str_in_set(verb, DRIVE_PREVIEW_NEEDS_TARGET,
                                  DRIVE_PREVIEW_NEEDS_TARGET_N);
    if (needs_target && !has_ref && !has_sel) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "%s needs a ref from action='elements' (e.g. 'btn-sign-in') "
                 "or a CSS selector.", verb);
        free(verb);
        return dp_tool_error(buf);
    }

    /* type needs the text to enter. */
    if (strcmp(verb, "type") == 0 && !json_has(args, "text")) {
        free(verb);
        return dp_tool_error("type needs the text to enter.");
    }

    /* press needs a key. */
    const char *key = NULL;
    json_t *jkey = json_obj_get(args, "key");
    if (jkey && jkey->type == JSON_STRING) key = jkey->str_val;
    if (strcmp(verb, "press") == 0 && !(key && key[0])) {
        free(verb);
        return dp_tool_error("press needs a key, e.g. 'Enter' or 'Escape'.");
    }

    /* to must be one of SCROLL_TO. */
    json_t *jto = json_obj_get(args, "to");
    if (jto && jto->type == JSON_STRING &&
        !str_in_set(jto->str_val, DRIVE_PREVIEW_SCROLL_TO, DRIVE_PREVIEW_SCROLL_TO_N)) {
        char buf[128];
        snprintf(buf, sizeof buf, "to must be one of: %s, %s.",
                 DRIVE_PREVIEW_SCROLL_TO[0], DRIVE_PREVIEW_SCROLL_TO[1]);
        free(verb);
        return dp_tool_error(buf);
    }

    /* payload with int(amount)->"amount", int(limit)->"max"; None omitted.
     * Python's TypeError/ValueError here -> "amount and max must be integers." */
    json_t *payload = json_object();
    if (!payload) { free(verb); return dp_tool_error("alloc failed"); }
    json_set(payload, "action", json_string(verb));
    static const char *pass_keys[] =
        { "ref", "selector", "text", "key", "submit", "full", "to" };
    for (size_t i = 0; i < sizeof(pass_keys)/sizeof(char*); i++) {
        json_t *v = json_obj_get(args, pass_keys[i]);
        if (v && v->type != JSON_NULL)
            json_set(payload, pass_keys[i], json_copy(v));
    }
    json_t *amt = json_obj_get(args, "amount");
    if (amt && amt->type != JSON_NULL) {
        long iv;
        if (py_int_val(amt, &iv) != 0) {
            json_free(payload); free(verb);
            return dp_tool_error("amount and max must be integers.");
        }
        json_set(payload, "amount", json_number((double)iv));
    }
    json_t *lim = json_obj_get(args, "limit");
    if (lim && lim->type != JSON_NULL) {
        long iv;
        if (py_int_val(lim, &iv) != 0) {
            json_free(payload); free(verb);
            return dp_tool_error("amount and max must be integers.");
        }
        json_set(payload, "max", json_number((double)iv));
    }
    free(verb);

    char *raw = cb(cb_ctx, payload);
    json_free(payload);

    if (!raw)
        return dp_tool_error(
            "The action timed out, or no GUI window answered. "
            "Open a page with open_preview first.");

    /* The renderer answers with a JSON object; pass it through, else wrap it. */
    json_t *parsed = json_parse(raw, NULL);
    if (parsed) {
        char *out = json_serialize(parsed);
        json_free(parsed);
        free(raw);
        return out;
    }
    json_t *wrap = json_object();
    if (!wrap) { free(raw); return dp_tool_error("alloc failed"); }
    json_set(wrap, "text", json_string(raw));
    free(raw);
    char *out = json_serialize(wrap);
    json_free(wrap);
    return out;
}
