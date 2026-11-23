/*
 * agent_init_ollama.c — C home for upstream Hermes agent/agent_init_ollama.py.
 *
 * Faithful C11 ports of init-time local-server num_ctx detection and the
 * compressor clamp. Local-server / detection results are injected through the
 * args object so the C side never performs I/O or reads live config.
 *
 * PoP: w9_agentinito_5ea913_configure_num_ctx @ agent/agent_init_ollama.py:_configure_ollama_num_ctx
 * PoP: w9_agentinito_5ea913_clamp_compressor @ agent/agent_init_ollama.py:_clamp_compressor_to_ollama_num_ctx
 */
#include "noc_w9_agentinito_5ea913.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_int_arg(const json_t *v, long long *out)
{
    if (!v || json_is_null(v)) return 0;
    if (json_is_bool(v)) { *out = json_bool_value(v) ? 1 : 0; return 1; }
    if (json_is_number(v)) { *out = (long long)json_number_value(v); return 1; }
    if (!json_is_string(v)) return 0;
    const char *s = json_string_value(v);
    if (!s) return 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    if (!*s) return 0;
    long long n = 0;
    int last = 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); last = 1; }
        else if (*s == '_' && last) { last = 0; }
        else return 0;
    }
    if (!last) return 0;
    *out = sign * n;
    return 1;
}

/* PoP: w9_agentinito_5ea913_configure_num_ctx @ agent/agent_init_ollama.py:_configure_ollama_num_ctx */
json_t *w9_agentinito_5ea913_configure_num_ctx(json_t *a)
{
    json_t *cfg = json_obj_get(a, "model_cfg");
    json_t *override = json_is_object(cfg) ? json_obj_get(cfg, "ollama_num_ctx") : NULL;
    int override_present = override && !json_is_null(override);
    int debug = 0, info = 0;
    long long num_ctx = 0;
    int has_ctx = 0;

    if (override_present) {
        long long v;
        if (py_int_arg(override, &v)) { num_ctx = v; has_ctx = 1; }
        else debug++; /* invalid config value */
    }
    int is_local = json_get_bool(a, "is_local", 0) ? 1 : 0;
    if (!has_ctx && is_local) {
        if (json_get_bool(a, "detect_error", 0)) debug++;
        else {
            json_t *det = json_obj_get(a, "detected");
            if (json_is_number(det) && json_number_value(det) > 0) {
                num_ctx = (long long)json_number_value(det);
                has_ctx = 1;
            }
        }
    }
    json_t *ccl = json_obj_get(a, "config_context_length");
    long long ccl_v = json_is_number(ccl) ? (long long)json_number_value(ccl) : 0;
    if (has_ctx && num_ctx && !override_present && ccl_v && num_ctx > ccl_v) {
        info++; /* capped */
        num_ctx = ccl_v;
    }
    int quiet = json_get_bool(a, "quiet_mode", 0) ? 1 : 0;
    if (has_ctx && num_ctx && !quiet) info++; /* final "will request" log */

    json_t *out = json_object();
    json_set(out, "num_ctx", has_ctx ? json_number((double)num_ctx) : json_null());
    json_set(out, "info", json_number(info));
    json_set(out, "debug", json_number(debug));
    return out;
}

/* PoP: w9_agentinito_5ea913_clamp_compressor @ agent/agent_init_ollama.py:_clamp_compressor_to_ollama_num_ctx */
json_t *w9_agentinito_5ea913_clamp_compressor(json_t *a)
{
    json_t *nc = json_obj_get(a, "num_ctx");
    int has_ctx = json_is_number(nc) && json_number_value(nc) > 0;
    long long num_ctx = has_ctx ? (long long)json_number_value(nc) : 0;
    json_t *cw = json_obj_get(a, "cc_window");
    long long cc_window = (json_is_number(cw) && json_number_value(cw) != 0)
                              ? (long long)json_number_value(cw) : 0;
    int updated = has_ctx && num_ctx > 0 && cc_window && num_ctx < cc_window;
    json_t *out = json_object();
    json_set(out, "updated", json_bool(updated));
    json_set(out, "context_length", updated ? json_number((double)num_ctx) : json_null());
    return out;
}
