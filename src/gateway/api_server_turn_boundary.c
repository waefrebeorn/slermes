/*
 * api_server_turn_boundary.c -- C home for gateway/platforms/api_server_turn_boundary.py.
 *
 * Real C11 port of the two pure helpers that decide where this turn starts inside
 * the transcript an OpenAI-compat route gets back from run_conversation:
 *   _same_transcript_prefix  -- prefix equality by what each message says
 *   response_turn_start_index -- the 0-based index this turn's rows begin at
 *
 * Python `==` on arbitrary JSON values is reproduced by tb_py_eq (bool/number
 * cross-equality, key-set object compare). The one live dependency,
 * agent.turn_context.reanchor_current_turn_user_idx, is a seam: the caller
 * injects its answer (reanchor_idx, -1 = "not found"), exactly like the oracle
 * stubs it.
 *
 * PoP: w19_turnbound_4998c3_same_prefix @ gateway/platforms/api_server_turn_boundary.py:_same_transcript_prefix
 * PoP: w19_turnbound_4998c3_start_index @ gateway/platforms/api_server_turn_boundary.py:response_turn_start_index
 */
#include "noc_w19_turnbound_4998c3.h"

#include <string.h>

static const char *const IDENTITY_KEYS[] = {"role", "content", "tool_calls", "tool_call_id"};

/* Python equality for the JSON value domain: True == 1, 1.0 == 1, key sets
 * compared unordered, arrays element-wise, str/bool/number compared by value. */
/* A missing dict key reads as Python None, which a JSON null also is. */
static int tb_py_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static int tb_py_eq(const json_t *a, const json_t *b) {
    if (tb_py_null(a) || tb_py_null(b)) return tb_py_null(a) && tb_py_null(b);
    int an = (a->type == JSON_BOOL || a->type == JSON_NUMBER);
    int bn = (b->type == JSON_BOOL || b->type == JSON_NUMBER);
    if (an && bn) {
        double av = (a->type == JSON_BOOL) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = (b->type == JSON_BOOL) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return (a->str_val && b->str_val) ? strcmp(a->str_val, b->str_val) == 0
                                         : a->str_val == b->str_val;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        size_t n = json_len(a);
        if (n != json_len(b)) return 0;
        for (size_t i = 0; i < n; ++i) {
            if (!tb_py_eq(json_get(a, i), json_get(b, i))) return 0;
        }
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        size_t n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (size_t i = 0; i < n; ++i) {
            const char *k = json_object_get_key_at(a, i);
            if (!k) return 0;
            const json_t *bv = json_obj_get(b, k);
            if (!bv) return 0;  /* Python dicts: a missing key never equals a present one */
            if (!tb_py_eq(json_object_get_at(a, i), bv)) return 0;
        }
        return 1;
    }
    return 0;
}

/* Same, but with dict.get() semantics on both sides: a missing key reads None. */
static int tb_field_eq(const json_t *got, const json_t *want, const char *key) {
    const json_t *g = (got && got->type == JSON_OBJECT) ? json_obj_get(got, key) : NULL;
    const json_t *w = (want && want->type == JSON_OBJECT) ? json_obj_get(want, key) : NULL;
    return tb_py_eq(g, w);
}

json_t *w19_turnbound_4998c3_same_prefix(json_t *agent_messages, json_t *prefix) {
    if (!agent_messages || agent_messages->type != JSON_ARRAY ||
        !prefix || prefix->type != JSON_ARRAY)
        return json_bool(0);
    size_t na = json_len(agent_messages), np = json_len(prefix);
    if (na < np) return json_bool(0);
    for (size_t i = 0; i < np; ++i) {
        const json_t *got = json_get(agent_messages, i);
        const json_t *want = json_get(prefix, i);
        int gd = (got && got->type == JSON_OBJECT), wd = (want && want->type == JSON_OBJECT);
        if (!gd || !wd) {
            if (!tb_py_eq(got, want)) return json_bool(0);
            continue;
        }
        for (size_t k = 0; k < sizeof(IDENTITY_KEYS) / sizeof(IDENTITY_KEYS[0]); ++k) {
            if (!tb_field_eq(got, want, IDENTITY_KEYS[k])) return json_bool(0);
        }
    }
    return json_bool(1);
}

/* `prior + [{"role": "user", "content": user_message}]` -- the expected prefix. */
static json_t *tb_expected_prefix(json_t *prior, json_t *user_message) {
    json_t *out = json_array();
    size_t n = (prior && prior->type == JSON_ARRAY) ? json_len(prior) : 0;
    for (size_t i = 0; i < n; ++i) json_array_append(out, json_copy(json_get(prior, i)));
    json_t *u = json_object();
    json_set(u, "role", json_string("user"));
    json_set(u, "content", json_copy(user_message ? user_message : json_null()));
    json_array_append(out, u);
    return out;
}

json_t *w19_turnbound_4998c3_start_index(json_t *conversation_history,
                                        json_t *user_message, json_t *result,
                                        int reanchor_idx) {
    json_t *agent_messages = (result && result->type == JSON_OBJECT)
                                 ? json_obj_get(result, "messages") : NULL;
    if (!agent_messages || agent_messages->type != JSON_ARRAY || json_len(agent_messages) == 0)
        return json_number(0);
    const json_t *suffix = json_obj_get(result, "_messages_are_turn_suffix");
    if (suffix && ((suffix->type == JSON_BOOL && suffix->bool_val) ||
                   (suffix->type == JSON_NUMBER && suffix->num_val != 0.0) ||
                   (suffix->type == JSON_STRING && suffix->str_val && suffix->str_val[0])))
        return json_number(0);
    if (reanchor_idx >= 0) return json_number((double)(reanchor_idx + 1));

    json_t *prior = (conversation_history && conversation_history->type == JSON_ARRAY)
                        ? conversation_history : json_array();
    json_t *expected = tb_expected_prefix(prior, user_message);
    json_t *r_expected = w19_turnbound_4998c3_same_prefix(agent_messages, expected);
    int same = r_expected && r_expected->type == JSON_BOOL && r_expected->bool_val;
    json_free(r_expected);
    double answer = 0.0;
    if (same) {
        answer = (double)json_len(expected);
    } else if (json_len(prior) > 0) {
        json_t *r_prior = w19_turnbound_4998c3_same_prefix(agent_messages, prior);
        int same_prior = r_prior && r_prior->type == JSON_BOOL && r_prior->bool_val;
        json_free(r_prior);
        if (same_prior) answer = (double)json_len(prior);
    }
    json_free(expected);
    return json_number(answer);
}
