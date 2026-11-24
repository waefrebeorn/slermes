/*
 * _holder.c - C11 port of upstream persistence/sqlite/_holder.py
 * (gate w23_holder_f15834).
 *
 * PoP: w23_holder_f15834_coalesce @ persistence/sqlite/_holder.py:StoreHolder._coalesce_token_deltas
 * PoP: w23_holder_f15834_quarantine_reason @ persistence/sqlite/_holder.py:StoreHolder._quarantine_reason
 * PoP: w23_holder_f15834_constants @ persistence/sqlite/_holder.py:StoreHolder.<budget constants>
 *
 * The SQLite handle itself is a seam (one writer connection, a bounded
 * read-only pool, WAL generation guards, the accounting thread). What is ported
 * here is the part that is a pure function of values: how adjacent token/cost
 * deltas of one route coalesce, which advisory makes the handle refuse to
 * checkpoint, and the contention budgets those decisions are made against.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_holder_f15834.h"

/* _TOKEN_DELTA_SUM_FIELDS */
static const char *const HOLDER_SUM_FIELDS[W23_HOLDER_F15834_SUM_FIELDS] = {
    "input_tokens", "output_tokens", "cache_read_tokens",
    "cache_write_tokens", "reasoning_tokens", "api_call_count",
};

/* _TOKEN_DELTA_COST_FIELDS */
static const char *const HOLDER_COST_FIELDS[W23_HOLDER_F15834_COST_FIELDS] = {
    "estimated_cost_usd", "actual_cost_usd",
};

/* _TOKEN_DELTA_ROUTE_FIELDS */
static const char *const HOLDER_ROUTE_FIELDS[W23_HOLDER_F15834_ROUTE_FIELDS] = {
    "model", "cost_status", "cost_source", "pricing_version",
    "billing_provider", "billing_base_url", "billing_mode",
    "source", "task", "prompt_cache_clock",
};

/* Python truthiness of a JSON value. */
static int hd_truthy(const json_t *v) {
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

/* Python's numeric view: an int, a float or a bool; None is none of those. */
static int hd_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==' over the shapes a route key element can hold.  A missing key and
 * an explicit null are the same element: kwargs.get(f) is None for both. */
static int hd_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (hd_number(a, &x) && hd_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!hd_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !hd_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

typedef struct {
    json_t *sid;                                           /* owned */
    json_t *kwargs;                                        /* owned */
    const json_t *route[W23_HOLDER_F15834_ROUTE_FIELDS];   /* borrowed from batch */
    int keyless;                                           /* absolute -> key is None */
} hd_group;

static int hd_same_key(const hd_group *g, const json_t *sid, const json_t *const *route) {
    size_t i;
    if (g->keyless) return 0;
    if (!hd_equal(g->sid, sid)) return 0;
    for (i = 0; i < W23_HOLDER_F15834_ROUTE_FIELDS; i++)
        if (!hd_equal(g->route[i], route[i])) return 0;
    return 1;
}

/* merged[f] = merged.get(f, 0) + kwargs.get(f, 0) */
static void hd_merge_sum(json_t *merged, const json_t *kwargs, const char *f) {
    double base = json_get_num(merged, f, 0.0);
    double add = json_get_num(kwargs, f, 0.0);
    json_set(merged, f, json_number(base + add));
}

/* merged[f] = (merged.get(f) or 0.0) + value, only when value is not None. */
static void hd_merge_cost(json_t *merged, const json_t *kwargs, const char *f) {
    const json_t *v = json_obj_get(kwargs, f);
    double base;
    if (!v || v->type == JSON_NULL) return;
    base = json_get_num(merged, f, 0.0);
    if (v->type == JSON_BOOL) json_set(merged, f, json_number(base + (v->bool_val ? 1.0 : 0.0)));
    else if (v->type == JSON_NUMBER) json_set(merged, f, json_number(base + v->num_val));
}

json_t *w23_holder_f15834_coalesce(const json_t *batch) {
    hd_group *groups;
    size_t n = 0, cap, i, j;
    json_t *out;

    out = json_array();
    if (!batch || batch->type != JSON_ARRAY) return out;
    cap = json_len(batch);
    if (cap == 0) return out;
    groups = (hd_group *)calloc(cap, sizeof(hd_group));
    if (!groups) return out;

    for (i = 0; i < cap; i++) {
        const json_t *entry = json_get(batch, i);
        const json_t *kw = entry && entry->type == JSON_OBJECT ? json_obj_get(entry, "kwargs") : NULL;
        const json_t *sid = entry && entry->type == JSON_OBJECT ? json_obj_get(entry, "session_id") : NULL;
        const json_t *route[W23_HOLDER_F15834_ROUTE_FIELDS];
        int keyless;

        if (!kw || kw->type != JSON_OBJECT) continue;
        keyless = hd_truthy(json_obj_get(kw, "absolute"));
        for (j = 0; j < W23_HOLDER_F15834_ROUTE_FIELDS; j++)
            route[j] = keyless ? NULL : json_obj_get(kw, HOLDER_ROUTE_FIELDS[j]);

        if (n > 0 && !keyless && hd_same_key(&groups[n - 1], sid, route)) {
            for (j = 0; j < W23_HOLDER_F15834_SUM_FIELDS; j++)
                hd_merge_sum(groups[n - 1].kwargs, kw, HOLDER_SUM_FIELDS[j]);
            for (j = 0; j < W23_HOLDER_F15834_COST_FIELDS; j++)
                hd_merge_cost(groups[n - 1].kwargs, kw, HOLDER_COST_FIELDS[j]);
            continue;
        }
        groups[n].sid = sid ? json_copy(sid) : json_null();
        groups[n].kwargs = json_copy(kw);
        groups[n].keyless = keyless;
        for (j = 0; j < W23_HOLDER_F15834_ROUTE_FIELDS; j++) groups[n].route[j] = route[j];
        n++;
    }

    for (i = 0; i < n; i++) {
        json_t *row = json_object();
        json_set(row, "session_id", groups[i].sid ? groups[i].sid : json_null());
        json_set(row, "kwargs", groups[i].kwargs ? groups[i].kwargs : json_object());
        json_append(out, row);
    }
    free(groups);
    return out;
}

json_t *w23_holder_f15834_quarantine_reason(const json_t *state) {
    const json_t *reason, *corrupt;
    char *buf;
    size_t rl, need;
    json_t *out;

    if (!state || state->type != JSON_OBJECT) return json_null();
    corrupt = json_obj_get(state, "db_corrupt");
    if (hd_truthy(corrupt)) {
        reason = json_obj_get(state, "reason");
        if (reason && reason->type != JSON_STRING) reason = NULL;
        rl = reason ? strlen(reason->str_val ? reason->str_val : "") : 0;
        need = strlen("structural corruption ()") + rl + 1;
        buf = (char *)malloc(need);
        if (!buf) return json_null();
        snprintf(buf, need, "structural corruption (%s)",
                 reason ? (reason->str_val ? reason->str_val : "") : "");
        out = json_string(buf);
        free(buf);
        return out;
    }
    if (hd_truthy(json_obj_get(state, "db_replaced")))
        return json_string("a replaced state.db file");
    if (hd_truthy(json_obj_get(state, "wal_generation_lost")))
        return json_string("a deleted WAL generation (split-brain)");
    return json_null();
}

json_t *w23_holder_f15834_constants(void) {
    json_t *o = json_object();
    json_set(o, "read_open_retry_seconds", json_number(60.0));
    json_set(o, "read_only_ioerr_retry_attempts", json_number(3.0));
    json_set(o, "read_only_ioerr_retry_backoff_s", json_number(0.05));
    json_set(o, "read_busy_timeout_s", json_number(5.0));
    json_set(o, "write_patience_s", json_number(20.0));
    json_set(o, "transcript_write_patience_s", json_number(60.0));
    json_set(o, "activity_write_patience_s", json_number(0.5));
    json_set(o, "compression_busy_wait_s", json_number(5.0));
    json_set(o, "write_retry_min_s", json_number(0.02));
    json_set(o, "write_retry_max_s", json_number(0.15));
    json_set(o, "write_retry_slow_after_s", json_number(2.0));
    json_set(o, "write_retry_slow_min_s", json_number(0.25));
    json_set(o, "write_retry_slow_max_s", json_number(1.0));
    json_set(o, "checkpoint_every_n_writes", json_number(50.0));
    json_set(o, "fts_merge_every_n_writes", json_number(1000.0));
    json_set(o, "fts_merge_max_pages_per_index", json_number(500.0));
    json_set(o, "fts_merge_commands_per_pass", json_number(4.0));
    json_set(o, "token_writer_idle_seconds", json_number(30.0));
    return o;
}
