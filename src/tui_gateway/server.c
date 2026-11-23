/*
 * server.c — real C11 port of the pure dispatch leaves of upstream Hermes
 * tui_gateway/server.py (gate w6_server_8d971d).
 *
 * PoP: tuisrv_ok @ tui_gateway/server.py:_ok
 * PoP: tuisrv_err @ tui_gateway/server.py:_err
 * PoP: tuisrv_normalize_request @ tui_gateway/server.py:_normalize_request
 * PoP: tuisrv_is_model_switch_marker @ tui_gateway/server.py:_is_model_switch_marker
 * PoP: tuisrv_is_pivot_marker @ tui_gateway/server.py:_is_pivot_marker
 * PoP: tuisrv_coerce_int_config_value @ tui_gateway/server.py:_coerce_int_config_value
 * PoP: tuisrv_cli_exec_blocked @ tui_gateway/server.py:_cli_exec_blocked
 * PoP: tuisrv_turn_started_at @ tui_gateway/server.py:_turn_started_at
 * PoP: tuisrv_event_frame @ tui_gateway/server.py:_event_frame
 *
 * Networking, threading and the split @method handler modules are NOT ported;
 * only the deterministic leaves used by the JSON-RPC protocol layer are.
 */
#include "noc_w6_server_8d971d.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MODEL_SWITCH_PREFIX "[System: The active model for this chat has changed to "

static json_t *tuisrv_err_obj(json_t *rid, double code, const char *msg,
                              const json_t *data) {
    json_t *err = json_object();
    json_set(err, "code", json_number(code));
    json_set(err, "message", json_string(msg));
    if (data && data->type != JSON_NULL) json_set(err, "data", json_copy(data));
    json_t *out = json_object();
    json_set(out, "jsonrpc", json_string("2.0"));
    json_set(out, "id", rid ? json_copy(rid) : json_null());
    json_set(out, "error", err);
    return out;
}

json_t *tuisrv_ok(json_t *rid, const json_t *result) {
    json_t *out = json_object();
    json_set(out, "jsonrpc", json_string("2.0"));
    json_set(out, "id", rid ? json_copy(rid) : json_null());
    json_set(out, "result", result ? json_copy(result) : json_null());
    return out;
}

json_t *tuisrv_err(json_t *rid, double code, const char *msg, const json_t *data) {
    return tuisrv_err_obj(rid, code, msg, data);
}

json_t *tuisrv_normalize_request(const json_t *req) {
    if (!req || req->type != JSON_OBJECT)
        return tuisrv_err_obj(NULL, -32600, "invalid request: expected an object", NULL);
    json_t *rid = json_obj_get(req, "id");
    json_t *method = json_obj_get(req, "method");
    if (!method || method->type != JSON_STRING || !method->str_val || !method->str_val[0])
        return tuisrv_err_obj(rid, -32600, "invalid request: method must be a non-empty string", NULL);
    json_t *params = json_obj_get(req, "params");
    if (params && params->type != JSON_OBJECT && params->type != JSON_NULL)
        return tuisrv_err_obj(rid, -32602, "invalid params: expected an object", NULL);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    json_set(out, "rid", rid ? json_copy(rid) : json_null());
    json_set(out, "method", json_copy(method));
    json_set(out, "params", (params && params->type == JSON_OBJECT) ? json_copy(params) : json_object());
    return out;
}

json_t *tuisrv_is_model_switch_marker(const json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return json_bool(false);
    json_t *content = json_obj_get(entry, "content");
    if (!content || content->type != JSON_STRING || !content->str_val) return json_bool(false);
    return json_bool(strncmp(content->str_val, MODEL_SWITCH_PREFIX, strlen(MODEL_SWITCH_PREFIX)) == 0
                     ? true : false);
}

json_t *tuisrv_is_pivot_marker(const json_t *entry) {
    json_t *sw = tuisrv_is_model_switch_marker(entry);
    if (sw->bool_val) { json_free(sw); return json_bool(true); }
    json_free(sw);
    if (!entry || entry->type != JSON_OBJECT) return json_bool(false);
    json_t *dk = json_obj_get(entry, "display_kind");
    return json_bool(dk && dk->type == JSON_STRING && dk->str_val &&
                     !strcmp(dk->str_val, "personality_switch") ? true : false);
}

/* Python int(value) for the JSON scalar types used by config values. */
static int tuisrv_py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        while (*s && isspace((unsigned char)*s)) s++;
        int sign = 1;
        if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
        if (!*s || !isdigit((unsigned char)*s)) return 0;
        long long acc = 0;
        while (*s && isdigit((unsigned char)*s)) { acc = acc * 10 + (*s - '0'); s++; }
        while (*s && isspace((unsigned char)*s)) s++;
        if (*s) return 0;
        *out = sign * acc;
        return 1;
    }
    return 0;
}

json_t *tuisrv_coerce_int_config_value(const json_t *value, double def, double min_value) {
    long long coerced;
    long long d = (long long)def;
    if (!tuisrv_py_int(value, &coerced)) return json_number((double)d);
    if ((double)coerced < min_value) return json_number((double)d);
    return json_number((double)coerced);
}

static char *tuisrv_lower_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

json_t *tuisrv_cli_exec_blocked(const json_t *argv) {
    if (!argv || argv->type != JSON_ARRAY || json_len(argv) == 0)
        return json_string("bare `hermes` is interactive — use `/hermes chat -q …` or run `hermes` in another terminal");
    json_t *a0 = json_get(argv, 0);
    char *h0 = tuisrv_lower_dup(a0 && a0->type == JSON_STRING ? a0->str_val : "");
    char *h1 = json_len(argv) > 1 ? tuisrv_lower_dup(
        (json_get(argv, 1) && json_get(argv, 1)->type == JSON_STRING) ? json_get(argv, 1)->str_val : "") : NULL;
    const char *msg = NULL;
    if (h0 && !strcmp(h0, "setup"))
        msg = "`hermes setup` needs a full terminal — run it outside the TUI";
    else if (h0 && !strcmp(h0, "gateway"))
        msg = "`hermes gateway` is long-running — run it in another terminal";
    else if (h0 && h1 && !strcmp(h0, "sessions") && !strcmp(h1, "browse"))
        msg = "`hermes sessions browse` is interactive — use /resume here, or run browse in another terminal";
    else if (h0 && h1 && !strcmp(h0, "config") && !strcmp(h1, "edit"))
        msg = "`hermes config edit` needs $EDITOR in a real terminal";
    free(h0); free(h1);
    return msg ? json_string(msg) : json_null();
}

json_t *tuisrv_turn_started_at(const json_t *session) {
    if (!session || session->type != JSON_OBJECT) return json_null();
    json_t *inflight = json_obj_get(session, "inflight_turn");
    if (!inflight || inflight->type != JSON_OBJECT) return json_null();
    json_t *started = json_obj_get(inflight, "started_at");
    if (!started) return json_null();
    if (started->type == JSON_NUMBER && started->num_val != 0.0) return json_copy(started);
    if (started->type == JSON_BOOL && started->bool_val) return json_number(1.0);
    if (started->type == JSON_STRING && started->str_val && started->str_val[0]) {
        char *end = NULL;
        double d = strtod(started->str_val, &end);
        if (end && *end == '\0') return json_number(d);
    }
    return json_null();
}

json_t *tuisrv_event_frame(const char *event, const char *sid, const json_t *payload) {
    json_t *params = json_object();
    json_set(params, "type", json_string(event ? event : ""));
    json_set(params, "session_id", json_string(sid ? sid : ""));
    if (payload && payload->type != JSON_NULL) json_set(params, "payload", json_copy(payload));
    json_t *out = json_object();
    json_set(out, "jsonrpc", json_string("2.0"));
    json_set(out, "method", json_string("event"));
    json_set(out, "params", params);
    return out;
}
