/*
 * credentials.c — C11 port of upstream Hermes
 * plugins/config_backends/remote/credentials.py (gate w26_credential_0a5825).
 *
 * PoP: w26_credential_0a5825_idp_env @ plugins/config_backends/remote/credentials.py:_idp_env
 * PoP: w26_credential_0a5825_credential_kind @ plugins/config_backends/remote/credentials.py:credential_kind
 * PoP: w26_credential_0a5825_idp_form @ plugins/config_backends/remote/credentials.py:_idp_token
 * PoP: w26_credential_0a5825_cache_ttl_offset @ plugins/config_backends/remote/credentials.py:_idp_token
 * PoP: w26_credential_0a5825_constants @ plugins/config_backends/remote/credentials.py:<module constants>
 *
 * os.environ, the OAuth2 HTTP round trip, the token cache and time.monotonic are
 * seams.  What is ported is what their answers decide: which credential kind a
 * configuration selects, the form body the grant sends, and how long the
 * response's expiry is allowed to live in the cache.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_credential_0a5825.h"

/* _IDP_KEYS, in declaration order: the env name is the upper-cased key. */
static const char *const CRED_IDP_KEYS[] = {"token_url", "client_id", "client_secret", "scope"};
#define CRED_IDP_KEY_COUNT (sizeof(CRED_IDP_KEYS) / sizeof(CRED_IDP_KEYS[0]))

/* HERMES_CONFIG_*: which backend, which plane, which agent record. */
static const char *const CRED_PLANE_CONFIG_ENV[] = {
    "HERMES_CONFIG_BACKEND", "HERMES_CONFIG_REMOTE_URL", "HERMES_CONFIG_INSTANCE_ID",
};
/* Cloud: where auth.json lives and where its refresh token is sent. */
static const char *const CRED_PLANE_CLOUD_ENV[] = {
    "HERMES_SHARED_AUTH_DIR", "HERMES_PORTAL_BASE_URL", "NOUS_PORTAL_BASE_URL",
};
#define CRED_PLANE_CONFIG_ENV_COUNT (sizeof(CRED_PLANE_CONFIG_ENV) / sizeof(CRED_PLANE_CONFIG_ENV[0]))
#define CRED_PLANE_CLOUD_ENV_COUNT (sizeof(CRED_PLANE_CLOUD_ENV) / sizeof(CRED_PLANE_CLOUD_ENV[0]))

static char *cred_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Python str.strip(): ASCII whitespace from both ends. */
static char *cred_strip_copy(const char *s) {
    char *buf, *start;
    size_t end;
    if (!s) return cred_strdup("");
    buf = cred_strdup(s);
    if (!buf) return NULL;
    start = buf;
    while (*start && isspace((unsigned char)*start)) start++;
    end = strlen(start);
    while (end > 0 && isspace((unsigned char)start[end - 1])) end--;
    start[end] = '\0';
    if (start != buf) memmove(buf, start, end + 1);
    return buf;
}

/* os.environ.get(IDP_ENV_PREFIX + key.upper(), "").strip() */
static char *cred_env_value(const json_t *env, const char *key) {
    char name[128];
    const json_t *v;
    snprintf(name, sizeof(name), "%s%s", W26_CREDENTIAL_0A5825_IDP_ENV_PREFIX, key);
    {
        size_t i;
        for (i = 0; name[i]; i++) name[i] = (char)toupper((unsigned char)name[i]);
    }
    v = (env && env->type == JSON_OBJECT) ? json_obj_get(env, name) : NULL;
    if (!v || v->type != JSON_STRING) return cred_strdup("");
    return cred_strip_copy(v->str_val);
}

static void cred_idp_view(const json_t *env, char *out[CRED_IDP_KEY_COUNT]) {
    size_t i;
    for (i = 0; i < CRED_IDP_KEY_COUNT; i++) out[i] = cred_env_value(env, CRED_IDP_KEYS[i]);
}

json_t *w26_credential_0a5825_idp_env(const json_t *env) {
    char *vals[CRED_IDP_KEY_COUNT];
    json_t *o = json_object();
    size_t i;
    cred_idp_view(env, vals);
    for (i = 0; i < CRED_IDP_KEY_COUNT; i++) {
        json_set(o, CRED_IDP_KEYS[i], json_string(vals[i] ? vals[i] : ""));
        free(vals[i]);
    }
    return o;
}

json_t *w26_credential_0a5825_credential_kind(const json_t *env) {
    char *token_url = cred_env_value(env, "token_url");
    json_t *out = json_string(token_url && token_url[0] ? "idp" : "nous");
    free(token_url);
    return out;
}

json_t *w26_credential_0a5825_idp_form(const json_t *env) {
    char *vals[CRED_IDP_KEY_COUNT];
    json_t *o;
    cred_idp_view(env, vals);
    o = json_object();
    json_set(o, "grant_type", json_string("client_credentials"));
    json_set(o, "client_id", json_string(vals[1] ? vals[1] : ""));
    json_set(o, "client_secret", json_string(vals[2] ? vals[2] : ""));
    if (vals[3] && vals[3][0]) json_set(o, "scope", json_string(vals[3]));
    {
        size_t i;
        for (i = 0; i < CRED_IDP_KEY_COUNT; i++) free(vals[i]);
    }
    return o;
}

json_t *w26_credential_0a5825_cache_ttl_offset(const json_t *expires_in) {
    double ttl = 60.0;
    /* isinstance(expires_in, int | float) and expires_in > 0 -- a bool is an int
     * in Python, so true earns the 1-second ttl. */
    if (expires_in && (expires_in->type == JSON_NUMBER ||
                       (expires_in->type == JSON_BOOL && expires_in->bool_val))) {
        double raw = expires_in->type == JSON_BOOL ? 1.0 : expires_in->num_val;
        if (raw > 0.0) ttl = raw;
    }
    return json_number(ttl - 30.0 > 0.0 ? ttl - 30.0 : 0.0);
}

json_t *w26_credential_0a5825_constants(void) {
    /* Sorted, as a set view of PLANE_CREDENTIAL_ENV_NAMES is reported. */
    static const char *const sorted_names[] = {
        "GATEWAY_RELAY_IDP_CLIENT_ID", "GATEWAY_RELAY_IDP_CLIENT_SECRET",
        "GATEWAY_RELAY_IDP_SCOPE", "GATEWAY_RELAY_IDP_TOKEN_URL",
        "HERMES_CONFIG_BACKEND", "HERMES_CONFIG_INSTANCE_ID", "HERMES_CONFIG_REMOTE_URL",
        "HERMES_PORTAL_BASE_URL", "HERMES_SHARED_AUTH_DIR", "NOUS_PORTAL_BASE_URL",
    };
    json_t *o = json_object();
    json_t *keys = json_array();
    json_t *names = json_array();
    size_t i;

    for (i = 0; i < CRED_IDP_KEY_COUNT; i++) json_append(keys, json_string(CRED_IDP_KEYS[i]));
    for (i = 0; i < sizeof(sorted_names) / sizeof(sorted_names[0]); i++)
        json_append(names, json_string(sorted_names[i]));

    json_set(o, "idp_env_prefix", json_string(W26_CREDENTIAL_0A5825_IDP_ENV_PREFIX));
    json_set(o, "idp_keys", keys);
    json_set(o, "plane_credential_env_names", names);
    json_set(o, "http_timeout_s", json_number(W26_CREDENTIAL_0A5825_HTTP_TIMEOUT_S));
    return o;
}
