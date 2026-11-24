/*
 * health.c - C11 port of upstream persistence/sqlite/_state/health.py
 * (gate w24_health_c83367).
 *
 * PoP: w24_health_c83367_latch @ persistence/sqlite/_state/health.py:mark_storage_corrupt/storage_state/storage_corrupt_reason/reset_storage_state
 * PoP: w24_health_c83367_log_message @ persistence/sqlite/_state/health.py:mark_storage_corrupt (logger.error text)
 * PoP: w24_health_c83367_constants @ persistence/sqlite/_state/health.py:STORAGE_OK/STORAGE_CORRUPT/__all__
 *
 * The real module keeps one dict behind a threading.Lock and keys it with a
 * resolved absolute path.  The lock and the resolver are the seams: here the
 * table is local to one call and the path is the caller's opaque key, so the
 * observable transition table -- first reason wins, the latch never clears on
 * its own, reset forgets one path or all of them -- is what gets compared.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_health_c83367.h"

/* Exports, in upstream __all__ order (sorted for the JSON table). */
static const char *const HL_ALL[] = {
    "STORAGE_CORRUPT", "STORAGE_OK", "is_structural_corruption_error",
    "mark_storage_corrupt", "note_storage_error", "reset_storage_state",
    "storage_corrupt_reason", "storage_state",
};
#define HL_ALL_COUNT (sizeof(HL_ALL) / sizeof(HL_ALL[0]))

typedef struct {
    char *key;
    char *reason;
} hl_entry;

typedef struct {
    hl_entry *v;
    size_t n, cap;
} hl_table;

static char *hl_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* str(value): the scalar shapes a latch reason takes.  Containers are out of
 * scope (their repr is not a value->value mapping worth owning). */
static char *hl_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return hl_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return hl_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return hl_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            fabs(v->num_val) < 1e16)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.15g", v->num_val);
        return hl_dup(buf);
    default:
        return hl_dup("");
    }
}

static hl_entry *hl_find(hl_table *t, const char *key) {
    size_t i;
    for (i = 0; i < t->n; i++)
        if (!strcmp(t->v[i].key, key)) return &t->v[i];
    return NULL;
}

/* Returns 1 when the entry was created, 0 when the key was already latched. */
static int hl_mark(hl_table *t, const char *key, char *reason /* owned */) {
    hl_entry *e = hl_find(t, key);
    if (e) {
        free(reason);
        return 0;
    }
    if (t->n == t->cap) {
        size_t cap = t->cap ? t->cap * 2 : 8;
        hl_entry *grown = (hl_entry *)realloc(t->v, cap * sizeof *grown);
        if (!grown) {
            free(reason);
            return 0;
        }
        t->v = grown;
        t->cap = cap;
    }
    t->v[t->n].key = hl_dup(key);
    t->v[t->n].reason = reason;
    t->n++;
    return 1;
}

static void hl_drop(hl_table *t, const char *key) {
    size_t i;
    for (i = 0; i < t->n; i++) {
        if (!strcmp(t->v[i].key, key)) {
            free(t->v[i].key);
            free(t->v[i].reason);
            t->v[i] = t->v[t->n - 1];
            t->n--;
            return;
        }
    }
}

static void hl_clear(hl_table *t) {
    size_t i;
    for (i = 0; i < t->n; i++) {
        free(t->v[i].key);
        free(t->v[i].reason);
    }
    free(t->v);
    t->v = NULL;
    t->n = t->cap = 0;
}

json_t *w24_health_c83367_latch(const json_t *ops) {
    hl_table t = {NULL, 0, 0};
    json_t *out = json_array();
    size_t i, n;

    if (!ops || ops->type != JSON_ARRAY) return out;
    n = json_len(ops);
    for (i = 0; i < n; i++) {
        const json_t *op = json_get(ops, i);
        const char *kind = op && op->type == JSON_OBJECT ? json_get_str(op, "op", "") : "";
        const char *path = op && op->type == JSON_OBJECT ? json_get_str(op, "path", "") : "";
        if (!strcmp(kind, "mark")) {
            hl_mark(&t, path, hl_str(json_obj_get(op, "reason")));
            json_append(out, json_null());
        } else if (!strcmp(kind, "state")) {
            json_append(out, json_string(hl_find(&t, path) ? W24_HEALTH_C83367_STORAGE_CORRUPT
                                                           : W24_HEALTH_C83367_STORAGE_OK));
        } else if (!strcmp(kind, "reason")) {
            hl_entry *e = hl_find(&t, path);
            json_append(out, e ? json_string(e->reason) : json_null());
        } else if (!strcmp(kind, "reset")) {
            hl_drop(&t, path);
            json_append(out, json_null());
        } else if (!strcmp(kind, "reset_all")) {
            hl_clear(&t);
            json_append(out, json_null());
        } else {
            json_append(out, json_string("UNKNOWN_OP"));
        }
    }
    hl_clear(&t);
    return out;
}

json_t *w24_health_c83367_log_message(const json_t *req) {
    char *path = hl_str(json_obj_get(req, "path"));
    char *reason = hl_str(json_obj_get(req, "reason"));
    size_t need;
    char *buf;
    json_t *out;

    need = strlen("state.db at  is structurally corrupt (); session storage is reported "
                  "as corrupt until Hermes restarts on a recovered or restored file. Stop "
                  "Hermes, then run `hermes sessions recover --source ` --inspect-only` or "
                  "restore a snapshot.")
           + 2 * strlen(path) + strlen(reason) + 1;
    buf = (char *)malloc(need);
    if (!buf) {
        free(path);
        free(reason);
        return json_null();
    }
    snprintf(buf, need,
             "state.db at %s is structurally corrupt (%s); session storage is reported as "
             "corrupt until Hermes restarts on a recovered or restored file. Stop Hermes, "
             "then run `hermes sessions recover --source %s --inspect-only` or restore a "
             "snapshot.",
             path, reason, path);
    out = json_string(buf);
    free(buf);
    free(path);
    free(reason);
    return out;
}

json_t *w24_health_c83367_constants(void) {
    json_t *o = json_object();
    json_t *all = json_array();
    size_t i;
    json_set(o, "storage_ok", json_string(W24_HEALTH_C83367_STORAGE_OK));
    json_set(o, "storage_corrupt", json_string(W24_HEALTH_C83367_STORAGE_CORRUPT));
    for (i = 0; i < HL_ALL_COUNT; i++) json_append(all, json_string(HL_ALL[i]));
    json_set(o, "all", all);
    return o;
}
