/*
 * api_server_config_guard.c -- real C11 port of
 * gateway/platforms/api_server_config_guard.py (gate w22_apiserverc_6e486b).
 *
 * PoP: w22_apiserverc_6e486b_config_unavailable_body @ gateway/platforms/api_server_config_guard.py:config_unavailable_middleware
 * PoP: w22_apiserverc_6e486b_middleware @ gateway/platforms/api_server_config_guard.py:config_unavailable_middleware
 *
 * The upstream middleware is three lines around one decision: pass a handler's
 * result through, answer a ConfigBackendUnavailable with a 503 whose message is
 * `str(exc.code)`, or let anything else escape. The handler and the aiohttp
 * response are seams; the decision and the payload are ported verbatim.
 */
#include "hermes_json.h"
#include "noc_w22_apiserverc_6e486b.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ap_6e486b_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* str(value) for the scalar shapes an error `code` carries. A container would
 * need Python's repr; wire codes are scalars, so it degrades to "" there. */
static char *ap_6e486b_str(const json_t *v) {
    char buf[64];
    if (!v) return ap_6e486b_dup("None");
    switch (v->type) {
    case JSON_STRING: return ap_6e486b_dup(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return ap_6e486b_dup(v->bool_val ? "True" : "False");
    case JSON_NULL:   return ap_6e486b_dup("None");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            v->num_val > -9.007199254740992e15 && v->num_val < 9.007199254740992e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return ap_6e486b_dup(buf);
    default:
        return ap_6e486b_dup("");
    }
}

json_t *w22_apiserverc_6e486b_config_unavailable_body(const json_t *code) {
    json_t *body = json_object();
    json_t *error = json_object();
    char *message = ap_6e486b_str(code);
    json_set(error, "message", json_string(message ? message : ""));
    json_set(error, "type", json_string("server_error"));
    json_set(error, "code", json_string("config_backend_unavailable"));
    json_set(body, "error", error);
    free(message);
    return body;
}

json_t *w22_apiserverc_6e486b_middleware(const char *outcome, const json_t *value,
                                         const json_t *code) {
    json_t *out;

    if (!outcome || !outcome[0]) outcome = "return";

    if (!strcmp(outcome, "config_unavailable")) {
        /* SystemExit always carries `.code` (None when built without args), so
         * there is no AttributeError arm here -- str(None) is "None". */
        out = json_object();
        json_set(out, "action", json_string("response"));
        json_set(out, "status", json_number(503));
        json_set(out, "body", w22_apiserverc_6e486b_config_unavailable_body(code));
        return out;
    }
    if (!strcmp(outcome, "runtime_error")) {
        out = json_object();
        json_set(out, "action", json_string("raise"));
        json_set(out, "type", json_string("RuntimeError"));
        return out;
    }
    /* Handler finished: whichever object it returned is what the middleware
     * hands back, untouched. */
    out = json_object();
    json_set(out, "action", json_string("return"));
    json_set(out, "value", value ? json_copy(value) : json_null());
    return out;
}
