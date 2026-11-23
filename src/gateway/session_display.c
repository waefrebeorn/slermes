/*
 * session_display.c — C home for upstream Hermes gateway/session_display.py.
 *
 * PoP: w9_sessiondisp_b6cff2_submit_fields @ gateway/session_display.py:submit_display_fields
 * PoP: w9_sessiondisp_b6cff2_admit       @ gateway/session_display.py:admit_display
 * PoP: w9_sessiondisp_b6cff2_restore     @ gateway/session_display.py:restore_display
 *
 * Per-turn presentation fields through canonical admission.  Only "hidden" is
 * a client-authorable display kind; any other kind (or a non-string title
 * preview) is refused.  Refusals are surfaced as {"error":"AuthorityRefusal"},
 * matching the AuthorityRefusal('invalid_params') the Python raises.
 */
#include "noc_w9_sessiondisp_b6cff2.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *HIDDEN = "hidden";
#define TITLE_PREVIEW_LIMIT 1000

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

static json_t *refusal(void) {
    json_t *o = json_object();
    json_set(o, "error", json_string("AuthorityRefusal"));
    return o;
}

static int json_equal(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; ++i)
                if (!json_equal(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; ++i) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !json_equal(a->c.items[i], bv)) return 0;
            }
            return 1;
    }
    return 0;
}

static char *py_strip(const char *s) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *r = (char *)malloc(n + 1);
    if (r) { memcpy(r, s, n); r[n] = '\0'; }
    return r;
}

struct json_t *w9_sessiondisp_b6cff2_submit_fields(const struct json_t *params) {
    json_t *out = json_object();
    static const char *const keys[] = {"display_kind", "title_preview"};
    for (size_t i = 0; i < 2; ++i) {
        json_t *v = json_obj_get(params, keys[i]);
        if (v) json_set(out, keys[i], json_copy(v));
    }
    return out;
}

struct json_t *w9_sessiondisp_b6cff2_admit(const struct json_t *params) {
    json_t *kind = json_obj_get(params, "display_kind");
    json_t *preview = json_obj_get(params, "title_preview");

    /* if kind is not None and kind != HIDDEN: raise AuthorityRefusal */
    if (kind && kind->type != JSON_NULL &&
        !(kind->type == JSON_STRING && kind->str_val && !strcmp(kind->str_val, HIDDEN)))
        return refusal();
    /* if preview is not None and not isinstance(preview, str): raise */
    if (preview && preview->type != JSON_NULL && preview->type != JSON_STRING)
        return refusal();

    json_t *committed = json_object();
    if (py_truthy(kind))
        json_set(committed, "kind", json_string(HIDDEN));
    if (py_truthy(preview)) {
        char *s = py_strip(preview->str_val);
        int has_content = s && s[0];
        free(s);
        if (has_content) {
            const char *raw = preview->str_val ? preview->str_val : "";
            size_t n = strlen(raw);
            if (n > TITLE_PREVIEW_LIMIT) n = TITLE_PREVIEW_LIMIT;
            char *sub = (char *)malloc(n + 1);
            if (sub) { memcpy(sub, raw, n); sub[n] = '\0'; json_set(committed, "title_preview", json_string(sub)); free(sub); }
        }
    }

    json_t *out = json_object();
    if (committed->c.count) json_set(out, "display_v1", committed);
    else json_free(committed);
    return out;
}

struct json_t *w9_sessiondisp_b6cff2_restore(const struct json_t *committed) {
    if (!committed || committed->type != JSON_OBJECT || committed->c.count == 0)
        return refusal();
    for (size_t i = 0; i < committed->c.count; ++i) {
        const char *k = committed->c.keys[i];
        if (strcmp(k, "kind") && strcmp(k, "title_preview"))
            return refusal();
    }
    /* wire = {'display_kind': committed.get('kind'), 'title_preview': committed.get('title_preview')} */
    json_t *wire = json_object();
    json_t *k = json_obj_get(committed, "kind");
    json_t *p = json_obj_get(committed, "title_preview");
    json_set(wire, "display_kind", k ? json_copy(k) : json_null());
    json_set(wire, "title_preview", p ? json_copy(p) : json_null());
    json_t *admitted = w9_sessiondisp_b6cff2_admit(wire);
    json_free(wire);
    json_t *dv = json_obj_get(admitted, "display_v1");
    int ok = json_equal(dv, committed);
    json_free(admitted);
    if (!ok) return refusal();
    return json_copy(committed);
}
