/*
 * persisted_turn_receipt.c — faithful C11 port of agent/persisted_turn_receipt.py
 * (gate w16_persistedt_fc545e).
 *
 * PoP: w16_persistedt_fc545e_committed_row_id @ agent/persisted_turn_receipt.py:committed_row_id
 * PoP: w16_persistedt_fc545e_receipt @ agent/persisted_turn_receipt.py:persisted_turn_receipt
 *
 * _DB_PERSISTED_MARKER == "_db_persisted" (session_rules.turns). type(row_id) is
 * int and row_id > 0; JSON_BOOL is rejected. prefix_row_matches is injected as
 * exact deep JSON equality (the oracle passes before == after).
 */
#include "noc_w16_persistedt_fc545e.h"

#include <stdlib.h>
#include <string.h>

#define DB_MARKER "_db_persisted"

static int json_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0 ? 1 : 0;
    return 1;
}

/* Deep equality, order-independent for objects (Python dict ==). */
static int json_equal(const json_t *a, const json_t *b) {
    size_t i, n;
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        case JSON_ARRAY:
            n = a->c.count;
            if (n != b->c.count) return 0;
            for (i = 0; i < n; ++i)
                if (!json_equal(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        case JSON_OBJECT:
            n = a->c.count;
            if (n != b->c.count) return 0;
            for (i = 0; i < n; ++i) {
                const char *k = a->c.keys[i];
                if (!json_equal(a->c.items[i], json_obj_get(b, k))) return 0;
            }
            return 1;
    }
    return 0;
}

static int committed_row(const json_t *message, long long *out) {
    const json_t *rid, *marker;
    if (!message || message->type != JSON_OBJECT) return 0;
    marker = json_obj_get(message, DB_MARKER);
    if (!json_truthy(marker)) return 0;
    rid = json_obj_get(message, "_row_id");
    if (!rid || rid->type != JSON_NUMBER) return 0;     /* bool/float/str rejected (type is int) */
    if (rid->num_val != (double)(long long)rid->num_val) return 0;
    if (rid->num_val <= 0) return 0;
    *out = (long long)rid->num_val;
    return 1;
}

json_t *w16_persistedt_fc545e_committed_row_id(const json_t *message) {
    long long id;
    if (!committed_row(message, &id)) return json_null();
    return json_number((double)id);
}

static const char *msg_str(const json_t *m, const char *key) {
    const json_t *v = m && m->type == JSON_OBJECT ? json_obj_get(m, key) : NULL;
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

json_t *w16_persistedt_fc545e_receipt(const json_t *messages, const char *start,
                                      const json_t *history, const char *raw,
                                      const char *status, int compression_unchanged) {
    long long s;
    size_t n, hn, i;
    json_t *out = json_object();
    json_t *row_ids = json_array();
    json_t *user_row_ids = json_array();
    long long anchor = 0;
    int have_anchor = 0, have_final = 0, user_rows_added = 0;
    int unicode_users = 0, all_rows_tailed = 1, user_count = 0;
    long long final_id = 0;
    size_t tail_n;

    if (!messages || messages->type != JSON_ARRAY) { json_free(out); return json_null(); }
    n = messages->c.count;
    s = strtoll(start ? start : "0", NULL, 10);
    if (s < 0 || s >= (long long)n) { json_free(out); return json_null(); }
    {
        const json_t *first = json_get(messages, (size_t)s);
        const char *role = msg_str(first, "role");
        if (!first || first->type != JSON_OBJECT || !role || strcmp(role, "user")) {
            json_free(out); return json_null();
        }
    }
    tail_n = n - (size_t)s;
    have_anchor = committed_row(json_get(messages, (size_t)s), &anchor);

    hn = (history && history->type == JSON_ARRAY) ? history->c.count : 0;
    for (i = 0; i < hn; ++i) {
        const json_t *before = json_get(history, i);
        if (have_anchor) {
            long long bid;
            if (committed_row(before, &bid) && bid == anchor) { json_free(out); return json_null(); }
        }
    }

    for (i = (size_t)s; i < n; ++i) {
        long long rid;
        const json_t *m = json_get(messages, i);
        if (committed_row(m, &rid)) json_append(row_ids, json_number((double)rid));
        else all_rows_tailed = 0;
        {
            const char *role = msg_str(m, "role");
            if (role && !strcmp(role, "user")) user_count++;
        }
    }
    if (json_array_size(row_ids) == 0) { json_free(out); return json_null(); }
    json_set(out, "row_ids", row_ids);
    row_ids = NULL;
    json_set(out, "complete", json_bool(0));

    {
        long long uid;
        if (committed_row(json_get(messages, (size_t)s), &uid))
            json_set(out, "user_row_id", json_number((double)uid));
    }

    for (i = (size_t)s; i < n; ++i) {
        const json_t *m = json_get(messages, i);
        const char *role = msg_str(m, "role");
        const char *dk = msg_str(m, "display_kind");
        if (role && !strcmp(role, "user") && !(dk && !strcmp(dk, "hidden"))) {
            long long uid;
            if (committed_row(m, &uid)) json_append(user_row_ids, json_number((double)uid));
            else { json_free(user_row_ids); user_row_ids = NULL; break; }
            unicode_users++;
        }
    }
    if (unicode_users > 0 && user_row_ids) {
        json_set(out, "user_row_ids", user_row_ids);
        user_row_ids = NULL;
        user_rows_added = 1;
    }
    json_free(user_row_ids);

    {
        const json_t *last = json_get(messages, n - 1);
        const char *role = msg_str(last, "role");
        const char *content = msg_str(last, "content");
        const json_t *tool_calls = json_obj_get(last, "tool_calls");
        if (status && !strcmp(status, "complete") && role && !strcmp(role, "assistant")
                && !json_truthy(tool_calls) && content && raw && !strcmp(content, raw)
                && committed_row(last, &final_id)) {
            have_final = 1;
            json_set(out, "final_assistant_row_id", json_number((double)final_id));
        }
    }

    {
        int prefix_unchanged = 0;
        if ((long long)hn == s) {
            prefix_unchanged = 1;
            for (i = 0; i < hn; ++i) {
                if (!json_equal(json_get(history, i), json_get(messages, i))) { prefix_unchanged = 0; break; }
            }
        }
        {
            int complete = prefix_unchanged && compression_unchanged && all_rows_tailed
                           && user_count == 1 && have_final;
            json_set(out, "complete", json_bool(complete ? 1 : 0));
        }
    }
    (void)user_rows_added; (void)tail_n;
    return out;
}
