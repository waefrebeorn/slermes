/*
 * browser_vault_context.c - C home for upstream Hermes tools/browser_vault_context.py.
 * PoP: w15_browservau_3f8d5c_saved_origins @ tools/browser_vault_context.py:saved_origins
 * PoP: w15_browservau_3f8d5c_snapshot_refusal @ tools/browser_vault_context.py:snapshot_refusal
 *
 * Origin-binding helpers for the desktop `fill_context` vault path. Backend metadata
 * (allowed_origins / origin / kind) and the "has its own context" fact are injected
 * as JSON/ints; only the deterministic binding checks and refusal construction live
 * here. No backend, lock, or plugin is ever touched by this translation unit.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_browservau_3f8d5c.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }

static int jtruthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}
/* Python str() coercion for the scalar origins we ever see here. */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL)   return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}

struct json_t *w15_browservau_3f8d5c_saved_origins(const struct json_t *meta) {
    const json_t *ao = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, "allowed_origins") : NULL;
    if (ao && ao->type == JSON_ARRAY && json_len(ao) > 0) return json_copy(ao);
    if (ao && ao->type == JSON_STRING && ao->str_val && ao->str_val[0]) {
        /* Faithful to Python list(<str>): split into single characters. */
        json_t *r = json_array();
        for (const char *p = ao->str_val; *p; p++) { char t[2] = { *p, 0 }; json_append(r, json_string(t)); }
        return r;
    }
    const json_t *origin = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, "origin") : NULL;
    if (jtruthy(origin)) {
        json_t *r = json_array();
        char *s = py_str(origin);
        json_append(r, json_string(s ? s : ""));
        free(s);
        return r;
    }
    return json_array();
}

struct json_t *w15_browservau_3f8d5c_snapshot_refusal(int custom_context, const struct json_t *fresh,
                                                     const char *meta_kind, const char *page_origin) {
    const char *po = page_origin ? page_origin : "";
    const char *mk = meta_kind ? meta_kind : "";
    /* Built-in backends use LoginBackend.fill_context and freeze nothing. */
    if (!custom_context) return json_null();
    int binds = 0;
    if (fresh && fresh->type == JSON_OBJECT) {
        const json_t *fk = json_obj_get(fresh, "kind");
        int same_kind = 0;
        if (fk && fk->type == JSON_STRING) same_kind = !strcmp(fk->str_val ? fk->str_val : "", mk);
        else if (fk && fk->type == JSON_NULL) same_kind = 0;  /* None == "" is False in Python */
        if (same_kind) {
            json_t *origins = w15_browservau_3f8d5c_saved_origins(fresh);
            for (size_t i = 0; i < json_len(origins); i++) {
                const json_t *o = json_get(origins, i);
                if (o && o->type == JSON_STRING && o->str_val && !strcmp(o->str_val, po)) { binds = 1; break; }
            }
            json_free(origins);
        }
    }
    if (binds) return json_null();
    size_t n = strlen(po) + 160;
    char *msg = (char *)malloc(n);
    snprintf(msg, n, "Refused: the saved item changed before the fill and no longer binds %s. Nothing was resolved or written.", po);
    json_t *r = json_object();
    json_set(r, "success", json_bool(false));
    json_set(r, "error_type", json_string("origin_mismatch"));
    json_set(r, "error", json_string(msg));
    free(msg);
    return r;
}
