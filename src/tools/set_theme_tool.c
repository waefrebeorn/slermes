/*
 * set_theme_tool.c — C home for upstream Hermes tools/set_theme_tool.py.
 * PoP: w7_setthemeto_ee2735_accent_error @ tools/set_theme_tool.py:_accent_error
 * PoP: w7_setthemeto_ee2735_set_theme_tool @ tools/set_theme_tool.py:set_theme_tool
 *
 * The renderer callback is injected from the fixture as a boolean presence
 * flag; both sides use the same deterministic stub callback
 *   lambda d: {"accepted": d}
 * so the differential test exercises the module's validation/value building.
 */
#include "noc_w7_setthemeto_ee2735.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SWATCHES[] = {
    "mono", "green", "cyan", "nous", "violet", "pink", "red", "orange"};
static const char *const MODES[] = {"light", "dark", "system"};
static const char *const PICKERS[] = {"accent", "layout"};

static int in_list(const char *const *list, size_t n, const char *s) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++) if (!strcmp(list[i], s)) return 1;
    return 0;
}
static int is_hex_color(const char *s) {
    size_t i;
    if (!s || strlen(s) != 7 || s[0] != '#') return 0;
    for (i = 1; i < 7; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return 0;
    }
    return 1;
}
static char *strip_ws(const char *s) {
    char *out;
    size_t a, b, n;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    a = 0; b = n;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' || s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t' || s[b-1] == '\n' || s[b-1] == '\r' || s[b-1] == '\v' || s[b-1] == '\f')) b--;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}
/* Python repr() of a str, choosing double quotes only when the value has a
 * single quote and no double quote. */
static char *py_repr(const char *s) {
    const char *p;
    int dq = 0;
    size_t cap;
    char *buf, *o;
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) dq = 1;
    cap = strlen(s) * 6 + 4;
    buf = (char *)malloc(cap);
    if (!buf) return NULL;
    o = buf;
    *o++ = dq ? '"' : '\'';
    for (p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c == (unsigned char)(dq ? '"' : '\'')) { *o++ = '\\'; *o++ = (char)c; }
        else if (c == '\n') { *o++ = '\\'; *o++ = 'n'; }
        else if (c == '\r') { *o++ = '\\'; *o++ = 'r'; }
        else if (c == '\t') { *o++ = '\\'; *o++ = 't'; }
        else if (c < 0x20 || c == 0x7f) { sprintf(o, "\\x%02x", c); o += 4; }
        else *o++ = (char)c;
    }
    *o++ = dq ? '"' : '\'';
    *o = '\0';
    return buf;
}
static json_t *tool_error_obj(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg ? msg : ""));
    return o;
}
static json_t *avail_error(const char *msg, const char *const *available, size_t n) {
    json_t *o = json_object();
    size_t i;
    json_t *a = json_array();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : ""));
    for (i = 0; i < n; i++) json_append(a, json_string(available[i]));
    json_set(o, "available", a);
    return o;
}
static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }

json_t *w7_setthemeto_ee2735_accent_error(const char *accent) {
    char *msg;
    json_t *out;
    if (!accent) return NULL;
    if (in_list(SWATCHES, sizeof(SWATCHES)/sizeof(SWATCHES[0]), accent) ||
        !strcmp(accent, "theme") || is_hex_color(accent)) return NULL;
    msg = py_repr(accent);
    out = json_object();
    {
        char *full;
        size_t n = strlen(msg) + 128;
        full = (char *)malloc(n);
        if (full) {
            snprintf(full, n, "Unknown accent %s: use a swatch id, 'theme' or #rrggbb.", msg);
            json_set(out, "success", json_bool(false));
            json_set(out, "error", json_string(full));
            {
                json_t *a = json_array();
                size_t i;
                for (i = 0; i < sizeof(SWATCHES)/sizeof(SWATCHES[0]); i++) json_append(a, json_string(SWATCHES[i]));
                json_append(a, json_string("theme"));
                json_set(out, "available", a);
            }
            free(full);
        }
    }
    free(msg);
    return out;
}

json_t *w7_setthemeto_ee2735_set_theme_tool(json_t *accent, json_t *mode, json_t *layout,
                                            json_t *picker, int callback) {
    json_t *values, *out;
    const char *v;
    char *t;
    if (!callback) return tool_error_obj("set_theme is only available in the Hermes desktop app.");
    values = json_object();
    t = NULL; v = jstr(accent); if (v && (t = strip_ws(v)) && *t) { json_set(values, "accent", json_string(t)); } if (t) { free(t); t = NULL; }
    t = NULL; v = jstr(mode);   if (v && (t = strip_ws(v)) && *t) { json_set(values, "mode", json_string(t)); }   if (t) { free(t); t = NULL; }
    t = NULL; v = jstr(layout); if (v && (t = strip_ws(v)) && *t) { json_set(values, "layout", json_string(t)); } if (t) { free(t); t = NULL; }
    v = jstr(picker);
    if (picker && picker->type != JSON_NULL) {
        json_t *inner;
        if (!v || (!strcmp(v, "accent") ? 0 : strcmp(v, "layout") != 0))
            { json_free(values); return tool_error_obj("picker must be one of: accent, layout."); }
        if (json_object_size(values) > 0)
            { json_free(values); return tool_error_obj("picker shows a card for the user to choose; pass picker alone, without values."); }
        inner = json_object();
        json_set(inner, "picker", json_string(v));
        out = json_object();
        json_set(out, "accepted", inner);
        return out;
    }
    if (json_object_size(values) == 0)
        { json_free(values); return tool_error_obj("Pass at least one of accent, mode or layout, or picker to let the user choose."); }
    {
        json_t *m = json_obj_get(values, "mode");
        if (m && !in_list(MODES, sizeof(MODES)/sizeof(MODES[0]), jstr(m))) {
            char *r = py_repr(jstr(m));
            char *full = (char *)malloc(strlen(r) + 64);
            if (full) snprintf(full, strlen(r) + 64, "Unknown mode %s.", r);
            out = avail_error(full ? full : "", MODES, sizeof(MODES)/sizeof(MODES[0]));
            free(full); free(r); json_free(values); return out;
        }
    }
    {
        json_t *a = json_obj_get(values, "accent");
        json_t *e;
        if (a) {
            e = w7_setthemeto_ee2735_accent_error(jstr(a));
            if (e) { json_free(values); return e; }
        }
    }
    out = json_object();
    json_set(out, "accepted", values);
    return out;
}
