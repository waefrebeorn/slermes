/*
 * prompt_refresh.c - C11 port of upstream
 * persistence/sqlite/conversation/_store/prompt_refresh.py
 * (gate w24_promptrefr_587f3f).
 *
 * PoP: w24_promptrefr_587f3f_refresh_key @ persistence/sqlite/conversation/_store/prompt_refresh.py:request_prompt_refresh
 * PoP: w24_promptrefr_587f3f_next_epoch @ persistence/sqlite/conversation/_store/prompt_refresh.py:request_prompt_refresh._do
 * PoP: w24_promptrefr_587f3f_epoch_from_row @ persistence/sqlite/conversation/_store/prompt_refresh.py:prompt_refresh_epoch
 * PoP: w24_promptrefr_587f3f_count_from_row @ persistence/sqlite/conversation/_store/prompt_refresh.py:count_sessions_with_stored_prompt
 * PoP: w24_promptrefr_587f3f_constants @ persistence/sqlite/conversation/_store/prompt_refresh.py:<module>
 *
 * The holder is a seam: ``_execute_write`` (one UPSERT) and ``_read_one`` (a
 * MAX/COUNT projection) never run here.  Ported is everything those statements
 * decide from values: which state_meta key a request targets, that the epoch is
 * ``int(row[0] or 0) + 1``, and the two row -> int collapses (with ``or 0`` for
 * the MAX probe and without it for the COUNT probe).
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_promptrefr_587f3f.h"

#define PROFILE_KEY "prompt_refresh.epoch"
#define SESSION_KEY_PREFIX "prompt_refresh.epoch:"

/* Python ``x or 0`` -> int(x).  1 on success.  int() of a string is the
 * stripped, optionally signed, all-digit form; of a float, truncation toward
 * zero; of a bool, 0/1; None is a TypeError. */
static int pr_int(const json_t *v, long long *out) {
    const char *s;
    char buf[64];
    size_t i, n;
    int neg = 0;
    unsigned long long acc = 0, limit;

    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:
        *out = v->bool_val ? 1 : 0;
        return 1;
    case JSON_NUMBER:
        if (!isfinite(v->num_val)) return 0;
        *out = (long long)v->num_val;   /* truncates toward zero, like int() */
        return 1;
    case JSON_STRING:
        s = v->str_val ? v->str_val : "";
        n = strlen(s);
        if (n >= sizeof(buf)) return 0;
        memcpy(buf, s, n + 1);
        /* Python str.strip() */
        i = 0;
        while (i < n && isspace((unsigned char)buf[i])) i++;
        while (n > i && isspace((unsigned char)buf[n - 1])) n--;
        buf[n] = '\0';
        if (i == n) return 0;                    /* int("") -> ValueError */
        if (buf[i] == '+' || buf[i] == '-') { neg = buf[i] == '-'; i++; }
        if (i == n) return 0;
        limit = neg ? 9223372036854775808ULL : 9223372036854775807ULL;
        for (; i < n; i++) {
            if (!isdigit((unsigned char)buf[i])) return 0;   /* ValueError */
            if (acc > (limit - (unsigned long long)(buf[i] - '0')) / 10ULL) return 0;
            acc = acc * 10ULL + (unsigned long long)(buf[i] - '0');
        }
        *out = neg ? (long long)(0ULL - acc) : (long long)acc;
        return 1;
    default:
        return 0;                                /* None / list / dict: TypeError */
    }
}

/* Python truthiness, used for the ``or 0`` in ``row[0] or 0``. */
static int pr_truthy(const json_t *v) {
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

/* ``x or 0`` then int(): 0 is the fallback, so it never fails there. */
static int pr_int_or_zero(const json_t *v, long long *out) {
    if (!pr_truthy(v)) { *out = 0; return 1; }
    return pr_int(v, out);
}

json_t *w24_promptrefr_587f3f_refresh_key(const json_t *session_id) {
    const char *sid;
    char *buf;
    size_t need;

    if (!session_id || session_id->type == JSON_NULL)
        return json_string(PROFILE_KEY);
    if (session_id->type != JSON_STRING)
        return json_null();            /* str + non-str -> TypeError seam */
    sid = session_id->str_val ? session_id->str_val : "";
    need = strlen(SESSION_KEY_PREFIX) + strlen(sid) + 1;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    snprintf(buf, need, "%s%s", SESSION_KEY_PREFIX, sid);
    {
        json_t *out = json_string(buf);
        free(buf);
        return out;
    }
}

json_t *w24_promptrefr_587f3f_next_epoch(const json_t *current) {
    long long epoch = 0;
    if (!pr_int_or_zero(current, &epoch)) return json_null();
    return json_number((double)(epoch + 1));
}

/* row[0] of a one-column projection.  Python guards with ``if row`` (a
 * truthiness test, so an empty row is the same as a missing one), so the
 * caller checks truthiness first; here the row is truthy already. */
static const json_t *pr_row_head(const json_t *row) {
    if (row->type != JSON_ARRAY) return NULL;          /* not subscriptable */
    return json_len(row) > 0 ? json_get(row, 0) : NULL;
}

json_t *w24_promptrefr_587f3f_epoch_from_row(const json_t *row) {
    const json_t *head;
    long long epoch = 0;

    if (!pr_truthy(row)) return json_number(0.0);      /* ``if row`` false -> 0 */
    if (row->type != JSON_ARRAY) return json_null();   /* TypeError seam */
    head = pr_row_head(row);
    if (!pr_int_or_zero(head, &epoch)) return json_null();
    return json_number((double)epoch);
}

json_t *w24_promptrefr_587f3f_count_from_row(const json_t *row) {
    const json_t *head;
    long long count = 0;

    if (!pr_truthy(row)) return json_number(0.0);
    if (row->type != JSON_ARRAY) return json_null();
    head = pr_row_head(row);
    if (!pr_int(head, &count)) return json_null();      /* int(None) / int("x") seam */
    return json_number((double)count);
}

json_t *w24_promptrefr_587f3f_constants(void) {
    json_t *o = json_object();
    json_set(o, "profile_key", json_string(PROFILE_KEY));
    json_set(o, "session_key_prefix", json_string(SESSION_KEY_PREFIX));
    return o;
}
