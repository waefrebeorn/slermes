/*
 * api_server_fire_startup.c - C11 port of upstream
 * gateway/platforms/api_server_fire_startup.py (gate w25_apiserverf_ce83ea).
 *
 * PoP: w25_apiserverf_ce83ea_is_draining @ gateway/platforms/api_server_fire_startup.py:runner_is_draining
 * PoP: w25_apiserverf_ce83ea_startup_gate @ gateway/platforms/api_server_fire_startup.py:refuse_until_started
 * PoP: w25_apiserverf_ce83ea_live_adapters @ gateway/platforms/api_server_fire_startup.py:live_adapters_once_started
 * PoP: w25_apiserverf_ce83ea_constants @ gateway/platforms/api_server_fire_startup.py:<module constants>
 *
 * The aiohttp handler, the gateway runner object, the event loop clock and the
 * 0.1s poll sleep are seams: they arrive as fixed inputs (the runner's flag
 * values, the loop time when the handler was entered, the first sampled time).
 * What is ported is the decision the loop makes -- allow the fire, refuse it
 * with the dashboard's retryable 503, or keep waiting -- including the
 * re-check of the drain flag after every poll and the budget counting the time
 * already spent verifying the token.
 */
#include <stdio.h>
#include <string.h>
#include "noc_w25_apiserverf_ce83ea.h"

static int as_truthy(const json_t *v) {
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

static double as_num(const json_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    return def;
}

/* bool(getattr(runner, "_draining", False) or getattr(runner, "_external_drain_active", False)) */
json_t *w25_apiserverf_ce83ea_is_draining(const json_t *state) {
    const json_t *a, *b;
    if (!state || state->type != JSON_OBJECT) return json_bool(false);
    a = json_obj_get(state, "draining");
    b = json_obj_get(state, "external_drain");
    return json_bool(as_truthy(a) || as_truthy(b) ? true : false);
}

static json_t *refusal_body(const char *job_id, size_t polls) {
    json_t *out = json_object();
    json_t *body = json_object();
    json_set(body, "error", json_string(W25_APISERVERF_CE83EA_ERROR_TEXT));
    json_set(body, "job_id", job_id ? json_string(job_id) : json_null());
    json_set(out, "result", json_string("refuse"));
    json_set(out, "polls", json_number((double)polls));
    json_set(out, "status", json_number(503.0));
    json_set(out, "retry_after", json_string("60"));
    json_set(out, "body", body);
    return out;
}

static json_t *allow_result(size_t polls) {
    json_t *out = json_object();
    json_set(out, "result", json_string("allow"));
    json_set(out, "polls", json_number((double)polls));
    return out;
}

json_t *w25_apiserverf_ce83ea_startup_gate(const json_t *state) {
    const json_t *has_runner, *draining_j, *external_j, *running_j;
    const char *job_id;
    double received_at, now, deadline;
    int draining, running_truthy;
    size_t polls = 0;

    if (!state || state->type != JSON_OBJECT) return allow_result(0);
    has_runner = json_obj_get(state, "has_runner");
    if (has_runner && !as_truthy(has_runner)) return allow_result(0);   /* runner is None */
    if (!has_runner) return allow_result(0);

    draining_j = json_obj_get(state, "draining");
    external_j = json_obj_get(state, "external_drain");
    draining = as_truthy(draining_j) || as_truthy(external_j);

    /* getattr(runner, "_running", True): a missing key is the default True. */
    running_j = json_obj_get(state, "running");
    running_truthy = running_j ? as_truthy(running_j) : 1;

    received_at = as_num(json_obj_get(state, "received_at"), 0.0);
    now = as_num(json_obj_get(state, "now"), 0.0);
    deadline = received_at + W25_APISERVERF_CE83EA_FIRE_STARTUP_WAIT_SECONDS;
    job_id = json_get_str(state, "job_id", NULL);
    if (!job_id) job_id = "";

    for (;;) {
        if (!draining && running_truthy) return allow_result(polls);
        if (draining || now >= deadline) return refusal_body(job_id, polls);
        now += W25_APISERVERF_CE83EA_POLL_SECONDS;
        polls++;
        if (polls > 100000) return refusal_body(job_id, polls);
    }
}

json_t *w25_apiserverf_ce83ea_live_adapters(const json_t *state) {
    const json_t *adapters;
    json_t *out = json_object();
    int has_runner = 1;

    if (state && state->type == JSON_OBJECT) {
        const json_t *hr = json_obj_get(state, "has_runner");
        if (hr) has_runner = as_truthy(hr);
    }
    json_set(out, "refusal", w25_apiserverf_ce83ea_startup_gate(state));
    adapters = (state && state->type == JSON_OBJECT) ? json_obj_get(state, "adapters") : NULL;
    if (!has_runner || !as_truthy(adapters)) json_set(out, "adapters", json_null());
    else json_set(out, "adapters", json_copy(adapters));
    return out;
}

json_t *w25_apiserverf_ce83ea_constants(void) {
    json_t *o = json_object();
    json_set(o, "fire_startup_wait_seconds",
             json_number(W25_APISERVERF_CE83EA_FIRE_STARTUP_WAIT_SECONDS));
    json_set(o, "poll_seconds", json_number(W25_APISERVERF_CE83EA_POLL_SECONDS));
    json_set(o, "retry_after_seconds",
             json_number((double)W25_APISERVERF_CE83EA_RETRY_AFTER_SECONDS));
    json_set(o, "error_text", json_string(W25_APISERVERF_CE83EA_ERROR_TEXT));
    return o;
}
