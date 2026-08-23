/*
 * tour_tool.c — Faithful C11 port of tools/tour_tool.py.
 *
 * Run a guided tour (highlight + narrate UI elements) in the Hermes desktop
 * GUI. Schema + a thin dispatcher over the platform-injected blocking-prompt
 * bridge callback. Also owns the shared bridge seam storage used by
 * annotate_preview_tool / drive_preview_tool / close_preview_tool.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "preview_desktop_ui_tools.h"

const char *const TOUR_ACTIONS[] = { "targets", "show", "start", "next", "prev", "stop" };
const size_t TOUR_ACTIONS_N = 6;
const char *const TOUR_SURFACES[] = { "app", "preview" };
const size_t TOUR_SURFACES_N = 2;
const char *const TOUR_SIDES[] = { "top", "right", "bottom", "left" };
const size_t TOUR_SIDES_N = 4;

/* ── shared blocking-prompt bridge seam (single storage, see header) ────── */
static preview_bridge_cb g_bridge;
static void *g_bridge_ctx;

void preview_bridge_set_callback(preview_bridge_cb cb, void *ctx)
{
    g_bridge = cb;
    g_bridge_ctx = ctx;
}

preview_bridge_cb preview_bridge_get(void **out_ctx)
{
    if (out_ctx) *out_ctx = g_bridge_ctx;
    return g_bridge;
}

static char *tt_tool_error(const char *message)
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

/* non-empty string node helper */
static int nn_str(const json_t *v)
{
    return v && v->type == JSON_STRING && v->str_val && v->str_val[0];
}

/* _empty: every highlighted moment needs something to point at or to say. */
static int step_is_empty(const json_t *sel, const json_t *title, const json_t *text)
{
    return !(nn_str(sel) || nn_str(title) || nn_str(text));
}

/* PoP: tour_tool @ tools/tour_tool.py:tour_tool */
char *tour_tool(const json_t *args)
{
    preview_bridge_cb cb = preview_bridge_get(NULL);
    if (!cb)
        return tt_tool_error("tour is only available in the Hermes desktop app.");

    char *verb = strip_lower_dup(json_get_str(args, "action", ""));
    if (!verb || !str_in_set(verb, TOUR_ACTIONS, TOUR_ACTIONS_N)) {
        char buf[256];
        snprintf(buf, sizeof buf,
                 "action must be one of: %s, %s, %s, %s, %s, %s.",
                 TOUR_ACTIONS[0], TOUR_ACTIONS[1], TOUR_ACTIONS[2],
                 TOUR_ACTIONS[3], TOUR_ACTIONS[4], TOUR_ACTIONS[5]);
        free(verb);
        return tt_tool_error(buf);
    }

    char *where = strip_lower_dup(json_get_str(args, "surface", "app"));
    if (!where) where = strdup("app");
    if (!where) { free(verb); return tt_tool_error("alloc failed"); }
    if (!str_in_set(where, TOUR_SURFACES, TOUR_SURFACES_N)) {
        char buf[128];
        snprintf(buf, sizeof buf, "surface must be one of: %s, %s.",
                 TOUR_SURFACES[0], TOUR_SURFACES[1]);
        free(verb); free(where);
        return tt_tool_error(buf);
    }

    json_t *side = json_obj_get(args, "side");
    if (side && side->type == JSON_STRING &&
        !str_in_set(side->str_val, TOUR_SIDES, TOUR_SIDES_N)) {
        char buf[128];
        snprintf(buf, sizeof buf, "side must be one of: %s, %s, %s, %s.",
                 TOUR_SIDES[0], TOUR_SIDES[1], TOUR_SIDES[2], TOUR_SIDES[3]);
        free(verb); free(where);
        return tt_tool_error(buf);
    }

    /* show needs a selector and/or title/text for the popover. */
    if (strcmp(verb, "show") == 0 &&
        step_is_empty(json_obj_get(args, "selector"),
                      json_obj_get(args, "title"),
                      json_obj_get(args, "text"))) {
        free(verb); free(where);
        return tt_tool_error("show needs a selector (and/or title/text for the popover).");
    }

    /* start needs a non-empty steps array of non-empty objects. */
    json_t *steps = json_obj_get(args, "steps");
    if (strcmp(verb, "start") == 0) {
        if (!steps || steps->type != JSON_ARRAY || json_len(steps) == 0) {
            free(verb); free(where);
            return tt_tool_error("start needs a non-empty steps array.");
        }
        for (size_t i = 0; i < json_len(steps); i++) {
            json_t *st = json_get(steps, i);
            if (!st || st->type != JSON_OBJECT) {
                char buf[64];
                snprintf(buf, sizeof buf, "steps[%zu] must be an object.", i);
                free(verb); free(where);
                return tt_tool_error(buf);
            }
            if (step_is_empty(json_obj_get(st, "selector"),
                              json_obj_get(st, "title"),
                              json_obj_get(st, "text"))) {
                char buf[96];
                snprintf(buf, sizeof buf,
                         "steps[%zu] needs a selector and/or title/text.", i);
                free(verb); free(where);
                return tt_tool_error(buf);
            }
        }
    }

    /* payload: only keys whose value is not None. */
    json_t *payload = json_object();
    if (!payload) { free(verb); free(where); return tt_tool_error("alloc failed"); }
    json_set(payload, "action", json_string(verb));
    json_set(payload, "surface", json_string(where));
    static const char *opt_keys[] =
        { "selector", "title", "text", "side", "steps", "step_index" };
    for (size_t i = 0; i < sizeof(opt_keys)/sizeof(char*); i++) {
        json_t *v = json_obj_get(args, opt_keys[i]);
        if (v && v->type != JSON_NULL) {
            /* deep copy so the payload owns its values */
            json_t *cp = json_copy(v);
            if (cp) json_set(payload, opt_keys[i], cp);
        }
    }
    free(verb); free(where);

    char *raw = cb(g_bridge_ctx, payload);
    json_free(payload);

    if (!raw)
        return tt_tool_error(
            "The tour request timed out, or no GUI window answered. "
            "For surface='preview' open a page in the preview pane first.");

    /* The renderer answers with a JSON object; pass it through, else wrap it. */
    json_t *parsed = json_parse(raw, NULL);
    if (parsed) {
        char *out = json_serialize(parsed);
        json_free(parsed);
        free(raw);
        return out;
    }
    json_t *wrap = json_object();
    if (!wrap) { free(raw); return tt_tool_error("alloc failed"); }
    json_set(wrap, "text", json_string(raw));
    free(raw);
    char *out = json_serialize(wrap);
    json_free(wrap);
    return out;
}
