/*
 * cli_single_query.c — C port of upstream Hermes hermes_cli/cli_single_query.py.
 * Gate: w15_clisingle_3677f3
 *
 * PoP: w15_clisingle_3677f3_int_or @ hermes_cli/cli_single_query.py:_int_or
 * PoP: w15_clisingle_3677f3_sync_session @ hermes_cli/cli_single_query.py:_sync_cli_session_id_from_agent
 * PoP: w15_clisingle_3677f3_configure_quiet @ hermes_cli/cli_single_query.py:_configure_quiet_agent
 *
 * The three ported helpers are the side-effect-free core of the single-query
 * driver; agent/subprocess orchestration in the live module is out of scope.
 */
#include "noc_w15_clisingle_3677f3.h"
#include "hermes_json.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python truthiness for a JSON value. */
static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default:          return 0;
    }
}

/* Python `a != b` for the value shapes our callers can see. */
static int py_equal(const json_t *a, const json_t *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
        return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
    if (a->type == JSON_BOOL && b->type == JSON_BOOL)
        return a->bool_val == b->bool_val;
    if (a->type == JSON_NULL && b->type == JSON_NULL) return 1;
    char *sa = json_dumps(a, 0), *sb = json_dumps(b, 0);
    int eq = sa && sb && !strcmp(sa, sb);
    free(sa); free(sb);
    return eq;
}

/* int(str) in base 10: leading/trailing ASCII whitespace, sign, and digit
 * separators. Out-of-int64-range is a parse failure for the harness domain. */
static int py_str_to_ll(const char *s, long long *out) {
    if (!s) return 0;
    while (*s && isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (!isdigit((unsigned char)*s)) return 0;
    long long val = 0;
    int prev_us = 0;
    for (; *s; s++) {
        if (*s == '_') {
            if (prev_us) return 0;
            prev_us = 1;
            continue;
        }
        if (!isdigit((unsigned char)*s)) break;
        int d = *s - '0';
        if (val > (LLONG_MAX - d) / 10) return 0;
        val = val * 10 + d;
        prev_us = 0;
    }
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s || prev_us) return 0;
    *out = neg ? -val : val;
    return 1;
}

json_t *w15_clisingle_3677f3_int_or(const json_t *value, const json_t *dflt) {
    long long n = 0;
    if (value) {
        if (value->type == JSON_BOOL)
            return json_number(value->bool_val ? 1.0 : 0.0);
        if (value->type == JSON_NUMBER)
            return json_number((double)(long long)value->num_val);
        if (value->type == JSON_STRING && py_str_to_ll(value->str_val, &n))
            return json_number((double)n);
    }
    return dflt ? json_copy(dflt) : json_null();
}

json_t *w15_clisingle_3677f3_sync_session(const json_t *cli) {
    const json_t *session = json_obj_get(cli, "session_id");
    json_t *agent = json_obj_get(cli, "agent");
    const json_t *agent_sid = json_is_object(agent) ? json_obj_get(agent, "session_id") : NULL;
    json_t *out = json_object();
    if (json_truthy(agent_sid) && !py_equal(agent_sid, session))
        json_set(out, "session_id", json_copy(agent_sid));
    else
        json_set(out, "session_id", session ? json_copy(session) : json_null());
    return out;
}

json_t *w15_clisingle_3677f3_configure_quiet(const json_t *agent) {
    json_t *out = json_is_object(agent) ? json_copy(agent) : json_object();
    json_set(out, "quiet_mode", json_bool(1));
    json_set(out, "suppress_status_output", json_bool(1));
    json_set(out, "stream_delta_callback", json_null());
    json_set(out, "tool_gen_callback", json_null());
    json_set(out, "reasoning_callback", json_null());
    json_set(out, "tool_progress_callback", json_null());
    json_set(out, "tool_start_callback", json_null());
    json_set(out, "tool_complete_callback", json_null());
    json_set(out, "tool_progress_mode", json_string("off"));
    return out;
}
