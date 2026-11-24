/*
 * webapp_relay_target.c - C11 port of upstream hermes_cli/webapp_relay_target.py
 * (gate w24_webapprela_ffd6bc).
 *
 * PoP: w24_webapprela_ffd6bc_bearer @ hermes_cli/webapp_relay_target.py:Target.bearer
 * PoP: w24_webapprela_ffd6bc_entry_url @ hermes_cli/webapp_relay_target.py:entry_url
 * PoP: w24_webapprela_ffd6bc_resolve @ hermes_cli/webapp_relay_target.py:_resolve
 * PoP: w24_webapprela_ffd6bc_own_ticket @ hermes_cli/webapp_relay_target.py:_OWN_TICKET_PATHS.get
 * PoP: w24_webapprela_ffd6bc_constants @ hermes_cli/webapp_relay_target.py:<module constants>
 *
 * What is ported is every decision the relay makes before it dials: which
 * credential a Target carries, what a connection id names once the registry
 * answers, whether that answer is a Target, "no such thing" or an Unreachable
 * refusal (with the exact shareable text), and which sockets need a ticket of
 * their own.  The registry read, the blocking reach resolver, the SSH tunnel
 * and the ticket POST stay seams -- they are inputs here, so nothing blocks,
 * dials or writes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_webapprela_ffd6bc.h"

#define RELAY_SESSION_TOKEN_HEADER "X-Hermes-Session-Token"

static int py_truthy(const json_t *v) {
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

json_t *w24_webapprela_ffd6bc_bearer(const json_t *headers) {
    const json_t *auth = json_obj_get(headers, "Authorization");
    const char *s = (auth && auth->type == JSON_STRING && auth->str_val) ? auth->str_val : "";

    if (!strncmp(s, "Bearer ", 7)) s += 7;
    return json_string(s);
}

json_t *w24_webapprela_ffd6bc_entry_url(const json_t *entry) {
    const json_t *url, *host, *id;

    if (!entry || entry->type != JSON_OBJECT) return json_null();  /* _entry() -> None */
    url = json_obj_get(entry, "url");
    host = json_obj_get(entry, "host");
    id = json_obj_get(entry, "id");
    if (py_truthy(url)) return json_copy(url);
    if (py_truthy(host)) return json_copy(host);
    if (id) return json_copy(id);
    return json_null();
}

/* any(k in headers for k in ("Authorization", SESSION_TOKEN_HEADER)) */
static int headers_have_credential(const json_t *headers) {
    if (!headers || headers->type != JSON_OBJECT) return 0;
    if (json_obj_get(headers, "Authorization")) return 1;
    if (json_obj_get(headers, RELAY_SESSION_TOKEN_HEADER)) return 1;
    return 0;
}

json_t *w24_webapprela_ffd6bc_resolve(const json_t *req) {
    const json_t *entry = json_obj_get(req, "entry");
    const json_t *reach = json_obj_get(req, "reach");
    const char *connection_id = json_get_str(req, "connection_id", "");
    const char *kind = reach ? json_get_str(reach, "kind", "ok") : "ok";
    const json_t *label_v = entry && entry->type == JSON_OBJECT ? json_obj_get(entry, "label") : NULL;
    const char *label = label_v && label_v->type == JSON_STRING && label_v->str_val
                            ? label_v->str_val : "";
    json_t *out;

    if (!entry || entry->type != JSON_OBJECT) {
        out = json_object();
        json_set(out, "kind", json_string("none"));
        return out;
    }
    if (!strcmp(json_get_str(entry, "kind", ""), "local")) {
        out = json_object();
        json_set(out, "kind", json_string("none"));
        return out;
    }
    if (!strcmp(kind, "nosuch")) {           /* NoSuchConnection -> None */
        out = json_object();
        json_set(out, "kind", json_string("none"));
        return out;
    }
    if (!strcmp(kind, "error")) {            /* an _REACH_ERRORS type -> Unreachable */
        out = json_object();
        json_set(out, "kind", json_string("error"));
        json_set(out, "message", json_string(json_get_str(reach, "message", "")));
        return out;
    }
    {
        const json_t *headers = json_obj_get(reach, "headers");
        if (!headers_have_credential(headers)) {
            char *msg;
            size_t need = strlen(label) + strlen(" has no saved credential. Edit it in Settings \xE2\x86\x92 Gateways.") + 1;
            msg = (char *)malloc(need);
            out = json_object();
            json_set(out, "kind", json_string("error"));
            if (msg) {
                snprintf(msg, need, "%s has no saved credential. Edit it in Settings \xE2\x86\x92 Gateways.",
                         label);
                json_set(out, "message", json_string(msg));
                free(msg);
            } else {
                json_set(out, "message", json_string(""));
            }
            return out;
        }
        out = json_object();
        json_set(out, "kind", json_string("target"));
        {
            json_t *target = json_object();
            json_set(target, "connection_id", json_string(connection_id));
            json_set(target, "label", json_string(label));
            json_set(target, "base_url", json_string(json_get_str(reach, "base_url", "")));
            json_set(target, "headers", headers ? json_copy(headers) : json_object());
            json_set(out, "target", target);
        }
        return out;
    }
}

json_t *w24_webapprela_ffd6bc_own_ticket(const json_t *path) {
    if (!path || path->type != JSON_STRING) return json_null();
    if (!strcmp(path->str_val ? path->str_val : "", "/api/host-terminal"))
        return json_string("host-terminal");
    return json_null();
}

json_t *w24_webapprela_ffd6bc_constants(void) {
    json_t *o = json_object();
    json_t *paths = json_object();
    json_set(paths, "/api/host-terminal", json_string("host-terminal"));
    json_set(o, "own_ticket_paths", paths);
    json_set(o, "session_token_header", json_string(RELAY_SESSION_TOKEN_HEADER));
    return o;
}
