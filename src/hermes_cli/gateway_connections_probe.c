/*
 * gateway_connections_probe.c — C11 port of upstream Hermes
 * hermes_cli/gateway_connections_probe.py (gate w25_gatewaycon_1c907d).
 *
 * PoP: w25_gatewaycon_1c907d_credentials @ hermes_cli/gateway_connections_probe.py:_credential_headers
 * PoP: w25_gatewaycon_1c907d_probe @ hermes_cli/gateway_connections_probe.py:_probe
 * PoP: w25_gatewaycon_1c907d_constants @ hermes_cli/gateway_connections_probe.py:_TIMEOUT_S
 *
 * One registry read yields the URL and the credential that belongs to it -- the
 * saved session token for a token-mode remote, the Bearer of the OAuth set for an
 * oauth-mode sign-in -- and one authenticated GET /api/status yields ok+version or
 * the refusal the route maps to HTTP.  The store read, the HTTP client and its
 * error type are seams: what is ported is the decision those values make.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_gatewaycon_1c907d.h"

/* Python truthiness of a JSON value. */
static int gc_truthy(const json_t *v) {
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

/* str(value) for the scalar an f-string would interpolate. */
static void gc_text(const json_t *v, char *buf, size_t size) {
    if (!v) { snprintf(buf, size, "None"); return; }
    switch (v->type) {
    case JSON_STRING: snprintf(buf, size, "%s", v->str_val ? v->str_val : ""); return;
    case JSON_NUMBER: snprintf(buf, size, "%g", v->num_val); return;
    case JSON_BOOL:   snprintf(buf, size, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NULL:   snprintf(buf, size, "None"); return;
    default:          snprintf(buf, size, "%s", ""); return;
    }
}

/* store.ConnectionStoreError(message) as the route maps it to HTTP. */
static json_t *gc_error(const char *message) {
    json_t *out = json_object();
    json_set(out, "error", json_string(message));
    return out;
}

json_t *w25_gatewaycon_1c907d_credentials(const json_t *req) {
    const json_t *snap = json_obj_get(req, "snapshot");
    const json_t *entry = json_obj_get(snap, "entry");
    const json_t *url = json_obj_get(snap, "url");
    const json_t *stored = json_obj_get(snap, "headers");
    json_t *headers, *out;

    /* headers = dict(snap["headers"]) */
    headers = stored && stored->type == JSON_OBJECT ? json_copy(stored) : json_object();

    if (!strcmp(json_get_str(entry, "authMode", ""), "oauth")) {
        const json_t *tokens = json_obj_get(snap, "tokens");
        const json_t *access;
        char buf[1024];
        if (!gc_truthy(tokens)) {
            json_free(headers);
            return gc_error("This connection is not signed in. Sign in to it first.");
        }
        access = json_obj_get(tokens, "accessToken");
        gc_text(access, buf, sizeof(buf));
        {
            char line[1200];
            snprintf(line, sizeof(line), "Bearer %s", buf);
            json_set(headers, "Authorization", json_string(line));
        }
    } else if (gc_truthy(json_obj_get(snap, "token"))) {
        json_set(headers, "X-Hermes-Session-Token",
                 json_copy(json_obj_get(snap, "token")));
    } else {
        json_free(headers);
        return gc_error("This connection has no saved session token. "
                        "Edit the connection and paste one.");
    }
    if (!gc_truthy(url)) {
        json_free(headers);
        return gc_error("This connection has no gateway address.");
    }
    out = json_object();
    json_set(out, "url", json_copy(url));
    json_set(out, "headers", headers);
    return out;
}

json_t *w25_gatewaycon_1c907d_probe(const json_t *req) {
    const json_t *net_error = json_obj_get(req, "net_error");
    const json_t *base = json_obj_get(req, "base_url");
    const json_t *body;
    json_t *out;
    char buf[1024];

    if (gc_truthy(net_error)) {
        char why[512];
        gc_text(net_error, why, sizeof(why));
        snprintf(buf, sizeof(buf), "Could not reach the gateway: %s", why);
        return gc_error(buf);
    }
    {
        const json_t *status_v = json_obj_get(req, "status");
        int status = status_v && status_v->type == JSON_NUMBER ? (int)status_v->num_val : 0;
        if (status == 401 || status == 403) {
            snprintf(buf, sizeof(buf),
                     "The gateway refused the saved credential (HTTP %d).", status);
            return gc_error(buf);
        }
        if (status >= 400) {
            snprintf(buf, sizeof(buf), "The gateway answered HTTP %d.", status);
            return gc_error(buf);
        }
    }
    if (gc_truthy(json_obj_get(req, "json_error"))) {
        body = NULL;
    } else {
        body = json_obj_get(req, "body");
        if (body && body->type == JSON_NULL) body = NULL;
    }
    out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "baseUrl", base ? json_copy(base) : json_null());
    /* version = (body or {}).get("version") or None */
    {
        const json_t *version = gc_truthy(body) ? json_obj_get(body, "version") : NULL;
        json_set(out, "version", gc_truthy(version) ? json_copy(version) : json_null());
    }
    return out;
}

json_t *w25_gatewaycon_1c907d_constants(void) {
    json_t *out = json_object();
    json_set(out, "timeout_s", json_number(8.0));
    return out;
}
