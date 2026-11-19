/*
 * relay_shared_metrics_flow.c — shared-metrics normalization/aggregation
 * FLOW cores of hermes_cli/observability/relay_shared_metrics.py (LIVE
 * authority).
 *
 * Ports decision sequences: guard chains, branch conditions, emission ORDER,
 * aggregation state machines and error paths. Every live effect (Relay scope
 * emission, the monotonic clock, the contract/fields/model-state callees)
 * enters through the seam structs declared in relay_shared_metrics_flow.h;
 * the cores below perform no I/O.
 *
 * Python semantics honored: `is True` identity gates (JSON true only, never
 * 1), `or` fallback chains, str() of scalars, set vs dict mutation order,
 * OrderedDict FIFO eviction, bool-as-int accumulation (+= True).
 *
 * The _event_matches_task_turn / _remember_turn / _existing_model_call_key
 * chains mirror the already-ported _Runtime helpers (relay_shared_metrics.c);
 * the oracle drives the LIVE defs, so any drift surfaces as a mismatch.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "relay_shared_metrics_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── internal helpers ── */

static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* `x is True` — JSON boolean true ONLY (1 / "true" are not True). */
static bool is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

static bool is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static void py_float_repr(double v, char *out, size_t cap) {
    for (int prec = 1; prec <= 17; prec++) {
        snprintf(out, cap, "%.*g", prec, v);
        if (strtod(out, NULL) == v) return;
    }
    snprintf(out, cap, "%.17g", v);
}

static char *jpystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && fabs(d) < 9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                py_float_repr(d, buf, sizeof(buf));
            return strdup(buf);
        }
        default: return json_serialize(v);
    }
}

/* _text(event, key): str(event.get(key) or ""). malloc'd. */
static char *text_field(const json_t *event, const char *key) {
    const json_t *v = json_obj_get(event, key);
    if (!jtruth(v)) return strdup("");
    return jpystr(v);
}

static json_t *jget(const json_t *obj, const char *key) {
    return json_obj_get(obj, key);
}

static const char *str_at(const json_t *arr, size_t i) {
    if (!arr || arr->type != JSON_ARRAY || i >= arr->c.count) return "";
    const json_t *v = arr->c.items[i];
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

static bool str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

static bool arr_has_str(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *v = arr->c.items[i];
        if (v && v->type == JSON_STRING && str_eq(v->str_val, s)) return true;
    }
    return false;
}

/* Remove array element i in place (libjson has no array-remove API). */
static void arr_remove(json_t *arr, size_t i) {
    if (!arr || arr->type != JSON_ARRAY || i >= arr->c.count) return;
    json_free(arr->c.items[i]);
    memmove(&arr->c.items[i], &arr->c.items[i + 1],
            (arr->c.count - i - 1) * sizeof(json_t *));
    arr->c.count--;
}

static double num_field(const json_t *obj, const char *key, double dflt) {
    const json_t *v = jget(obj, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : dflt;
}

/* ── trace recorder ── */
void rsm_flow_trace_add(rsm_flow_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

void rsm_flow_trace_free(rsm_flow_trace_t *t) {
    if (!t) return;
    json_free(t->events);
    t->events = NULL;
}

/* ── 1. _terminal_flags ── */
/* PoP: rsm_flow_terminal_flags @ hermes_cli/observability/relay_shared_metrics.py:_terminal_flags */
json_t *rsm_flow_terminal_flags(const json_t *result, const json_t *error) {
    json_t *out = json_object();
    if (!is_none(error)) {
        const char *name = json_get_str(error, "name", "");
        /* isinstance(error, (KeyboardInterrupt, InterruptedError)) or
         * type(error).__name__ == "CancelledError" — fixtures describe
         * builtin exceptions (or asyncio.CancelledError) by class name. */
        bool interrupted = str_eq(name, "KeyboardInterrupt") ||
                           str_eq(name, "InterruptedError") ||
                           str_eq(name, "CancelledError");
        const char *reason = interrupted ? "interrupted_by_user"
                             : (str_eq(name, "TimeoutError") ? "timed_out"
                                                             : "system_aborted");
        json_set(out, "completed", json_bool(false));
        json_set(out, "failed", json_bool(!interrupted));
        json_set(out, "interrupted", json_bool(interrupted));
        json_set(out, "turn_exit_reason", json_string(reason));
        json_set(out, "failure_class", json_string("exception"));
        return out;
    }
    const json_t *terminal = (result && result->type == JSON_OBJECT)
                                 ? result : NULL;
    bool failed = is_true(terminal ? jget(terminal, "failed") : NULL);
    /* str(terminal.get("turn_exit_reason") or terminal.get("failure_reason") or "") */
    const json_t *rv = terminal ? jget(terminal, "turn_exit_reason") : NULL;
    if (!jtruth(rv)) rv = terminal ? jget(terminal, "failure_reason") : NULL;
    char *reason = jtruth(rv) ? jpystr(rv) : strdup("");
    bool completed = is_true(terminal ? jget(terminal, "completed") : NULL);
    bool interrupted = is_true(terminal ? jget(terminal, "interrupted") : NULL);
    const char *exit_reason = reason[0] ? reason : (failed ? "failed" : "unknown");
    char *failure_reason = jtruth(terminal ? jget(terminal, "failure_reason") : NULL)
                               ? jpystr(jget(terminal, "failure_reason"))
                               : strdup("");
    json_set(out, "completed", json_bool(completed));
    json_set(out, "failed", json_bool(failed));
    json_set(out, "interrupted", json_bool(interrupted));
    json_set(out, "turn_exit_reason", json_string(exit_reason));
    json_set(out, "failure_reason", json_string(failure_reason));
    free(reason);
    free(failure_reason);
    return out;
}

/* ── 2. _session_pair ── */
/* PoP: rsm_flow_session_pair @ hermes_cli/observability/relay_shared_metrics.py:_session_pair */
json_t *rsm_flow_session_pair(const json_t *event, const char *key) {
    char *sid = text_field(event, "session_id");
    char *val = text_field(event, key);
    json_t *out = NULL;
    if (sid[0] && val[0]) {
        out = json_array();
        json_append(out, json_string(sid));
        json_append(out, json_string(val));
    }
    free(sid);
    free(val);
    return out;
}

/* ── 3. _identities_compatible ── */
static bool ident_compat3(const char *cand[3], const char *obs[3]) {
    if (!obs[2][0] || !str_eq(cand[2], obs[2])) return false;
    for (size_t i = 0; i < 2; i++) {
        /* not left or not right or left == right */
        if (cand[i][0] && obs[i][0] && !str_eq(cand[i], obs[i])) return false;
    }
    return true;
}

/* PoP: rsm_flow_identities_compatible @ hermes_cli/observability/relay_shared_metrics.py:_identities_compatible */
bool rsm_flow_identities_compatible(const json_t *candidate, const json_t *observed) {
    const char *cand[3] = {str_at(candidate, 0), str_at(candidate, 1), str_at(candidate, 2)};
    const char *obs[3] = {str_at(observed, 0), str_at(observed, 1), str_at(observed, 2)};
    return ident_compat3(cand, obs);
}

/* ── 4. _compatible_tool_call_keys ── */
/* PoP: rsm_flow_compatible_tool_call_keys @ hermes_cli/observability/relay_shared_metrics.py:_compatible_tool_call_keys */
json_t *rsm_flow_compatible_tool_call_keys(const json_t *tool_call_keys,
                                           const char *task_id,
                                           const json_t *identity) {
    const char *obs[3] = {str_at(identity, 0), str_at(identity, 1), str_at(identity, 2)};
    json_t *out = json_array();
    if (!tool_call_keys || tool_call_keys->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < tool_call_keys->c.count; i++) {
        const json_t *key = tool_call_keys->c.items[i];
        if (!key || key->type != JSON_ARRAY || key->c.count < 4) continue;
        if (!str_eq(str_at(key, 0), task_id)) continue;
        const char *cand[3] = {str_at(key, 1), str_at(key, 2), str_at(key, 3)};
        if (ident_compat3(cand, obs))
            json_append(out, json_copy(key));
    }
    return out;
}

/* ── 5. _absorb_peak ── */
/* PoP: rsm_flow_absorb_peak @ hermes_cli/observability/relay_shared_metrics.py:_absorb_peak */
void rsm_flow_absorb_peak(json_t *into, const json_t *segment) {
    if (!into || !segment) return;
    const json_t *seg_route = jget(segment, "peak_route");
    if (is_none(seg_route)) return;
    bool limit_hit = jtruth(jget(into, "limit_hit")) || jtruth(jget(segment, "limit_hit"));
    json_set(into, "limit_hit", json_bool(limit_hit));
    const json_t *seg_window = jget(segment, "peak_window");
    const json_t *into_window = jget(into, "peak_window");
    double seg_tokens = num_field(segment, "peak_tokens", 0);
    double into_tokens = num_field(into, "peak_tokens", 0);
    bool fuller = !is_none(seg_window) &&
                  (is_none(into_window) ||
                   seg_tokens / seg_window->num_val > into_tokens / into_window->num_val);
    if (is_none(jget(into, "peak_route")) || fuller) {
        json_set(into, "peak_route", json_copy(seg_route));
        json_set(into, "peak_tokens", json_number(seg_tokens));
        json_set(into, "peak_window", seg_window ? json_copy(seg_window) : json_null());
    }
}

/* ── 6. _admits (mirrors _event_matches_task_turn + _remember_turn) ── */
/* PoP: rsm_flow_admits @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._admits */
bool rsm_flow_admits(json_t *state, json_t *session, json_t *task,
                     const json_t *event, bool current) {
    char *turn_id = text_field(event, "turn_id");
    char *task_id = text_field(task, "task_id");
    char *sid = text_field(session, "session_id");
    bool admitted = false;
    /* closing or not matches or (current and stale) — short-circuit order. */
    if (jtruth(jget(session, "closing"))) goto done;
    if (turn_id[0]) {
        if (arr_has_str(jget(task, "retired_turn_ids"), turn_id)) goto done;
        const json_t *turn_ids = jget(task, "turn_ids");
        if (turn_ids && turn_ids->type == JSON_ARRAY && turn_ids->c.count > 0 &&
            !arr_has_str(turn_ids, turn_id)) goto done;
    }
    if (current) {
        const json_t *tasks = jget(session, "tasks");
        const json_t *live_task = tasks ? jget(tasks, task_id) : NULL;
        /* `is not task` — ref markers model object identity. */
        const char *live_ref = live_task ? json_get_str(live_task, "ref", "") : "";
        const char *task_ref = json_get_str(task, "ref", "");
        if (!live_task || !str_eq(live_ref, task_ref)) goto done;
    }
    /* _remember_turn: admitted events have their turn remembered. */
    if (turn_id[0]) {
        json_t *turn_ids = jget(task, "turn_ids");
        if (!turn_ids || turn_ids->type != JSON_ARRAY) {
            turn_ids = json_array();
            json_set(task, "turn_ids", turn_ids);
        }
        if (!arr_has_str(turn_ids, turn_id))
            json_append(turn_ids, json_string(turn_id));
        json_t *ts = jget(state, "turn_sessions");
        if (!ts || ts->type != JSON_OBJECT) {
            ts = json_object();
            json_set(state, "turn_sessions", ts);
        }
        size_t cap = strlen(sid) + strlen(turn_id) + 2;
        char *key = malloc(cap);
        if (key) {
            snprintf(key, cap, "%s\t%s", sid, turn_id);
            json_set(ts, key, json_string(sid));
            free(key);
        }
    }
    admitted = true;
done:
    free(turn_id);
    free(task_id);
    free(sid);
    return admitted;
}

/* ── 7. _announce_cold ── */
/* PoP: rsm_flow_announce_cold @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._announce_cold */
bool rsm_flow_announce_cold(json_t *expected_keys, const char *key) {
    if (!expected_keys || expected_keys->type != JSON_ARRAY) return false;
    if (arr_has_str(expected_keys, key)) return false;
    json_append(expected_keys, json_string(key));
    while (expected_keys->c.count > 256)
        arr_remove(expected_keys, 0);
    return true;
}

/* ── 8. _consume_cold ── */
/* PoP: rsm_flow_consume_cold @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._consume_cold */
bool rsm_flow_consume_cold(json_t *expected_keys, const char *key) {
    if (!expected_keys || expected_keys->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < expected_keys->c.count; i++) {
        const json_t *v = expected_keys->c.items[i];
        if (v && v->type == JSON_STRING && str_eq(v->str_val, key)) {
            /* pop(key, False) is None — every stored value is None live. */
            arr_remove(expected_keys, i);
            return true;
        }
    }
    return false;
}

/* ── 9. _count_session_turn ── */
/* PoP: rsm_flow_count_session_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._count_session_turn */
void rsm_flow_count_session_turn(json_t *session, const json_t *task,
                                 const char *outcome, const rsm_clock_seams_t *s) {
    if (!session || !task) return;
    if (is_none(jget(session, "start_fields"))) {
        const json_t *sf = jget(task, "start_fields");
        json_set(session, "start_fields",
                 (sf && sf->type == JSON_OBJECT) ? json_copy(sf) : json_object());
        json_set(session, "first_turn_ns", json_number(num_field(task, "started_ns", 0)));
    }
    json_set(session, "turns", json_number(num_field(session, "turns", 0) + 1));
    json_set(session, "failed_turns",
             json_number(num_field(session, "failed_turns", 0) +
                         (str_eq(outcome, "failed") ? 1 : 0)));
    const json_t *mc = jget(task, "model_call_ids");
    const json_t *tc = jget(task, "tool_call_ids");
    double mc_n = (mc && mc->type == JSON_ARRAY) ? (double)mc->c.count : 0;
    double tc_n = (tc && tc->type == JSON_ARRAY) ? (double)tc->c.count : 0;
    json_set(session, "api_calls", json_number(num_field(session, "api_calls", 0) + mc_n));
    json_set(session, "tool_call_total",
             json_number(num_field(session, "tool_call_total", 0) + tc_n +
                         num_field(task, "unidentified_tool_calls", 0)));
    json_set(session, "last_outcome", json_string(outcome ? outcome : ""));
    long long now = (s && s->monotonic_ns) ? s->monotonic_ns(s->ctx) : 0;
    json_set(session, "last_turn_ns", json_number((double)now));
}

/* ── 10. update_model_call ── */

/* _existing_model_call_key mirror: (task_id, request_id) of an open call; a
 * task-less event may match by request alone when UNIQUE. Returns malloc'd
 * "task\trequest" key or NULL. */
static char *existing_model_call_key(const json_t *session, const json_t *event) {
    char *request_id = text_field(event, "api_request_id");
    if (!request_id[0]) { free(request_id); return NULL; }
    char *task_id = text_field(event, "task_id");
    const json_t *calls = jget(session, "model_calls");
    size_t cap = strlen(task_id) + strlen(request_id) + 2;
    char *key = malloc(cap);
    if (!key) { free(request_id); free(task_id); return NULL; }
    snprintf(key, cap, "%s\t%s", task_id, request_id);
    if ((calls && json_obj_get(calls, key)) || task_id[0]) {
        bool present = calls && json_obj_get(calls, key);
        free(request_id);
        free(task_id);
        if (present) return key;
        free(key);
        return NULL;
    }
    /* candidates by request alone */
    char *found = NULL;
    size_t n = 0;
    if (calls && calls->type == JSON_OBJECT) {
        for (size_t i = 0; i < calls->c.count; i++) {
            const char *k = calls->c.keys[i];
            const char *tab = strchr(k, '\t');
            if (tab && str_eq(tab + 1, request_id)) {
                n++;
                if (n == 1) found = strdup(k);
            }
        }
    }
    free(key);
    free(request_id);
    free(task_id);
    if (n == 1) return found;
    free(found);
    return NULL;
}

/* PoP: rsm_flow_update_model_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.update_model_call */
void rsm_flow_update_model_call(json_t *session, const json_t *event, bool finish,
                                const rsm_update_seams_t *s) {
    if (!session || !s) return;                 /* _any_session -> None */
    if (jtruth(jget(session, "closing"))) return;
    char *key = existing_model_call_key(session, event);
    if (!key) return;
    json_t *calls = jget(session, "model_calls");
    json_t *model_call = calls ? json_obj_get(calls, key) : NULL;
    if (!model_call || model_call->type != JSON_OBJECT) { free(key); return; }
    const json_t *fields = s->model_call_fields ? s->model_call_fields(s->ctx, event) : NULL;
    json_set(model_call, "fields", fields ? json_copy(fields) : json_object());
    const json_t *mc_fields = jget(model_call, "fields");
    if (finish) {
        const char *mc_task_id_src = json_get_str(model_call, "task_id", "");
        char *mc_task_id = strdup(mc_task_id_src ? mc_task_id_src : "");
        const json_t *tasks = jget(session, "tasks");
        const json_t *task = tasks ? jget(tasks, mc_task_id) : NULL;
        const json_t *cost = task ? jget(task, "cost") : NULL;
        bool user_turn = cost && jtruth(jget(cost, "user_turn"));
        /* session.replies += task is not None and task.cost.user_turn */
        json_set(session, "replies",
                 json_number(num_field(session, "replies", 0) + (user_turn ? 1 : 0)));
        if (s->observe_call)
            s->observe_call(s->ctx, mc_fields, jget(event, "usage"),
                            jget(event, "context_length"));
        if (s->observe_turn_call)
            s->observe_turn_call(s->ctx, model_call, event);
        const char *ttft = s->ttft_bucket ? s->ttft_bucket(s->ctx, event) : "unknown";
        json_set(model_call, "ttft_bucket", json_string(ttft ? ttft : "unknown"));
        /* Live keeps the popped _ModelCall alive via its local reference;
         * snapshot fields BEFORE the seam pops (and frees) the map entry. */
        json_t *held_fields = json_copy(mc_fields);
        if (s->finish_model_call)
            s->finish_model_call(s->ctx, session, key, "success");
        const json_t *tokens = s->model_token_fields
                                   ? s->model_token_fields(s->ctx, jget(event, "usage"), held_fields)
                                   : NULL;
        if (tokens) {
            /* _guarded: a raising mark logs WARNING and is swallowed. */
            const json_t *task2 = tasks ? jget(tasks, mc_task_id) : NULL;
            bool raised = false;
            if (s->mark)
                s->mark(s->ctx, task2, "hermes.model_tokens", tokens, &raised);
            if (raised && s->log_warning)
                s->log_warning(s->ctx, "Hermes shared-metrics token mark failed");
        }
        json_free(held_fields);
        free(mc_task_id);
    } else {
        const char *ec = s->model_error_class ? s->model_error_class(s->ctx, event) : "none";
        json_set(model_call, "error_class", json_string(ec ? ec : "none"));
        if (s->observe_error)
            s->observe_error(s->ctx, mc_fields, json_get_str(model_call, "error_class", "none"));
    }
    free(key);
}