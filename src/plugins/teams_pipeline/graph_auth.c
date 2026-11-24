/*
 * graph_auth.c - real C11 port of the Microsoft Graph app-only credential/token helpers in
 * upstream Hermes plugins/teams_pipeline/graph_auth.py. The httpx token POST stays Python-side;
 * every decision around it (env normalization, expiry arithmetic, health, error rendering) is here.
 * PoP: w19_graphauth_31d6da_format_graph_error @ plugins/teams_pipeline/graph_auth.py:format_graph_error
 * PoP: w19_graphauth_31d6da_credentials_from_env @ plugins/teams_pipeline/graph_auth.py:GraphCredentials.from_env
 * PoP: w19_graphauth_31d6da_token_url @ plugins/teams_pipeline/graph_auth.py:GraphCredentials.token_url
 * PoP: w19_graphauth_31d6da_token_is_expired @ plugins/teams_pipeline/graph_auth.py:CachedAccessToken.is_expired
 * PoP: w19_graphauth_31d6da_token_expires_in @ plugins/teams_pipeline/graph_auth.py:CachedAccessToken.expires_in_seconds
 * PoP: w19_graphauth_31d6da_fresh_cached @ plugins/teams_pipeline/graph_auth.py:MicrosoftGraphTokenProvider._fresh_cached
 * PoP: w19_graphauth_31d6da_inspect_health @ plugins/teams_pipeline/graph_auth.py:MicrosoftGraphTokenProvider.inspect_token_health
 * PoP: w19_graphauth_31d6da_extract_error_detail @ plugins/teams_pipeline/graph_auth.py:_extract_error_detail
 */
#include "noc_w19_graphauth_31d6da.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DEFAULT_GRAPH_SCOPE = "https://graph.microsoft.com/.default";
static const char *const DEFAULT_GRAPH_AUTHORITY_URL = "https://login.microsoftonline.com";
static const int DEFAULT_TOKEN_SKEW_SECONDS = 120;

static const char *const REQUIRED_ENV[] = {
    "MSGRAPH_TENANT_ID", "MSGRAPH_CLIENT_ID", "MSGRAPH_CLIENT_SECRET", NULL
};

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int is_py_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip(). NULL stays NULL; the empty string stays "". */
static char *pystrip(const char *s) {
    const char *a;
    const char *b;
    char *out;
    size_t n;
    if (!s) return dupstr("");
    a = s;
    while (*a && is_py_space(*a)) a++;
    b = a + strlen(a);
    while (b > a && is_py_space(b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* Python str(v) for the scalar JSON values these helpers can see. */
static char *py_str_node(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        long long i = (long long)v->num_val;
        if ((double)i == v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", i);
        } else {
            snprintf(buf, sizeof(buf), "%.12g", v->num_val);
        }
        return dupstr(buf);
    }
    return NULL; /* containers: their str() repr is not part of this seam */
}

/* Python truthiness of a JSON scalar/container. */
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

char *w19_graphauth_31d6da_format_graph_error(const json_t *error) {
    json_t *code;
    json_t *message;
    if (!error) return NULL;
    if (error->type == JSON_STRING) return dupstr(error->str_val ? error->str_val : "");
    if (error->type != JSON_OBJECT) return NULL;
    code = json_obj_get(error, "code");
    message = json_obj_get(error, "message");
    if (truthy(code) && truthy(message)) {
        char *cs = py_str_node(code);
        char *ms = py_str_node(message);
        char *out = NULL;
        if (cs && ms) {
            size_t n = strlen(cs) + strlen(ms) + 3;
            out = (char *)malloc(n);
            if (out) snprintf(out, n, "%s: %s", cs, ms);
        }
        free(cs);
        free(ms);
        return out;
    }
    if (truthy(message)) return py_str_node(message);
    return NULL;
}

json_t *w19_graphauth_31d6da_credentials_from_env(const json_t *env_map, int required) {
    char *values[3];
    const char *missing[4];
    int n_missing = 0;
    int i;
    json_t *out;

    for (i = 0; i < 3; i++) {
        json_t *v = (env_map && env_map->type == JSON_OBJECT) ? json_obj_get(env_map, REQUIRED_ENV[i]) : NULL;
        char *raw = py_str_node(v);
        char *stripped;
        if (!v || v->type == JSON_NULL) { free(raw); raw = dupstr(""); }
        stripped = pystrip(raw);
        free(raw);
        values[i] = stripped;
        if (!stripped || !stripped[0]) missing[n_missing++] = REQUIRED_ENV[i];
    }

    if (n_missing && !required) {
        for (i = 0; i < 3; i++) free(values[i]);
        return json_null();
    }
    if (n_missing) {
        char buf[256];
        size_t off;
        snprintf(buf, sizeof(buf), "Missing Microsoft Graph configuration: ");
        off = strlen(buf);
        for (i = 0; i < n_missing; i++) {
            size_t n = strlen(missing[i]);
            if (off + n + 3 >= sizeof(buf)) break;
            if (i) { buf[off++] = ','; buf[off++] = ' '; }
            memcpy(buf + off, missing[i], n);
            off += n;
        }
        buf[off] = '\0';
        for (i = 0; i < 3; i++) free(values[i]);
        out = json_object();
        json_set(out, "error", json_string(buf));
        return out;
    }

    out = json_object();
    json_set(out, "tenant_id", json_string(values[0]));
    json_set(out, "client_id", json_string(values[1]));
    json_set(out, "client_secret", json_string(values[2]));
    {
        json_t *sv = (env_map && env_map->type == JSON_OBJECT) ? json_obj_get(env_map, "MSGRAPH_SCOPE") : NULL;
        json_t *av = (env_map && env_map->type == JSON_OBJECT) ? json_obj_get(env_map, "MSGRAPH_AUTHORITY_URL") : NULL;
        char *sraw = truthy(sv) ? py_str_node(sv) : dupstr(DEFAULT_GRAPH_SCOPE);
        char *araw = truthy(av) ? py_str_node(av) : dupstr(DEFAULT_GRAPH_AUTHORITY_URL);
        char *scope = pystrip(sraw);
        char *authority = pystrip(araw);
        json_set(out, "scope", json_string(scope ? scope : ""));
        json_set(out, "authority_url", json_string(authority ? authority : ""));
        free(sraw);
        free(araw);
        free(scope);
        free(authority);
    }
    for (i = 0; i < 3; i++) free(values[i]);
    return out;
}

char *w19_graphauth_31d6da_token_url(const char *authority_url, const char *tenant_id) {
    char *authority = pystrip(authority_url ? authority_url : "");
    char *tenant = pystrip(tenant_id ? tenant_id : "");
    size_t a_len;
    char *out;
    size_t n;
    size_t t_start = 0, t_end;
    size_t tl;

    if (!authority || !tenant) { free(authority); free(tenant); return NULL; }
    /* authority_url.rstrip("/") */
    a_len = strlen(authority);
    while (a_len > 0 && authority[a_len - 1] == '/') a_len--;
    authority[a_len] = '\0';
    /* tenant_id.strip().strip("/") */
    while (tenant[t_start] == '/') t_start++;
    t_end = strlen(tenant);
    while (t_end > t_start && tenant[t_end - 1] == '/') t_end--;
    tl = t_end - t_start;

    n = a_len + 1 + tl + strlen("/oauth2/v2.0/token") + 1;
    out = (char *)malloc(n);
    if (out) snprintf(out, n, "%s/%.*s/oauth2/v2.0/token", authority, (int)tl, tenant + t_start);
    free(authority);
    free(tenant);
    return out;
}

int w19_graphauth_31d6da_token_is_expired(double expires_at, double now, int skew_seconds) {
    int skew = skew_seconds < 0 ? 0 : skew_seconds;
    return expires_at <= (now + (double)skew) ? 1 : 0;
}

json_t *w19_graphauth_31d6da_token_expires_in(double expires_at, double now) {
    long long v = (long long)(expires_at - now);  /* int() truncates toward zero */
    if (v < 0) v = 0;
    return json_number((double)v);
}

int w19_graphauth_31d6da_fresh_cached(int has_cached, double expires_at, double now, int skew_seconds) {
    if (!has_cached) return 0;
    return w19_graphauth_31d6da_token_is_expired(expires_at, now, skew_seconds) ? 0 : 1;
}

json_t *w19_graphauth_31d6da_inspect_health(const json_t *credentials, int has_cached,
                                            double expires_at, double now, int skew_seconds) {
    json_t *out = json_object();
    const char *tenant = "";
    const char *client = "";
    const char *scope = DEFAULT_GRAPH_SCOPE;
    const char *authority = DEFAULT_GRAPH_AUTHORITY_URL;
    char *url;
    int skew = skew_seconds < 0 ? 0 : skew_seconds;

    if (credentials && credentials->type == JSON_OBJECT) {
        json_t *v;
        v = json_obj_get(credentials, "tenant_id");
        if (v && v->type == JSON_STRING && v->str_val) tenant = v->str_val;
        v = json_obj_get(credentials, "client_id");
        if (v && v->type == JSON_STRING && v->str_val) client = v->str_val;
        v = json_obj_get(credentials, "scope");
        if (v && v->type == JSON_STRING && v->str_val) scope = v->str_val;
        v = json_obj_get(credentials, "authority_url");
        if (v && v->type == JSON_STRING && v->str_val) authority = v->str_val;
    }
    url = w19_graphauth_31d6da_token_url(authority, tenant);

    json_set(out, "configured", json_bool(1));
    json_set(out, "tenant_id", json_string(tenant));
    json_set(out, "client_id", json_string(client));
    json_set(out, "scope", json_string(scope));
    json_set(out, "authority_url", json_string(authority));
    json_set(out, "token_url", json_string(url ? url : ""));
    json_set(out, "cached", json_bool(has_cached ? 1 : 0));
    json_set(out, "expires_in_seconds",
             has_cached ? w19_graphauth_31d6da_token_expires_in(expires_at, now) : json_null());
    json_set(out, "is_expired",
             has_cached ? json_bool(w19_graphauth_31d6da_token_is_expired(expires_at, now, 0) ? 1 : 0)
                        : json_null());
    json_set(out, "refresh_skew_seconds", json_number((double)skew));
    free(url);
    return out;
}

char *w19_graphauth_31d6da_extract_error_detail(int is_json, const json_t *payload, const char *text) {
    json_t *desc;
    json_t *error;
    char *detail;

    if (!is_json) {
        char *stripped = pystrip(text ? text : "");
        if (!stripped) return dupstr("unknown error");
        if (!stripped[0]) { free(stripped); return dupstr("unknown error"); }
        return stripped;
    }
    if (!payload || payload->type != JSON_OBJECT) return py_str_node(payload);

    desc = json_obj_get(payload, "error_description");
    if (desc && desc->type == JSON_STRING) return dupstr(desc->str_val ? desc->str_val : "");

    error = json_obj_get(payload, "error");
    detail = w19_graphauth_31d6da_format_graph_error(error);
    if (detail) return detail;

    if (error && error->type == JSON_OBJECT && truthy(json_obj_get(error, "code")))
        return py_str_node(json_obj_get(error, "code"));
    return py_str_node(payload);
}
