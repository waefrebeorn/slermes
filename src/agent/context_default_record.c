/*
 * context_default_record.c — faithful C11 port of agent/context_default_record.py
 * (gate w16_contextdef_baf639).
 *
 * PoP: w16_contextdef_baf639_key @ agent/context_default_record.py:_key
 * PoP: w16_contextdef_baf639_was_context_defaulted @ agent/context_default_record.py:was_context_defaulted
 * PoP: w16_contextdef_baf639_record_resolution @ agent/context_default_record.py:records_context_defaults
 *
 * Python reads hermes_constants.hermes_home_key() and thread-local contextvars;
 * both are injected here as explicit seams (profile_home, outer_present, flag).
 * The record bookkeeping (str().strip().rstrip('/') key normalisation, the 512
 * cap/clear, the pop-on-no-default) is ported verbatim.
 */
#include "noc_w16_contextdef_baf639.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    size_t a = 0, b;
    if (!t) return NULL;
    b = strlen(t);
    while (a < b && isspace((unsigned char)t[a])) a++;
    while (b > a && isspace((unsigned char)t[b - 1])) b--;
    if (a && b > a) memmove(t, t + a, b - a);
    t[b - a] = '\0';                 /* NUL at the NEW length (pitfall #2) */
    return t;
}

static void rstrip_slashes(char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && s[n - 1] == '/') s[--n] = '\0';
}

typedef struct { char *model; char *base; long long window; } Rec;

static void rec_init(Rec *r, const char *model, const char *base, const char *window) {
    r->model = strip_dup(model);
    r->base = strip_dup(base);
    rstrip_slashes(r->base);
    r->window = strtoll(window ? window : "0", NULL, 10);
}

static void rec_free(Rec *r) { free(r->model); free(r->base); r->model = r->base = NULL; }

static int rec_same(const Rec *a, const char *home, const char *model, const char *base) {
    return !strcmp(a->model, model) && !strcmp(a->base, base) && home != NULL;
}

static json_t *key_array(const char *home, const char *model, const char *base) {
    char *m = strip_dup(model);
    char *b = strip_dup(base);
    json_t *arr = json_array();
    rstrip_slashes(b);
    json_append(arr, json_string(home ? home : ""));
    json_append(arr, json_string(m ? m : ""));
    json_append(arr, json_string(b ? b : ""));
    free(m); free(b);
    return arr;
}

json_t *w16_contextdef_baf639_key(const char *profile_home, const char *model, const char *base_url) {
    return key_array(profile_home, model, base_url);
}

json_t *w16_contextdef_baf639_was_context_defaulted(const json_t *records, const char *profile_home,
                                                    const char *context_length, const char *model,
                                                    const char *base_url) {
    char *m, *b;
    long long want;
    int found = 0;
    size_t i, n;
    if (!profile_home) profile_home = "";
    m = strip_dup(model); b = strip_dup(base_url); rstrip_slashes(b);
    want = strtoll(context_length ? context_length : "0", NULL, 10);
    n = (records && records->type == JSON_ARRAY) ? records->c.count : 0;
    for (i = 0; i < n; ++i) {
        const json_t *o = json_get(records, i);
        const json_t *rm, *rb, *rw;
        if (!o || o->type != JSON_OBJECT) continue;
        rm = json_obj_get(o, "model"); rb = json_obj_get(o, "base_url"); rw = json_obj_get(o, "window");
        char *om = strip_dup(rm && rm->type == JSON_STRING ? rm->str_val : "");
        char *ob = strip_dup(rb && rb->type == JSON_STRING ? rb->str_val : "");
        rstrip_slashes(ob);
        long long ow = strtoll(rw && rw->type == JSON_STRING ? rw->str_val : "0", NULL, 10);
        if (!strcmp(om, m) && !strcmp(ob, b) && ow == want) found = 1;
        free(om); free(ob);
        if (found) break;
    }
    free(m); free(b);
    return json_bool(found);
}

json_t *w16_contextdef_baf639_record_resolution(const json_t *records, const char *profile_home,
                                                long long max_records, int outer_present, int flag,
                                                const char *context_length, const char *model,
                                                const char *base_url) {
    Rec *v = NULL;
    size_t n = 0, cap = 0, i;
    char *m, *b;
    long long cl;
    json_t *out = json_object();
    json_t *jrecords = json_array();
    int outer_set = (flag && outer_present) ? 1 : 0;

    if (!profile_home) profile_home = "";
    if (max_records <= 0) max_records = 512;
    m = strip_dup(model); b = strip_dup(base_url); rstrip_slashes(b);
    cl = strtoll(context_length ? context_length : "0", NULL, 10);

    if (records && records->type == JSON_ARRAY) {
        size_t rn = records->c.count;
        if (rn) { v = (Rec *)calloc(rn, sizeof(Rec)); cap = rn; }
        for (i = 0; i < rn; ++i) {
            const json_t *o = json_get(records, i);
            const json_t *rm = o && o->type == JSON_OBJECT ? json_obj_get(o, "model") : NULL;
            const json_t *rb = o && o->type == JSON_OBJECT ? json_obj_get(o, "base_url") : NULL;
            const json_t *rw = o && o->type == JSON_OBJECT ? json_obj_get(o, "window") : NULL;
            rec_init(&v[n], rm && rm->type == JSON_STRING ? rm->str_val : "",
                           rb && rb->type == JSON_STRING ? rb->str_val : "",
                           rw && rw->type == JSON_STRING ? rw->str_val : "0");
            n++;
        }
    }

    if (flag) {
        if ((long long)n >= max_records) {
            for (i = 0; i < n; ++i) rec_free(&v[i]);
            n = 0;                     /* _RECORDS.clear() */
        }
        size_t found = n;
        for (i = 0; i < n; ++i) {
            if (!strcmp(v[i].model, m) && !strcmp(v[i].base, b)) { found = i; break; }
        }
        if (found < n) {
            v[found].window = cl;      /* overwrite existing key */
        } else {
            if (n == cap) {
                Rec *nv = (Rec *)realloc(v, (cap + 1) * sizeof(Rec));
                if (nv) { v = nv; cap++; }
            }
            if (n < cap) { rec_init(&v[n], m, b, context_length); n++; }
        }
    } else {
        for (i = 0; i < n; ++i) {
            if (!strcmp(v[i].model, m) && !strcmp(v[i].base, b)) {
                rec_free(&v[i]);
                if (i + 1 < n) memmove(&v[i], &v[i + 1], (n - i - 1) * sizeof(Rec));
                n--;
                break;
            }
        }
    }

    for (i = 0; i < n; ++i) {
        json_t *o = json_object();
        json_set(o, "model", json_string(v[i].model));
        json_set(o, "base_url", json_string(v[i].base));
        json_set(o, "window", json_number((double)v[i].window));
        json_append(jrecords, o);
    }
    json_set(out, "records", jrecords);
    json_set(out, "outer_set", json_bool(outer_set));
    if (flag) json_set(out, "last_defaulted", json_number((double)cl));
    else json_set(out, "last_defaulted", json_null());

    for (i = 0; i < n; ++i) rec_free(&v[i]);
    free(v); free(m); free(b);
    return out;
}
