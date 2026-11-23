/*
 * ws.c — C home for upstream Hermes tui_gateway/ws.py
 * Gate: w5_ws_735c83
 *
 * PoP: w5_ws_735c83_peer_label @ tui_gateway/ws.py:_ws_peer_label
 * PoP: w5_ws_735c83_is_unknown_method @ tui_gateway/ws.py:_is_unknown_method
 * PoP: w5_ws_735c83_is_concurrent_read @ tui_gateway/ws.py:_WS_CONCURRENT_READ_METHODS
 * PoP: w5_ws_735c83_is_streaming_event @ tui_gateway/ws.py:_STREAMING_EVENT_TYPES
 * PoP: w5_ws_735c83_answers_open_request @ tui_gateway/ws.py:_answers_open_request
 *
 * Only the pure framing/classification seams are ported. The async session
 * loop, socket writes and thread/loop handoff stay in the Python transport.
 */
#include "noc_w5_ws_735c83.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ws_num_to_str(struct json_t *v) {
    char buf[64];
    if (!v) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NULL) return strdup("None");
    return strdup("");
}

static int ws_truthy(struct json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    return 1;
}

char *w5_ws_735c83_peer_label(const struct json_t *client) {
    if (!client || client->type != JSON_OBJECT) return strdup("unknown");
    struct json_t *hostv = json_obj_get(client, "host");
    struct json_t *portv = json_obj_get(client, "port");
    char *host = ws_truthy(hostv) ? ws_num_to_str(hostv) : strdup("unknown");
    if (!portv || portv->type == JSON_NULL) return host;
    char *port = ws_num_to_str(portv);
    size_t n = strlen(host) + strlen(port) + 2;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s:%s", host, port);
    free(host); free(port);
    return out ? out : strdup("unknown");
}

int w5_ws_735c83_is_unknown_method(const struct json_t *resp) {
    if (!resp || resp->type != JSON_OBJECT) return 0;
    struct json_t *err = json_obj_get(resp, "error");
    if (!err || err->type != JSON_OBJECT) return 0;
    struct json_t *code = json_obj_get(err, "code");
    return (code && code->type == JSON_NUMBER && code->num_val == -32601.0) ? 1 : 0;
}

int w5_ws_735c83_is_concurrent_read(const char *method) {
    if (!method) return 0;
    return (!strcmp(method, "session.context_breakdown") ||
            !strcmp(method, "session.transcript") ||
            !strcmp(method, "session.account_usage")) ? 1 : 0;
}

int w5_ws_735c83_is_streaming_event(const char *event_type) {
    if (!event_type) return 0;
    return (!strcmp(event_type, "message.delta") ||
            !strcmp(event_type, "reasoning.delta") ||
            !strcmp(event_type, "thinking.delta")) ? 1 : 0;
}

int w5_ws_735c83_answers_open_request(const struct json_t *req, int is_response) {
    if (!req || req->type != JSON_OBJECT) return 0;
    struct json_t *method = json_obj_get(req, "method");
    if (method && method->type == JSON_STRING && method->str_val &&
        !strcmp(method->str_val, "clarify.lock")) return 1;
    return is_response ? 1 : 0;
}
