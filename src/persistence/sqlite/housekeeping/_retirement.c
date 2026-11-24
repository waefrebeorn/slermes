/*
 * _retirement.c -- real C11 port of the deletion-tombstone reclaim of upstream
 * Hermes persistence/sqlite/housekeeping/_retirement.py.
 *
 * PoP: w21_retirement_867fec_prune_plan @ persistence/sqlite/housekeeping/_retirement.py:prune_retired_tombstones
 * PoP: w21_retirement_867fec_identity_key @ persistence/sqlite/conversation/_store/terminal.py:identity_key
 * PoP: w21_retirement_867fec_constants @ persistence/sqlite/housekeeping/_retirement.py:prune_retired_tombstones
 *
 * The live function runs three statements and two `_reclaim_expired` sweeps over
 * `state_meta` on an open connection. The port keeps every decision it makes --
 * the stamp pass, the cutoff, the key set each DELETE removes, the order of the
 * two reclaim sweeps -- and takes the rows as data (the DB seam). `now` is the
 * clock seam; the identity pointer is the live `identity_key`, digest included.
 */
#include "hermes_json.h"
#include "noc_w21_retirement_867fec.h"
#include "crypto.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTR_ADMISSION_PREFIX "gateway.terminal_admission.v1."
#define RTR_RESULT_PREFIX    "gateway.admission.result.v1."
#define RTR_WORKER_PREFIX    "gateway.terminal_worker.v1."
#define RTR_IDENTITY_PREFIX  "gateway.terminal_identity.v1."
#define RTR_RETIRED_PREFIX   "gateway.retired_session.v1."
#define RTR_ROUTE_PREFIX     "gateway.retired_route.v1."
#define RTR_RETRY_HORIZON    604800.0

typedef struct {
    char buf[8192];
    size_t len;
} rtr_buf;

static void rtr_init(rtr_buf *b) { b->len = 0; b->buf[0] = '\0'; }

static void rtr_add(rtr_buf *b, const char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    if (b->len + n + 1 >= sizeof(b->buf)) n = sizeof(b->buf) - b->len - 1;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void rtr_addf(rtr_buf *b, const char *fmt, ...) {
    char tmp[4096];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(tmp)) tmp[sizeof(tmp) - 1] = '\0';
    rtr_add(b, tmp);
}

static int rtr_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    return (d < -9.007199254740992e15 || d > 9.007199254740992e15) ? 0 : 1;
}

/* --- json.dumps(..., ensure_ascii=False, sort_keys=True, separators=(',',':')) */

static void rtr_json_str(rtr_buf *b, const char *s) {
    const unsigned char *p;
    rtr_add(b, "\"");
    for (p = (const unsigned char *)(s ? s : ""); *p; p++) {
        switch (*p) {
        case '"':  rtr_add(b, "\\\""); break;
        case '\\': rtr_add(b, "\\\\"); break;
        case '\b': rtr_add(b, "\\b"); break;
        case '\f': rtr_add(b, "\\f"); break;
        case '\n': rtr_add(b, "\\n"); break;
        case '\r': rtr_add(b, "\\r"); break;
        case '\t': rtr_add(b, "\\t"); break;
        default:
            if (*p < 0x20) rtr_addf(b, "\\u%04x", (unsigned)*p);
            else rtr_addf(b, "%c", *p);
        }
    }
    rtr_add(b, "\"");
}

static int rtr_cmp_key(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void rtr_canon(rtr_buf *b, const json_t *v) {
    size_t i;
    if (!v || v->type == JSON_NULL) { rtr_add(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:
        rtr_add(b, "null");
        return;
    case JSON_BOOL:
        rtr_add(b, v->bool_val ? "true" : "false");
        return;
    case JSON_NUMBER:
        if (rtr_is_py_int(v)) rtr_addf(b, "%lld", (long long)v->num_val);
        else rtr_addf(b, "%.17g", v->num_val);
        return;
    case JSON_STRING:
        rtr_json_str(b, v->str_val);
        return;
    case JSON_ARRAY:
        rtr_add(b, "[");
        for (i = 0; i < v->c.count; i++) {
            if (i) rtr_add(b, ",");
            rtr_canon(b, v->c.items[i]);
        }
        rtr_add(b, "]");
        return;
    case JSON_OBJECT: {
        const char *keys[64];
        size_t n = 0, j;
        for (i = 0; i < v->c.count && n < 64; i++) {
            if (v->c.keys[i]) keys[n++] = v->c.keys[i];
        }
        qsort(keys, n, sizeof(keys[0]), rtr_cmp_key);
        rtr_add(b, "{");
        for (j = 0; j < n; j++) {
            if (j) rtr_add(b, ",");
            rtr_json_str(b, keys[j]);
            rtr_add(b, ":");
            rtr_canon(b, json_obj_get(v, keys[j]));
        }
        rtr_add(b, "}");
        return;
    }
    }
}

/* --- constants -------------------------------------------------------------- */

json_t *w21_retirement_867fec_constants(void) {
    json_t *out = json_object();
    json_set(out, "admission", json_string(RTR_ADMISSION_PREFIX));
    json_set(out, "result", json_string(RTR_RESULT_PREFIX));
    json_set(out, "worker", json_string(RTR_WORKER_PREFIX));
    json_set(out, "identity", json_string(RTR_IDENTITY_PREFIX));
    json_set(out, "retired", json_string(RTR_RETIRED_PREFIX));
    json_set(out, "route_fence", json_string(RTR_ROUTE_PREFIX));
    json_set(out, "retry_horizon", json_number(RTR_RETRY_HORIZON));
    return out;
}

/* --- identity_key ----------------------------------------------------------- */

char *w21_retirement_867fec_identity_key(const json_t *principal, const json_t *target,
                                         const json_t *request) {
    static const char *const HEX = "0123456789abcdef";
    rtr_buf b;
    unsigned char digest[CRYPTO_SHA256_LEN];
    char *out;
    size_t i, n;

    rtr_init(&b);
    rtr_add(&b, "{\"payload\":{\"principal\":");
    rtr_canon(&b, principal);
    rtr_add(&b, ",\"request\":");
    rtr_canon(&b, request);
    rtr_add(&b, "},\"target\":");
    rtr_canon(&b, target);
    rtr_add(&b, "}");

    crypto_sha256((const unsigned char *)b.buf, b.len, digest);
    n = strlen(RTR_IDENTITY_PREFIX) + 2 * CRYPTO_SHA256_LEN;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, RTR_IDENTITY_PREFIX, strlen(RTR_IDENTITY_PREFIX));
    for (i = 0; i < CRYPTO_SHA256_LEN; i++) {
        out[strlen(RTR_IDENTITY_PREFIX) + 2 * i] = HEX[digest[i] >> 4];
        out[strlen(RTR_IDENTITY_PREFIX) + 2 * i + 1] = HEX[digest[i] & 0xf];
    }
    out[n] = '\0';
    return out;
}

/* --- prune plan ------------------------------------------------------------- */

typedef struct {
    const char *key;
    json_t *val;     /* owned: parsed from the raw text, NULL when not valid JSON */
    int stamped;
    int deleted;
    int ra_null;     /* json_extract(value,'$.retired_at') IS NULL */
    int has_ra;      /* that extract compares as a number */
    double ra;
} rtr_row;

/* `json_extract(value,'$.retired_at')`: NULL when the member is absent or
 * null; numeric when it is a number or bool; a text member is never NULL but
 * never compares below a number either. */
static void rtr_read_retired_at(rtr_row *row) {
    json_t *v = row->val ? json_obj_get(row->val, "retired_at") : NULL;
    row->has_ra = 0;
    row->ra = 0.0;
    row->ra_null = 1;
    if (!v || v->type == JSON_NULL) return;
    row->ra_null = 0;
    if (v->type == JSON_NUMBER) { row->has_ra = 1; row->ra = v->num_val; }
    else if (v->type == JSON_BOOL) { row->has_ra = 1; row->ra = v->bool_val ? 1.0 : 0.0; }
}

/* `AND json_extract(value, '$.expires')` -- SQLite truthiness. */
static int rtr_expires_truthy(const json_t *val) {
    json_t *v = val ? json_obj_get(val, "expires") : NULL;
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: {
        char *end = NULL;
        double d = v->str_val ? strtod(v->str_val, &end) : 0.0;
        if (!end || end == v->str_val) return 0;
        return d != 0.0 ? 1 : 0;
    }
    default: return 0;
    }
}

static int rtr_has_prefix(const char *key, const char *prefix) {
    size_t n;
    if (!key) return 0;
    n = strlen(prefix);
    return strncmp(key, prefix, n) == 0 ? 1 : 0;
}

static void rtr_mark_deleted(rtr_row *rows, size_t n, const char *key) {
    size_t i;
    if (!key) return;
    for (i = 0; i < n; i++) {
        if (rows[i].key && !strcmp(rows[i].key, key)) rows[i].deleted = 1;
    }
}

json_t *w21_retirement_867fec_prune_plan(const json_t *rows, double now,
                                         int has_older_than, double older_than) {
    size_t n = 0, i;
    rtr_row *row = NULL;
    const char **stamp_order = NULL;
    size_t stamps = 0;
    double cutoff = now - (has_older_than ? older_than : RTR_RETRY_HORIZON);
    json_t *out = json_object();
    json_t *stamped = json_array();
    json_t *deleted = json_array();
    json_t *reclaim = json_array();
    const char *const sweep[2] = {RTR_RETIRED_PREFIX, RTR_ROUTE_PREFIX};

    if (rows && rows->type == JSON_ARRAY && rows->c.count) {
        n = rows->c.count;
        row = (rtr_row *)calloc(n, sizeof(rtr_row));
    }
    if (!row) n = 0;
    if (n) stamp_order = (const char **)malloc(n * sizeof(char *));

    /* Load the table: a string `value` is raw JSON text, anything else is the
     * row's value (serialized by the caller). */
    for (i = 0; i < n; i++) {
        json_t *r = rows->c.items[i];
        json_t *v;
        row[i].key = json_get_str(r, "key", "");
        v = json_obj_get(r, "value");
        if (v && v->type == JSON_STRING) row[i].val = json_parse(v->str_val, NULL);
        else if (v) row[i].val = json_copy(v);
        rtr_read_retired_at(&row[i]);
    }

    /* UPDATE ... json_set(value,'$.retired_at',now) for the admission and
     * worker tombstones that carry no stamp yet -- one statement per prefix,
     * so the stamps land prefix by prefix. */
    {
        const char *const stamp_prefix[2] = {RTR_ADMISSION_PREFIX, RTR_WORKER_PREFIX};
        int pass;
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < n; i++) {
                if (!row[i].val) continue;
                if (!rtr_has_prefix(row[i].key, stamp_prefix[pass])) continue;
                if (!row[i].ra_null) continue;
                row[i].stamped = 1;
                if (stamp_order) stamp_order[stamps++] = row[i].key;
                row[i].ra_null = 0;
                /* json_set only writes into an object root; on a scalar or
                 * array the statement matches but extracts NULL afterwards. */
                if (row[i].val->type == JSON_OBJECT) {
                    row[i].has_ra = 1;
                    row[i].ra = now;
                }
            }
        }
    }

    /* SELECT key,value ... admission tombstones retired before the cutoff. */
    for (i = 0; i < n; i++) {
        json_t *admission_id, *principal, *session, *request;
        if (!row[i].val || !rtr_has_prefix(row[i].key, RTR_ADMISSION_PREFIX)) continue;
        if (!row[i].has_ra || !(row[i].ra < cutoff)) continue;
        rtr_mark_deleted(row, n, row[i].key);
        admission_id = json_obj_get(row[i].val, "admission_id");
        principal = json_obj_get(row[i].val, "principal_id");
        session = json_obj_get(row[i].val, "target_session_id");
        request = json_obj_get(row[i].val, "request_id");
        if (admission_id && admission_id->type == JSON_STRING) {
            rtr_buf k;
            rtr_init(&k);
            rtr_add(&k, RTR_RESULT_PREFIX);
            rtr_add(&k, admission_id->str_val);
            rtr_mark_deleted(row, n, k.buf);
        }
        if (principal && session && request) {
            char *ikey = w21_retirement_867fec_identity_key(principal, session, request);
            rtr_mark_deleted(row, n, ikey);
            free(ikey);
        }
    }

    /* DELETE ... worker tombstones retired before the cutoff. */
    for (i = 0; i < n; i++) {
        if (!row[i].val || !rtr_has_prefix(row[i].key, RTR_WORKER_PREFIX)) continue;
        if (!row[i].has_ra || !(row[i].ra < cutoff)) continue;
        rtr_mark_deleted(row, n, row[i].key);
    }

    /* _reclaim_expired(conn, prefix, cutoff) for sessions then route fences:
     * only a sweep's marker carries `expires`; a delete's fence never does. */
    {
        int pass;
        for (pass = 0; pass < 2; pass++) {
            json_t *pair = json_array();
            for (i = 0; i < n; i++) {
                if (!row[i].val || !rtr_has_prefix(row[i].key, sweep[pass])) continue;
                if (!row[i].has_ra || !(row[i].ra < cutoff)) continue;
                if (!rtr_expires_truthy(row[i].val)) continue;
                rtr_mark_deleted(row, n, row[i].key);
            }
            json_append(pair, json_string(sweep[pass]));
            json_append(pair, json_number(cutoff));
            json_append(reclaim, pair);
        }
    }

    for (i = 0; i < stamps; i++) json_append(stamped, json_string(stamp_order[i]));
    free(stamp_order);
    {
        /* Sorted: the DELETE's row order is SQLite's, not the caller's. */
        const char **names = NULL;
        size_t m = 0;
        if (n) names = (const char **)malloc(n * sizeof(char *));
        if (names) {
            for (i = 0; i < n; i++) {
                if (row[i].deleted) names[m++] = row[i].key;
            }
            qsort(names, m, sizeof(names[0]), rtr_cmp_key);
            for (i = 0; i < m; i++) json_append(deleted, json_string(names[i]));
            free(names);
        }
    }

    for (i = 0; i < n; i++) json_free(row[i].val);
    free(row);
    json_set(out, "stamped", stamped);
    json_set(out, "deleted", deleted);
    json_set(out, "reclaim", reclaim);
    return out;
}
