/*
 * _hosted_rooms_common.c - C11 port of upstream
 * persistence/sqlite/gateway/_hosted_rooms_common.py (gate w24_hostedroom_178ff8).
 *
 * PoP: w24_hostedroom_178ff8_fenced_update @ persistence/sqlite/gateway/_hosted_rooms_common.py:fenced_update
 * PoP: w24_hostedroom_178ff8_epoch @ persistence/sqlite/gateway/_hosted_rooms_common.py:_epoch
 * PoP: w24_hostedroom_178ff8_table_exists @ persistence/sqlite/gateway/_hosted_rooms_common.py:table_exists
 * PoP: w24_hostedroom_178ff8_table_columns @ persistence/sqlite/gateway/_hosted_rooms_common.py:table_columns
 * PoP: w24_hostedroom_178ff8_constants @ persistence/sqlite/gateway/_hosted_rooms_common.py:StaleFenceKind
 *
 * What is ported is the part that is a pure function of values: the rowcount
 * verdict a compare-and-swap UPDATE is judged by, the epoch fence test the
 * owner verbs share (an epoch that is not exactly an int is stale even when it
 * compares equal), the existence verdict for a sqlite_master row, and the
 * column-name set of the PRAGMA rows.  The connection - every execute call,
 * transaction and row factory - is the seam: the caller hands in what it read.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_hostedroom_178ff8.h"

/* Python truthiness of a JSON value. */
static int hc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python '==': numbers compare numerically (bool folds in), strings by text,
 * a null/missing key equals only null. */
static int hc_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    int an, bn;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    an = a->type == JSON_NUMBER || a->type == JSON_BOOL;
    bn = b->type == JSON_NUMBER || b->type == JSON_BOOL;
    if (an && bn) {
        x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

json_t *w24_hostedroom_178ff8_fenced_update(const json_t *req) {
    const json_t *rowcount = json_obj_get(req, "rowcount");
    json_t *out = json_object();
    int ok = rowcount && rowcount->type == JSON_NUMBER && rowcount->num_val == 1.0;
    json_set(out, "ok", json_bool(ok));
    return out;
}

json_t *w24_hostedroom_178ff8_epoch(const json_t *req) {
    const json_t *epoch = json_obj_get(req, "epoch");
    const char *etype = json_get_str(req, "epoch_type", "int");
    const json_t *row_epoch = json_obj_get(req, "row_epoch");
    int row_present = hc_truthy(json_obj_get(req, "row_present"));
    int is_int = epoch && epoch->type == JSON_NUMBER && !strcmp(etype, "int");
    int stale = !is_int || !row_present || !hc_equal(row_epoch, epoch);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(!stale));
    if (stale) {
        /* FenceStale(StaleFenceKind.STALE_EPOCH, "runtime_epoch", "singleton") */
        json_set(out, "reason", json_string("stale_epoch"));
        json_set(out, "entity", json_string("runtime_epoch"));
        json_set(out, "entity_id", json_string("singleton"));
    }
    return out;
}

json_t *w24_hostedroom_178ff8_table_exists(const json_t *req) {
    return json_bool(hc_truthy(json_obj_get(req, "row_present")));
}

/* Numeric view of a JSON value (bool folds to 0/1). */
static double hc_num(const json_t *v) {
    if (!v) return 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    return 0.0;
}

json_t *w24_hostedroom_178ff8_table_columns(const json_t *req) {
    const json_t *rows = json_obj_get(req, "rows");
    size_t n = json_len(rows), i, j, keep = 0;
    json_t **names;
    json_t *out;
    json_type_t kind;

    if (!rows || rows->type != JSON_ARRAY) return json_array();
    names = (json_t **)calloc(n ? n : 1, sizeof(json_t *));
    if (!names) return json_array();
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rows, i);
        const json_t *name;
        int dup = 0;
        /* frozenset(row[1] for row in ...): a short row is IndexError. */
        if (!row || row->type != JSON_ARRAY || json_len(row) < 2) {
            for (j = 0; j < keep; j++) json_free(names[j]);
            free(names);
            out = json_object();
            json_set(out, "error", json_string("index"));
            return out;
        }
        name = json_get(row, 1);
        for (j = 0; j < keep; j++) if (hc_equal(names[j], name)) dup = 1;
        if (!dup) names[keep++] = name ? json_copy(name) : json_null();
    }
    /* sorted(frozenset): one element type, or the Python TypeError. */
    kind = keep ? names[0]->type : JSON_STRING;
    for (i = 0; i < keep; i++) {
        if (names[i]->type != kind) {
            for (j = 0; j < keep; j++) json_free(names[j]);
            free(names);
            out = json_object();
            json_set(out, "error", json_string("type"));
            return out;
        }
    }
    for (i = 0; i + 1 < keep; i++)
        for (j = i + 1; j < keep; j++) {
            int swap;
            if (kind == JSON_STRING)
                swap = strcmp(names[i]->str_val ? names[i]->str_val : "",
                              names[j]->str_val ? names[j]->str_val : "") > 0;
            else if (kind == JSON_NUMBER || kind == JSON_BOOL)
                swap = hc_num(names[i]) > hc_num(names[j]);
            else
                swap = 0;
            if (swap) {
                json_t *tmp = names[i];
                names[i] = names[j];
                names[j] = tmp;
            }
        }
    out = json_array();
    for (i = 0; i < keep; i++) json_append(out, names[i]);
    free(names);
    return out;
}

json_t *w24_hostedroom_178ff8_constants(void) {
    json_t *o = json_object();
    json_t *kinds = json_array();
    json_append(kinds, json_string("stale_epoch"));
    json_append(kinds, json_string("stale_generation"));
    json_append(kinds, json_string("revision_conflict"));
    json_set(o, "stale_fence_kinds", kinds);
    json_set(o, "epoch_reason", json_string("stale_epoch"));
    return o;
}
