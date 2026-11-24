/*
 * auth.c - C11 port of upstream plugins/model-providers/solstice/auth.py
 * (gate w25_auth_b05ad0).
 *
 * PoP: w25_auth_b05ad0_discover @ plugins/model-providers/solstice/auth.py:discover_client
 * PoP: w25_auth_b05ad0_broker @ plugins/model-providers/solstice/auth.py:broker_token_request
 * PoP: w25_auth_b05ad0_constants @ plugins/model-providers/solstice/auth.py:<module constants>
 *
 * The NAS broker is the seam: the HTTP round trip, the user's Nous access
 * token, the TLS verification policy and the single-use refresh all stay in
 * the caller.  Ported here is the value core those calls are decided by: which
 * fields the portal's config must carry, which grant fields may be forwarded,
 * and which status folds a refresh into a forced re-login.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_auth_b05ad0.h"

#define AUTH_CONFIG_PATH   W25_AUTH_B05AD0_CONFIG_PATH
#define AUTH_EXCHANGE_PATH W25_AUTH_B05AD0_EXCHANGE_PATH
#define AUTH_REFRESH_PATH  W25_AUTH_B05AD0_REFRESH_PATH
#define AUTH_TIMEOUT_S     30.0

/* _CONFIG_FIELDS in the order discover_client() checks them. */
static const char *const AUTH_CONFIG_FIELDS[W25_AUTH_B05AD0_CONFIG_FIELDS] = {
    "client_id", "authorize_url", "scope",
};

/* The grant keys broker_token_request() forwards. */
static const char *const AUTH_BODY_FIELDS[W25_AUTH_B05AD0_GRANT_BODY_FIELDS] = {
    "code", "code_verifier", "redirect_uri", "refresh_token",
};

static char *auth_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

/* Python truthiness. */
static int auth_truthy(const json_t *v) {
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

/* str(v) for the shapes a config/grant field can hold. */
static char *auth_pystr(const json_t *v) {
    char buf[64];
    if (!v) return auth_dup("");
    switch (v->type) {
    case JSON_NULL:
        return auth_dup("None");
    case JSON_BOOL:
        return auth_dup(v->bool_val ? "True" : "False");
    case JSON_STRING:
        return auth_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == (double)(long long)d && d > -1e15 && d < 1e15)
            snprintf(buf, sizeof buf, "%lld", (long long)d);
        else
            snprintf(buf, sizeof buf, "%g", d);
        return auth_dup(buf);
    }
    default:
        return auth_dup("");
    }
}

/* str(v or ""): a falsy value becomes the empty string before str(). */
static char *auth_text(const json_t *v) {
    if (!auth_truthy(v)) return auth_dup("");
    return auth_pystr(v);
}

/* Python str.strip(). */
static char *auth_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    char *out;
    size_t n;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

static json_t *auth_request_obj(const char *method, const char *path, json_t *body /* owned */) {
    json_t *r = json_object();
    json_set(r, "method", json_string(method));
    json_set(r, "path", json_string(path));
    json_set(r, "body", body ? body : json_null());
    return r;
}

static json_t *auth_envelope(int ok, json_t *value /* owned */, const char *code,
                             int relogin, json_t *request /* owned */) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(ok ? true : false));
    json_set(o, "value", value ? value : json_null());
    json_set(o, "code", code ? json_string(code) : json_null());
    json_set(o, "relogin", json_bool(relogin ? true : false));
    json_set(o, "request", request ? request : json_null());
    return o;
}

json_t *w25_auth_b05ad0_discover(const json_t *payload) {
    json_t *missing = json_array();
    json_t *value = json_object();
    size_t i;

    /* payload.get(...) on a non-mapping raises AttributeError after the GET. */
    if (payload && payload->type != JSON_OBJECT) {
        json_free(value);
        json_free(missing);
        return auth_envelope(0, NULL, "AttributeError", 0,
                             auth_request_obj("GET", AUTH_CONFIG_PATH, NULL));
    }

    for (i = 0; i < W25_AUTH_B05AD0_CONFIG_FIELDS; i++) {
        const char *field = AUTH_CONFIG_FIELDS[i];
        char *raw = auth_text(json_obj_get(payload, field));
        char *clean = auth_strip(raw);
        free(raw);
        if (!clean || !clean[0]) {
            json_append(missing, json_string(field));
        } else {
            json_set(value, field, json_string(clean));
        }
        free(clean);
    }

    if (json_len(missing) > 0) {
        json_free(value);
        json_free(missing);
        return auth_envelope(0, NULL, "solstice_config_invalid", 0,
                             auth_request_obj("GET", AUTH_CONFIG_PATH, NULL));
    }
    json_free(missing);
    return auth_envelope(1, value, NULL, 0, auth_request_obj("GET", AUTH_CONFIG_PATH, NULL));
}

json_t *w25_auth_b05ad0_broker(const json_t *grant, const json_t *nas) {
    const json_t *gt = json_obj_get(grant, "grant_type");
    const char *path;
    json_t *body;
    json_t *request;
    size_t i, j;
    int refreshing;

    /* path = _TOKEN_PATHS[grant["grant_type"]]: a KeyError for anything else. */
    if (!gt || gt->type != JSON_STRING || !gt->str_val)
        return auth_envelope(0, NULL, "KeyError", 0, NULL);
    if (!strcmp(gt->str_val, "authorization_code"))
        path = AUTH_EXCHANGE_PATH;
    else if (!strcmp(gt->str_val, "refresh_token"))
        path = AUTH_REFRESH_PATH;
    else
        return auth_envelope(0, NULL, "KeyError", 0, NULL);
    refreshing = !strcmp(gt->str_val, "refresh_token");

    /* body = {k: v for k, v in grant.items() if k in _BODY_KEYS} */
    body = json_object();
    for (i = 0; i < json_object_size(grant); i++) {
        const char *key = json_object_get_key_at(grant, i);
        if (!key) continue;
        for (j = 0; j < W25_AUTH_B05AD0_GRANT_BODY_FIELDS; j++) {
            if (!strcmp(key, AUTH_BODY_FIELDS[j])) {
                json_set(body, key, json_copy(json_object_get_at(grant, i)));
                break;
            }
        }
    }

    /* resolve_nous_access_token() failing raises before the POST is composed. */
    if (!nas || json_get_bool(nas, "token_error", false)) {
        json_free(body);
        return auth_envelope(0, NULL, "solstice_nous_login_required", 1, NULL);
    }

    request = auth_request_obj("POST", path, body);

    if (json_get_str(nas, "raise", NULL))
        return auth_envelope(0, NULL, "solstice_broker_unreachable", 0, request);

    {
        int status = (int)json_get_num(nas, "status", 200);
        if (status == 404)
            return auth_envelope(0, NULL, "solstice_not_configured", 0, request);
        if (status >= 400)
            return auth_envelope(0, NULL,
                                 refreshing ? "invalid_grant" : "solstice_broker_failed",
                                 refreshing, request);
    }
    return auth_envelope(1, json_copy(json_obj_get(nas, "payload")), NULL, 0, request);
}

json_t *w25_auth_b05ad0_constants(void) {
    json_t *out = json_object();
    json_t *paths = json_object();
    json_set(paths, "authorization_code", json_string(AUTH_EXCHANGE_PATH));
    json_set(paths, "refresh_token", json_string(AUTH_REFRESH_PATH));
    json_set(out, "nas_base_url", json_string(W25_AUTH_B05AD0_NAS_BASE_URL));
    json_set(out, "config_path", json_string(AUTH_CONFIG_PATH));
    json_set(out, "timeout_seconds", json_number(AUTH_TIMEOUT_S));
    json_set(out, "token_paths", paths);
    return out;
}
