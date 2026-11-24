/*
 * messaging_relay.c - C11 port of upstream
 * hermes_cli/web_routers/messaging_relay.py (gate w24_messagingr_cf3c4b).
 *
 * PoP: w24_messagingr_cf3c4b_ask @ hermes_cli/web_routers/messaging_relay.py:_ask_gateway
 * PoP: w24_messagingr_cf3c4b_params @ hermes_cli/web_routers/messaging_relay.py:router handlers
 * PoP: w24_messagingr_cf3c4b_not_ready @ hermes_cli/web_routers/messaging_relay.py:_NOT_READY
 * PoP: w24_messagingr_cf3c4b_constants @ hermes_cli/web_routers/messaging_relay.py:_NOT_READY/router
 *
 * Upstream queries the gateway's control socket in a worker thread and raises
 * HTTPException for the refusals.  The socket and the thread are seams: the
 * answer is a value here and an HTTPException is the outcome value
 * ({"status", "body"}), so the classification -- and nothing about the
 * transport -- is what gets compared.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_messagingr_cf3c4b.h"

#define MR_GATEWAY_NOT_ANSWERING \
    "The gateway did not answer. Make sure it is running and up to date: it holds " \
    "the Team Gateway connection."

/* _NOT_READY, sorted as the JSON table spells it. */
static const char *const MR_NOT_READY[] = {
    "connector_unavailable", "connector_unreachable", "relay_not_provisioned",
};
#define MR_NOT_READY_COUNT (sizeof(MR_NOT_READY) / sizeof(MR_NOT_READY[0]))

typedef struct {
    const char *method;
    const char *path;
} mr_route;

static const mr_route MR_ROUTES[] = {
    {"GET", "/api/messaging/relay"},
    {"POST", "/api/messaging/relay/{platform}/link"},
    {"GET", "/api/messaging/relay/link/{code}"},
    {"DELETE", "/api/messaging/relay/{platform}/accounts/{platform_user_id}"},
};
#define MR_ROUTE_COUNT (sizeof(MR_ROUTES) / sizeof(MR_ROUTES[0]))

/* Python truthiness of a JSON value; NULL is an absent key (None). */
static int mr_truthy(const json_t *v) {
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

static char *mr_dup(const char *s) {
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* str(value) for the scalars a connector error code takes. */
static char *mr_str(const json_t *v) {
    char buf[64];

    if (!v || v->type == JSON_NULL) return mr_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return mr_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return mr_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val && fabs(v->num_val) < 1e16)
            snprintf(buf, sizeof buf, "%.0f", v->num_val);
        else
            snprintf(buf, sizeof buf, "%.15g", v->num_val);
        return mr_dup(buf);
    default:
        return mr_dup("");
    }
}

/* Python's type name, for the AttributeError a non-mapping answer raises. */
static const char *mr_type_name(const json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
    case JSON_NULL:   return "NoneType";
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    }
    return "object";
}

static int mr_not_ready_code(const char *code) {
    size_t i;
    for (i = 0; i < MR_NOT_READY_COUNT; i++)
        if (!strcmp(code, MR_NOT_READY[i])) return 1;
    return 0;
}

static json_t *mr_outcome(int status, json_t *body /* owned */) {
    json_t *o = json_object();
    json_set(o, "status", json_number((double)status));
    json_set(o, "body", body);
    return o;
}

json_t *w24_messagingr_cf3c4b_ask(const json_t *answer) {
    json_t *body;

    if (!answer || answer->type == JSON_NULL) {
        /* No answer: a stopped gateway, one too old for the verb, a timeout. */
        body = json_object();
        json_set(body, "code", json_string("gateway_not_answering"));
        json_set(body, "message", json_string(MR_GATEWAY_NOT_ANSWERING));
        return mr_outcome(503, body);
    }
    if (answer->type != JSON_OBJECT) {
        char msg[128];
        json_t *o = json_object();
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", mr_type_name(answer));
        json_set(o, "error", json_string(msg));
        return o;
    }
    if (!mr_truthy(json_obj_get(answer, "ok"))) {
        const json_t *error = json_obj_get(answer, "error");
        const json_t *message = json_obj_get(answer, "message");
        char *code = mr_truthy(error) ? mr_str(error) : mr_dup("connector_refused");
        int status;

        if (!code) return json_null();
        status = mr_not_ready_code(code) ? 503 : 409;
        body = json_object();
        json_set(body, "code", json_string(code));
        /* detail message: the answer's own message when truthy, else the code */
        json_set(body, "message",
                 mr_truthy(message) ? json_copy(message) : json_string(code));
        free(code);
        return mr_outcome(status, body);
    }
    /* ok: the route returns the answer minus "ok" */
    body = json_copy(answer);
    json_obj_del(body, "ok");
    return mr_outcome(200, body);
}

json_t *w24_messagingr_cf3c4b_params(const json_t *req) {
    const char *action = json_get_str(req, "action", "");
    const json_t *platform = json_obj_get(req, "platform");
    const json_t *code_value = json_obj_get(req, "code");
    const json_t *user = json_obj_get(req, "platform_user_id");
    json_t *o = json_object();

    if (!strcmp(action, "status")) {
        json_set(o, "action", json_string("status"));
    } else if (!strcmp(action, "request")) {
        json_set(o, "action", json_string("request"));
        json_set(o, "platform", platform ? json_copy(platform) : json_null());
    } else if (!strcmp(action, "state")) {
        json_set(o, "action", json_string("state"));
        json_set(o, "code", code_value ? json_copy(code_value) : json_null());
    } else if (!strcmp(action, "unlink")) {
        json_set(o, "action", json_string("unlink"));
        json_set(o, "platform", platform ? json_copy(platform) : json_null());
        json_set(o, "platform_user_id", user ? json_copy(user) : json_null());
    } else {
        json_free(o);
        return json_null();
    }
    return o;
}

json_t *w24_messagingr_cf3c4b_not_ready(const json_t *req) {
    const json_t *code = json_obj_get(req, "code");
    if (!code || code->type != JSON_STRING) return json_bool(false);
    return json_bool(mr_not_ready_code(code->str_val ? code->str_val : "") ? true : false);
}

json_t *w24_messagingr_cf3c4b_constants(void) {
    json_t *o = json_object();
    json_t *codes = json_array();
    json_t *routes = json_array();
    size_t i;

    for (i = 0; i < MR_NOT_READY_COUNT; i++) json_append(codes, json_string(MR_NOT_READY[i]));
    json_set(o, "not_ready", codes);
    for (i = 0; i < MR_ROUTE_COUNT; i++) {
        json_t *row = json_object();
        json_set(row, "method", json_string(MR_ROUTES[i].method));
        json_set(row, "path", json_string(MR_ROUTES[i].path));
        json_append(routes, row);
    }
    json_set(o, "routes", routes);
    return o;
}
