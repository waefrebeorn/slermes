/*
 * compute_host.c - faithful C11 port of selected pure helpers from
 * upstream Hermes tui_gateway/compute_host.py (gate w11_computehos_d481eb).
 *
 * PoP: w11_computehos_d481eb_history_meta @ tui_gateway/compute_host.py:_history_meta
 * PoP: w11_computehos_d481eb_default_workers @ tui_gateway/compute_host.py:_default_workers
 * PoP: w11_computehos_d481eb_host_transport_write @ tui_gateway/compute_host.py:_HostTransport.write
 * PoP: w11_computehos_d481eb_frame_handler @ tui_gateway/compute_host.py:ComputeHost._FRAME_HANDLERS
 * PoP: w11_computehos_d481eb_control_failure @ tui_gateway/compute_host.py:_CONTROL_FAILURES
 *
 * Process lifecycle, thread/executor management and server-bound turn handling are
 * intentionally not ported.
 */
#include "noc_w11_computehos_d481eb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python int() for the scalar types the config/session fields use. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_STRING) {
        char *end = NULL;
        long long x = strtoll(v->str_val, &end, 10);
        if (end == v->str_val || *end != '\0') return 0;
        *out = x;
        return 1;
    }
    return 0;
}

/* str(value or "") for the session/transport string coercions. */
static char *str_or_empty(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        snprintf(buf, sizeof(buf), "%.15g", v->num_val);
        return strdup(buf);
    }
    return strdup("");
}

json_t *w11_computehos_d481eb_history_meta(const json_t *session) {
    json_t *out = json_object();
    const char *sk = json_get_str(session, "session_key", "");
    json_set(out, "session_key", json_string(sk ? sk : ""));
    long long hv = 0;
    json_t *v = json_obj_get(session, "history_version");
    if (v && !py_int(v, &hv)) hv = 0;
    json_set(out, "history_version", json_number((double)hv));
    json_t *h = json_obj_get(session, "history");
    size_t count = json_is_array(h) ? json_len(h) : 0;
    json_set(out, "message_count", json_number((double)count));
    return out;
}

json_t *w11_computehos_d481eb_default_workers(const json_t *env_value) {
    long long n = 8;
    if (env_value && env_value->type == JSON_STRING && env_value->str_val && *env_value->str_val) {
        if (!py_int(env_value, &n)) n = 8;
    } else if (env_value && env_value->type == JSON_NUMBER) {
        if (!py_int(env_value, &n)) n = 8;
    }
    if (n < 2) n = 2;
    return json_number((double)n);
}

json_t *w11_computehos_d481eb_host_transport_write(const json_t *obj) {
    char *sid = strdup("");
    if (json_is_object(obj) && !strcmp(json_get_str(obj, "method", ""), "event")) {
        json_t *params = json_obj_get(obj, "params");
        if (json_is_object(params)) {
            json_t *sv = json_obj_get(params, "session_id");
            if (sv && sv->type != JSON_NULL) {
                free(sid);
                sid = str_or_empty(sv);
            }
        }
    }
    json_t *out = json_object();
    json_set(out, "type", json_string("rpc"));
    json_set(out, "sid", json_string(sid ? sid : ""));
    json_set(out, "message", json_copy(obj));
    free(sid);
    return out;
}

json_t *w11_computehos_d481eb_frame_handler(const char *kind) {
    static const char *const kinds[] = {"turn.start", "interrupt", "respond",
                                        "reload_mcp", "control", "shutdown"};
    static const char *const handlers[] = {"_handle_turn_start", "_handle_interrupt",
                                           "_handle_respond", "_handle_reload_mcp",
                                           "_handle_control", "_handle_shutdown"};
    if (!kind) return json_null();
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (!strcmp(kind, kinds[i])) return json_string(handlers[i]);
    return json_null();
}

json_t *w11_computehos_d481eb_control_failure(const char *route) {
    if (!route) return json_null();
    if (!strcmp(route, "session.save")) return json_string("session save failed");
    if (!strcmp(route, "session.compress")) return json_string("session compression failed");
    return json_null();
}
