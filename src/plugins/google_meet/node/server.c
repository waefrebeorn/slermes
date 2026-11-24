/*
 * server.c -- real C11 port of upstream Hermes
 *   plugins/google_meet/node/server.py.
 *
 * PoP: w20_server_266a25_start_bot @ plugins/google_meet/node/server.py:_rpc_start_bot
 * PoP: w20_server_266a25_say @ plugins/google_meet/node/server.py:_rpc_say
 * PoP: w20_server_266a25_dispatch @ plugins/google_meet/node/server.py:_RPC
 * PoP: w20_server_266a25_pong @ plugins/google_meet/node/server.py:NodeServer._handle_request
 * PoP: w20_server_266a25_error_envelope @ plugins/google_meet/node/protocol.py:make_error
 * PoP: w20_server_266a25_ensure_token @ plugins/google_meet/node/server.py:NodeServer.ensure_token
 * PoP: w20_server_266a25_start_bot_keys @ plugins/google_meet/node/server.py:_START_BOT_KEYS
 *
 * The WebSocket loop, the lazy ``websockets`` import and the live
 * process_manager stay upstream; what is ported is the handler whitelist, the
 * queueing decision, the token decision and the envelope shapes.
 */
#include "hermes_json.h"
#include "noc_w20_server_266a25.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const NODE_START_BOT_KEYS[] = {
    "url", "guest_name", "duration", "headed", "auth_state", "session_id", "out_dir", NULL
};

static int node_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

json_t *w20_server_266a25_start_bot_keys(void) {
    json_t *arr = json_array();
    int i;
    for (i = 0; NODE_START_BOT_KEYS[i]; i++) json_append(arr, json_string(NODE_START_BOT_KEYS[i]));
    return arr;
}

json_t *w20_server_266a25_start_bot(const json_t *payload, const json_t *start_result) {
    json_t *out = json_object();
    json_t *kwargs = json_object();
    int i, has_url = 0;
    for (i = 0; NODE_START_BOT_KEYS[i]; i++) {
        json_t *v = json_obj_get(payload, NODE_START_BOT_KEYS[i]);
        if (!v) continue;                       /* `if k in payload` */
        json_set(kwargs, NODE_START_BOT_KEYS[i], json_copy(v));
        if (!strcmp(NODE_START_BOT_KEYS[i], "url")) has_url = 1;
    }
    if (!has_url) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "error", json_string("missing 'url' in payload"));
        json_set(out, "kwargs", kwargs);
        json_set(out, "payload", json_null());
        return out;
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "error", json_null());
    json_set(out, "kwargs", kwargs);
    json_set(out, "payload", start_result ? json_copy(start_result) : json_null());
    return out;
}

json_t *w20_server_266a25_say(const json_t *payload, const json_t *active, int write_ok) {
    json_t *out = json_object();
    json_t *text = payload ? json_obj_get(payload, "text") : NULL;
    int enqueued = 0;
    /* `if active and active.get("out_dir")` then the append; OSError suppresses. */
    if (active && active->type == JSON_OBJECT && node_truthy(json_obj_get(active, "out_dir"))) {
        enqueued = write_ok ? 1 : 0;
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "enqueued", json_bool(enqueued));
    json_set(out, "text", text ? json_copy(text) : json_string(""));
    return out;
}

/* Python repr() of the ASCII request-type strings the node accepts. */
static void node_repr(const char *s, char *out, size_t cap) {
    size_t w = 0, i;
    int has_single = 0;
    char quote;
    if (!s) s = "";
    for (i = 0; s[i]; i++) {
        if (s[i] == '\'') has_single = 1;
    }
    quote = has_single ? '"' : '\'';
    if (w + 1 < cap) out[w++] = quote;
    for (i = 0; s[i] && w + 4 < cap; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\' || c == '\n' || c == '\t' || c == '\r') {
            out[w++] = '\\';
            out[w++] = c == '\\' ? '\\' : (c == '\n' ? 'n' : (c == '\t' ? 't' : 'r'));
        } else if ((char)c == quote) {
            out[w++] = '\\';
            out[w++] = (char)c;
        } else {
            out[w++] = (char)c;
        }
    }
    if (w + 1 < cap) out[w++] = quote;
    out[w < cap ? w : cap - 1] = '\0';
}

static int node_is_rpc_type(const char *type) {
    static const char *const RPC_TYPES[] = {
        "start_bot", "stop", "status", "transcript", "say", NULL
    };
    int i;
    if (!type) return 0;
    for (i = 0; RPC_TYPES[i]; i++) {
        if (!strcmp(type, RPC_TYPES[i])) return 1;
    }
    return 0;
}

json_t *w20_server_266a25_dispatch(const char *type) {
    json_t *out = json_object();
    if (type && !strcmp(type, "ping")) {
        json_set(out, "kind", json_string("ping"));
        return out;
    }
    if (node_is_rpc_type(type)) {
        json_set(out, "kind", json_string("rpc"));
        json_set(out, "handler", json_string(type));
        return out;
    }
    {
        char repr[256], msg[300];
        node_repr(type, repr, sizeof(repr));
        snprintf(msg, sizeof(msg), "unhandled type: %s", repr);
        json_set(out, "kind", json_string("error"));
        json_set(out, "message", json_string(msg));
    }
    return out;
}

json_t *w20_server_266a25_pong(const json_t *msg, const char *display_name, double ts) {
    json_t *out = json_object();
    json_t *payload = json_object();
    json_t *id = msg ? json_obj_get(msg, "id") : NULL;
    json_set(payload, "display_name", json_string(display_name ? display_name : ""));
    json_set(payload, "ts", json_number(ts));
    json_set(out, "type", json_string("pong"));
    json_set(out, "id", id ? json_copy(id) : json_null());
    json_set(out, "payload", payload);
    return out;
}

json_t *w20_server_266a25_error_envelope(const char *id, const char *reason) {
    json_t *out = json_object();
    json_set(out, "type", json_string("error"));
    json_set(out, "id", json_string(id ? id : ""));
    json_set(out, "error", json_string(reason ? reason : ""));
    return out;
}

json_t *w20_server_266a25_ensure_token(const json_t *token_data, const json_t *cached) {
    json_t *out = json_object();
    json_t *tok = NULL;
    if (node_truthy(cached)) {
        /* `if self._token: return self._token` -- whatever truthy value it holds. */
        json_set(out, "token", json_copy(cached));
        json_set(out, "generate", json_bool(0));
        return out;
    }
    if (token_data && token_data->type == JSON_OBJECT) tok = json_obj_get(token_data, "token");
    if (tok && tok->type == JSON_STRING && tok->str_val && tok->str_val[0]) {
        json_set(out, "token", json_string(tok->str_val));
        json_set(out, "generate", json_bool(0));
        return out;
    }
    /* secrets.token_hex(16) -- 32 hex chars, minted and persisted by the caller */
    json_set(out, "token", json_null());
    json_set(out, "generate", json_bool(1));
    return out;
}
