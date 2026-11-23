/*
 * gateway_queue.c — C port of upstream Hermes hermes_cli/gateway_queue.py.
 *
 * PoP: w10_gatewayque_6a8e6b_published_queue @ hermes_cli/gateway_queue.py:published_queue
 * PoP: w10_gatewayque_6a8e6b_own_head @ hermes_cli/gateway_queue.py:_own_head
 * PoP: w10_gatewayque_6a8e6b_queued_rows @ hermes_cli/gateway_queue.py:queued_rows
 *
 * The three queue viewers are pure over the session state once prompt.receipt is
 * supplied as the `receipts` fixture seam; nothing here touches a socket.
 */
#include "noc_w10_gatewayque_6a8e6b.h"
#include "hermes_json.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNREAD_SEQ 4611686018427387904LL   /* 1 << 62 */

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
static char *py_str(const json_t *v) {
    if (!v) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else snprintf(b, sizeof(b), "%g", v->num_val);
        return strdup(b);
    }
    return strdup("");
}
/* Python: str(v or "") */
static char *str_or_empty(const json_t *v) {
    if (!truthy(v)) return strdup("");
    return py_str(v);
}
/* Python: int(v or 0) for the sequence fields (numeric or decimal string). */
static long long seq_int(const json_t *v) {
    if (!truthy(v)) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return 0;
}
static const char *admission_of(const json_t *row) {
    const json_t *a = json_obj_get(row, "admission_id");
    return (a && a->type == JSON_STRING && a->str_val[0]) ? a->str_val : NULL;
}
static int status_is(const json_t *row, const char *want) {
    const json_t *s = json_obj_get(row, "status");
    return s && s->type == JSON_STRING && !strcmp(s->str_val, want);
}
static long long queued_at_seq(const json_t *state, const char *admission) {
    const json_t *m = json_obj_get(state, "queued_at");
    const json_t *v = json_obj_get(m, admission);
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : -1;
}
static int contains_str(const json_t *arr, const char *s) {
    if (!json_is_array(arr) || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}
/* Object keys as a string set (dict insertion order preserved). */
static int key_in(const json_t *obj, const char *k) {
    return json_is_object(obj) && json_obj_get(obj, k) != NULL;
}

json_t *w10_gatewayque_6a8e6b_published_queue(const struct json_t *state) {
    json_t *out = json_array();
    const json_t *pending = json_obj_get(state, "pending");
    const json_t *inputs = json_obj_get(state, "queued_inputs");
    long long pseq = json_get_num(state, "pending_seq", 0.0);
    /* rows = [row for row in state.pending or () if isinstance(row, dict)] */
    for (size_t i = 0; i < (json_is_array(pending) ? json_len(pending) : 0); i++) {
        const json_t *row = json_get(pending, i);
        if (!json_is_object(row) || !status_is(row, "queued")) continue;
        char *t = str_or_empty(json_obj_get(row, "text"));
        json_append(out, json_string(t)); free(t);
    }
    /* fresh: inputs.items() with admission not among published rows and queued_at >= pending_seq */
    if (json_is_object(inputs)) {
        for (size_t i = 0; i < json_object_size(inputs); i++) {
            const char *a = json_object_get_key_at(inputs, i);
            int known = 0;
            for (size_t j = 0; j < (json_is_array(pending) ? json_len(pending) : 0); j++) {
                const json_t *row = json_get(pending, j);
                if (!json_is_object(row)) continue;
                const char *ra = admission_of(row);
                if (ra && !strcmp(ra, a)) { known = 1; break; }
            }
            if (known) continue;
            if (queued_at_seq(state, a) < pseq) continue;
            json_append(out, json_copy(json_object_get_at(inputs, i)));  /* raw text value */
        }
    }
    return out;
}

json_t *w10_gatewayque_6a8e6b_own_head(const struct json_t *state) {
    const json_t *pending = json_obj_get(state, "pending");
    const json_t *inputs = json_obj_get(state, "queued_inputs");
    const json_t *adopted = json_obj_get(state, "adopted");
    long long pseq = json_get_num(state, "pending_seq", 0.0);
    size_t np = json_is_array(pending) ? json_len(pending) : 0;
    /* any unknown row pauses the FIFO */
    for (size_t i = 0; i < np; i++) {
        const json_t *row = json_get(pending, i);
        if (json_is_object(row) && status_is(row, "unknown")) return json_null();
    }
    /* queued rows sorted by int(sequence or 0), stable */
    const char *heads[256]; long long seqs[256]; size_t nq = 0;
    for (size_t i = 0; i < np && nq < 256; i++) {
        const json_t *row = json_get(pending, i);
        const char *adm = admission_of(row);
        if (json_is_object(row) && adm && status_is(row, "queued")) {
            heads[nq] = adm; seqs[nq] = seq_int(json_obj_get(row, "sequence")); nq++;
        }
    }
    for (size_t i = 1; i < nq; i++) {  /* insertion sort, stable */
        const char *ha = heads[i]; long long sa = seqs[i]; size_t j = i;
        while (j > 0 && seqs[j - 1] > sa) { heads[j] = heads[j - 1]; seqs[j] = seqs[j - 1]; j--; }
        heads[j] = ha; seqs[j] = sa;
    }
    const char *head = NULL;
    if (nq) head = heads[0];
    else if (json_is_object(inputs)) {
        for (size_t i = 0; i < json_object_size(inputs); i++) {
            const char *a = json_object_get_key_at(inputs, i);
            int listed = 0;
            for (size_t j = 0; j < np; j++) {
                const json_t *row = json_get(pending, j);
                const char *ra = admission_of(row);
                if (json_is_object(pending) && json_is_object(row) && ra && !strcmp(ra, a)) { listed = 1; break; }
            }
            if (!listed && queued_at_seq(state, a) >= pseq) { head = a; break; }
        }
    }
    if (head && key_in(inputs, head) && !contains_str(adopted, head)) return json_string(head);
    return json_null();
}

typedef struct { const char *adm; long long seq; char *text; } Row;
static void row_set(Row *rows, size_t *n, const char *adm, long long seq, const char *text) {
    for (size_t i = 0; i < *n; i++) if (!strcmp(rows[i].adm, adm)) {
        rows[i].seq = seq; free(rows[i].text); rows[i].text = strdup(text); return;
    }
    rows[*n].adm = adm; rows[*n].seq = seq; rows[*n].text = strdup(text); (*n)++;
}
static void row_del(Row *rows, size_t *n, const char *adm) {
    for (size_t i = 0; i < *n; i++) if (!strcmp(rows[i].adm, adm)) {
        free(rows[i].text);
        for (size_t j = i; j + 1 < *n; j++) rows[j] = rows[j + 1];
        (*n)--; return;
    }
}

json_t *w10_gatewayque_6a8e6b_queued_rows(const struct json_t *state, const struct json_t *receipts) {
    json_t *out = json_array();
    const json_t *pending = json_obj_get(state, "pending");
    const json_t *inputs = json_obj_get(state, "queued_inputs");
    Row rows[512]; size_t n = 0;
    if (json_is_array(pending)) for (size_t i = 0; i < json_len(pending); i++) {
        const json_t *row = json_get(pending, i);
        const char *a = admission_of(row);
        if (!json_is_object(row) || !a || !status_is(row, "queued")) continue;
        char *t = str_or_empty(json_obj_get(row, "text"));
        row_set(rows, &n, a, seq_int(json_obj_get(row, "sequence")), t); free(t);
    }
    if (json_is_object(inputs)) {
        for (size_t i = 0; i < json_object_size(inputs) && n < 512; i++) {
            const char *a = json_object_get_key_at(inputs, i);
            const json_t *text = json_object_get_at(inputs, i);
            char *tv = str_or_empty(text);
            const json_t *rec = json_obj_get(receipts, a);
            if (!rec || truthy(json_obj_get(rec, "error"))) {
                int exists = 0;
                for (size_t j = 0; j < n; j++) if (!strcmp(rows[j].adm, a)) { exists = 1; break; }
                if (!exists) row_set(rows, &n, a, UNREAD_SEQ, tv);
            } else {
                const json_t *r = json_obj_get(rec, "receipt");
                if (status_is(r, "queued"))
                    row_set(rows, &n, a, seq_int(json_obj_get(r, "sequence")), tv);
                else
                    row_del(rows, &n, a);
            }
            free(tv);
        }
    }
    for (size_t i = 1; i < n; i++) {  /* stable sort by sequence */
        Row x = rows[i]; size_t j = i;
        while (j > 0 && rows[j - 1].seq > x.seq) { rows[j] = rows[j - 1]; j--; }
        rows[j] = x;
    }
    for (size_t i = 0; i < n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(rows[i].adm));
        json_append(pair, json_string(rows[i].text));
        json_append(out, pair);
    }
    for (size_t i = 0; i < n; i++) free(rows[i].text);
    return out;
}
