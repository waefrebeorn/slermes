/*
 * turn_preflight_gate.c — faithful C11 port of agent/turn_preflight_gate.py.
 * Gate w11_turnprefli_5aad24.
 *
 * PoP: w11_turnprefli_5aad24_run_preflight_gate @ agent/turn_preflight_gate.py:run_preflight_gate
 */
#include "hermes_json.h"
#include "noc_w11_turnprefli_5aad24.h"
#include <stdlib.h>
#include <string.h>

static json_t *num_or_null(const json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_NUMBER) ? json_copy(v) : json_null();
}
static json_t *str_or_null(const json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? json_copy(v) : json_null();
}
static double num(const json_t *o, const char *k, double def) { return json_get_num(o, k, def); }

json_t *w11_turnprefli_5aad24_run_preflight_gate(const json_t *in) {
    json_t *out = json_object();
    const char *runtime_error = json_get_str(in, "runtime_error", "");
    int has_runtime_error = runtime_error && *runtime_error;
    int provider_pending = json_get_bool(in, "provider_overflow_recovery_pending", false) ? 1 : 0;
    long long threshold = (long long)num(in, "threshold_tokens", 0);
    double pressure = num(in, "request_pressure_tokens", 0);
    int last_present = 0;
    {
        json_t *lp = json_obj_get(in, "last_preflight_pressure");
        if (lp && lp->type == JSON_NUMBER) last_present = 1;
    }
    int warrants = json_get_bool(in, "warrants", false) ? 1 : 0;
    int anchored = json_get_bool(in, "anchored", false) ? 1 : 0;
    int defer = json_get_bool(in, "defer", false) ? 1 : 0;
    int timeout = json_get_bool(in, "timeout_exhausted_in", false) ? 1 : 0;
    int blocked = json_get_bool(in, "blocked_in", false) ? 1 : 0;
    int failed = json_get_bool(in, "failed_in", false) ? 1 : 0;
    long long api_count = (long long)num(in, "api_call_count_in", 0);
    long long attempts = (long long)num(in, "compression_attempts_in", 0);
    long long msg_count = (long long)num(in, "messages_count", 0);
    int ovf_preflight = 0, deferred = 0;
    const char *action = "fallthrough";

    if (has_runtime_error) {
        action = "break";
        failed = 1;
        api_count -= 1;
        msg_count += 1;
        json_set(out, "final_response", json_string(runtime_error));
        json_set(out, "_turn_exit_reason", json_string("ollama_runtime_context_too_small"));
        json_set(out, "_last_preflight_pressure", json_null());
    } else {
        ovf_preflight = provider_pending && (threshold <= 0 || pressure >= (double)threshold);
        if (provider_pending && !ovf_preflight) provider_pending = 0;
        if (last_present && pressure >= (double)threshold && !warrants) blocked = 1;
        deferred = anchored ? 0 : defer;
        attempts += ovf_preflight ? 1 : 0;
        action = json_get_str(in, "compression_action", "fallthrough");
        json_set(out, "final_response", str_or_null(in, "final_response_in"));
        json_set(out, "_turn_exit_reason", str_or_null(in, "turn_exit_reason_in"));
        json_set(out, "_last_preflight_pressure", num_or_null(in, "post_pressure"));
    }
    json_set_hdr(out, "action", action);
    json_set(out, "api_call_count", json_number((double)api_count));
    json_set(out, "compression_attempts", json_number((double)attempts));
    json_set(out, "failed", json_bool(failed != 0));
    json_set(out, "_compression_timeout_exhausted", json_bool(timeout != 0));
    json_set(out, "_preflight_compression_blocked", json_bool(blocked != 0));
    json_set(out, "_provider_overflow_recovery_pending", json_bool(provider_pending != 0));
    json_set(out, "messages_count", json_number((double)msg_count));
    json_set(out, "provider_overflow_preflight", json_bool(ovf_preflight != 0));
    json_set(out, "deferred", json_bool(deferred != 0));
    return out;
}

