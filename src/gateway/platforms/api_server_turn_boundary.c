/*
 * api_server_turn_boundary.c -- real C11 port of the current-turn boundary the
 * OpenAI-compat routes cut out of a returned transcript, upstream
 * gateway/platforms/api_server_turn_boundary.py.
 *
 * PoP: w21_apiservert_4998c3_same_prefix @ gateway/platforms/api_server_turn_boundary.py:_same_transcript_prefix
 * PoP: w21_apiservert_4998c3_start_index @ gateway/platforms/api_server_turn_boundary.py:response_turn_start_index
 * PoP: w21_apiservert_4998c3_reanchor @ agent/turn_context.py:reanchor_current_turn_user_idx
 *
 * Python `==` over the JSON value domain is reproduced by tb_py_eq: True == 1
 * (a bool is an int), numbers compare by value, objects compare as unordered
 * key sets, and a missing dict key reads as None.
 *
 * The session-rules projection (`user_originated_turn_view`) is a seam: the
 * caller hands the per-row views in, and the ranking walk over them is ported.
 */
#include "hermes_json.h"
#include "noc_w21_apiservert_4998c3.h"

#include <math.h>
#include <string.h>

static const char *const AB_IDENTITY_KEYS[] = {"role", "content", "tool_calls", "tool_call_id"};
#define AB_NIDENTITY ((int)(sizeof(AB_IDENTITY_KEYS) / sizeof(AB_IDENTITY_KEYS[0])))

static int ab_py_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* Python `==`. */
static int ab_py_eq(const json_t *a, const json_t *b) {
    if (ab_py_null(a) || ab_py_null(b)) return ab_py_null(a) && ab_py_null(b);
    {
        int an = (a->type == JSON_BOOL || a->type == JSON_NUMBER);
        int bn = (b->type == JSON_BOOL || b->type == JSON_NUMBER);
        if (an && bn) {
            double av = (a->type == JSON_BOOL) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
            double bv = (b->type == JSON_BOOL) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
            return av == bv;
        }
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING) {
        if (!a->str_val || !b->str_val) return a->str_val == b->str_val;
        return strcmp(a->str_val, b->str_val) == 0;
    }
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        size_t n = json_len(a), i;
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++) {
            if (!ab_py_eq(json_get(a, i), json_get(b, i))) return 0;
        }
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        size_t n = json_object_size(a), i;
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            const json_t *bv;
            if (!k) return 0;
            bv = json_obj_get(b, k);
            if (!bv) return 0;   /* a missing key never equals a present one */
            if (!ab_py_eq(json_object_get_at(a, i), bv)) return 0;
        }
        return 1;
    }
    return 0;
}

/* dict.get(key) on both sides: absent reads as None. */
static int ab_field_eq(const json_t *got, const json_t *want, const char *key) {
    const json_t *g = (got && got->type == JSON_OBJECT) ? json_obj_get(got, key) : NULL;
    const json_t *w = (want && want->type == JSON_OBJECT) ? json_obj_get(want, key) : NULL;
    return ab_py_eq(g, w);
}

/* Python truthiness of a decoded value. */
static int ab_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* Python iteration over the shapes a transcript can be: a list, a str (its
 * characters) or a dict (its keys). Elements are materialised without
 * allocation so plain `==` can be applied to them. */
typedef struct { json_t node; char cbuf[2]; } ab_elem;

static size_t ab_seq_len(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_ARRAY) return json_len(v);
    if (v->type == JSON_STRING) return v->str_val ? strlen(v->str_val) : 0;
    if (v->type == JSON_OBJECT) return json_object_size(v);
    return 0;
}

static void ab_seq_elem(const json_t *v, size_t i, ab_elem *out) {
    if (v && v->type == JSON_ARRAY) {
        const json_t *it = json_get(v, i);
        if (it) { out->node = *it; return; }
        out->node.type = JSON_NULL;
        out->node.str_val = NULL;
        return;
    }
    out->node.type = JSON_STRING;
    if (v && v->type == JSON_STRING && v->str_val) {
        out->cbuf[0] = v->str_val[i];
        out->cbuf[1] = '\0';
        out->node.str_val = out->cbuf;
        return;
    }
    if (v && v->type == JSON_OBJECT) {
        out->node.str_val = (char *)json_object_get_key_at(v, i);
        return;
    }
    out->node.type = JSON_NULL;
    out->node.str_val = NULL;
}

json_t *w21_apiservert_4998c3_same_prefix(const json_t *agent_messages,
                                          const json_t *prefix) {
    size_t na, np, i;
    if (!agent_messages || !prefix) return json_bool(0);
    na = ab_seq_len(agent_messages);
    np = ab_seq_len(prefix);
    if (na < np) return json_bool(0);
    for (i = 0; i < np; i++) {
        ab_elem got, want;
        int gd, wd;
        ab_seq_elem(agent_messages, i, &got);
        ab_seq_elem(prefix, i, &want);
        gd = (got.node.type == JSON_OBJECT);
        wd = (want.node.type == JSON_OBJECT);
        if (!gd || !wd) {
            if (!ab_py_eq(&got.node, &want.node)) return json_bool(0);
            continue;
        }
        for (int k = 0; k < AB_NIDENTITY; k++) {
            if (!ab_field_eq(&got.node, &want.node, AB_IDENTITY_KEYS[k])) return json_bool(0);
        }
    }
    return json_bool(1);
}

json_t *w21_apiservert_4998c3_reanchor(const json_t *agent_messages,
                                       const json_t *user_message,
                                       const json_t *live_views) {
    size_t n, i;
    long fallback = -1;
    if (!agent_messages || agent_messages->type != JSON_ARRAY) return json_number(-1);
    n = json_len(agent_messages);
    for (i = n; i > 0; i--) {
        const json_t *msg = json_get(agent_messages, i - 1);
        const json_t *view;
        if (!msg || msg->type != JSON_OBJECT) continue;
        {
            const json_t *role = json_obj_get(msg, "role");
            if (!role || role->type != JSON_STRING || !role->str_val ||
                strcmp(role->str_val, "user") != 0)
                continue;
        }
        if (ab_py_eq(json_obj_get(msg, "content"), user_message)) return json_number((double)(i - 1));
        view = (live_views && live_views->type == JSON_ARRAY && (i - 1) < json_len(live_views))
                   ? json_get(live_views, i - 1) : NULL;
        if (ab_py_null(view)) continue;
        if (ab_field_eq(view, user_message, "content")) return json_number((double)(i - 1));
        if (fallback < 0) fallback = (long)(i - 1);
    }
    return json_number((double)fallback);
}

json_t *w21_apiservert_4998c3_start_index(const json_t *conversation_history,
                                          const json_t *user_message,
                                          const json_t *result,
                                          const json_t *live_views) {
    const json_t *messages = (result && result->type == JSON_OBJECT)
                                 ? json_obj_get(result, "messages") : NULL;
    size_t prior_n;
    long idx;

    if (!messages || messages->type != JSON_ARRAY || json_len(messages) == 0)
        return json_number(0);
    if (result && result->type == JSON_OBJECT &&
        ab_truthy(json_obj_get(result, "_messages_are_turn_suffix")))
        return json_number(0);

    {
        json_t *anchor = w21_apiservert_4998c3_reanchor(messages, user_message, live_views);
        idx = (long)anchor->num_val;
        json_free(anchor);
    }
    if (idx >= 0) return json_number((double)(idx + 1));

    prior_n = (conversation_history && conversation_history->type == JSON_ARRAY)
                  ? json_len(conversation_history) : 0;
    {
        /* expected_prefix = prior + [{"role": "user", "content": user_message}] */
        json_t *expected = json_array();
        size_t i;
        int same;
        for (i = 0; i < prior_n; i++) json_append(expected, json_copy(json_get(conversation_history, i)));
        {
            json_t *u = json_object();
            json_set(u, "role", json_string("user"));
            json_set(u, "content", user_message ? json_copy(user_message) : json_null());
            json_append(expected, u);
        }
        same = ab_truthy(w21_apiservert_4998c3_same_prefix(messages, expected));
        if (same) {
            double answer = (double)json_len(expected);
            json_free(expected);
            return json_number(answer);
        }
        json_free(expected);
    }
    if (prior_n > 0 &&
        ab_truthy(w21_apiservert_4998c3_same_prefix(messages, conversation_history)))
        return json_number((double)prior_n);
    return json_number(0);
}
