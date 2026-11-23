/*
 * session_connections.c - C port of upstream Hermes gateway/session_connections.py
 *
 * PoP: w14_sessioncon_1a39cb_session_owned @ gateway/session_connections.py:session_owned
 * PoP: w14_sessioncon_1a39cb_methods @ gateway/session_connections.py:METHODS
 * PoP: w14_sessioncon_1a39cb_connector_refusal @ gateway/session_connections.py:ConnectorRefusal.__init__
 *
 * The session-owned envelope predicate, the handled-method list and the refusal
 * wire shape. Operation dispatch/live I/O are replaced by JSON seams.
 */
#include "noc_w14_sessioncon_1a39cb.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const json_t *og(const json_t *o, const char *k) {
    return json_is_object(o) ? json_obj_get(o, k) : NULL;
}
static char *py_str(const json_t *v) {
    char b[64];
    if (v && v->type == JSON_STRING) return _strdup(v->str_val ? v->str_val : "");
    if (v && v->type == JSON_BOOL) return _strdup(v->bool_val ? "True" : "False");
    if (v && v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(b, sizeof b, "%lld", (long long)v->num_val);
        else snprintf(b, sizeof b, "%.17g", v->num_val);
        return _strdup(b);
    }
    return _strdup("");
}

json_t *w14_sessioncon_1a39cb_session_owned(const struct json_t *params) {
    const json_t *owner = og(params, "owner");
    const json_t *t = og(owner, "type");
    return json_bool(json_is_object(owner) && t && t->type == JSON_STRING && t->str_val
                     && !strcmp(t->str_val, "session"));
}

json_t *w14_sessioncon_1a39cb_methods(void) {
    json_t *a = json_array();
    json_append(a, json_string("connection.respond"));
    json_append(a, json_string("connectors.operation.status"));
    json_append(a, json_string("connectors.operation.wake"));
    json_append(a, json_string("connectors.connect"));
    return a;
}

json_t *w14_sessioncon_1a39cb_connector_refusal(const struct json_t *code, const struct json_t *reason,
                                                const struct json_t *message) {
    json_t *out = json_object();
    json_set(out, "wire_code", code ? json_copy(code) : json_null());
    json_set(out, "wire_message", message ? json_copy(message) : json_null());
    char *rs = py_str(reason);
    json_set(out, "reason", json_string(rs));
    json_set(out, "detail", message ? json_copy(message) : json_null());
    json_set(out, "field", json_null());
    json_t *args = json_array();
    json_append(args, json_string(rs));
    json_set(out, "args", args);
    free(rs);
    return out;
}
