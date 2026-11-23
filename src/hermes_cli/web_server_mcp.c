/*
 * web_server_mcp.c — C home for upstream Hermes hermes_cli/web_server_mcp.py.
 *
 * PoP: w12_webserver_3794af_normalize_mcp_server_create @ hermes_cli/web_server_mcp.py:normalize_mcp_server_create
 * PoP: w12_webserver_3794af_redact_mcp_env              @ hermes_cli/web_server_mcp.py:_redact_mcp_env
 *
 * The dashboard MCP create contract and env redaction.  The three tiny
 * collaborators (_strip_bearer_prefix / _bearer_auth_headers via
 * _env_key_for_server, and mask_secret behind redact_key) are reproduced
 * verbatim; validate_mcp_server_entry is a no-issues seam in the oracle.
 */
#include "noc_w12_webserver_3794af.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

static char lower_c(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace, bytes copied out first. */
static char *py_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && is_ws(s[a])) a++;
    while (b > a && is_ws(s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}
static char *py_lower(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (out) { for (size_t i = 0; i < n; ++i) out[i] = lower_c(s[i]); out[n] = '\0'; }
    return out;
}
static int json_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY) return json_len(v) ? 1 : 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 0;
}

/* _strip_bearer_prefix: strip(), then drop a leading "bearer " (7 chars). */
static char *strip_bearer_prefix(const char *token) {
    char *s = py_strip(token);
    if (!s) return NULL;
    /* case-insensitive first 7 chars == "bearer " */
    if (strlen(s) >= 7) {
        char first[8];
        for (int i = 0; i < 7; ++i) first[i] = lower_c(s[i]);
        first[7] = '\0';
        if (!strcmp(first, "bearer ")) {
            char *rest = py_strip(s + 7);
            free(s);
            return rest;
        }
    }
    return s;
}

/* _env_key_for_server + _bearer_auth_headers:
 *   re.sub(r"[^A-Za-z0-9_]", "_", name.upper()).strip("_") -> MCP_<...>_API_KEY */
static char *env_key_for_server(const char *name) {
    size_t n = strlen(name);
    char *raw = (char *)malloc(n + 1);
    if (!raw) return NULL;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)name[i];
        char up = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
        raw[i] = ((up >= 'A' && up <= 'Z') || (c >= '0' && c <= '9') || c == '_') ? up : '_';
    }
    raw[n] = '\0';
    size_t a = 0, b = strlen(raw);
    while (a < b && raw[a] == '_') a++;
    while (b > a && raw[b - 1] == '_') b--;
    size_t len = b - a;
    char *out = (char *)malloc(len + 14 + 1); /* "MCP_" (4) + key + "_API_KEY" (8) + NUL */
    if (out) {
        memcpy(out, "MCP_", 4);
        memcpy(out + 4, raw + a, len);
        memcpy(out + 4 + len, "_API_KEY", 8);
        out[4 + len + 8] = '\0';
    }
    free(raw);
    return out;
}

static json_t *bearer_auth_headers(const char *name) {
    json_t *h = json_object();
    char *key = env_key_for_server(name);
    if (key) {
        size_t n = strlen(key) + 20;
        char *val = (char *)malloc(n);
        if (val) {
            snprintf(val, n, "Bearer ${%s}", key);
            json_set(h, "Authorization", json_string(val));
            free(val);
        }
        free(key);
    }
    return h;
}

static json_t *err_obj(const char *msg) {
    json_t *e = json_object();
    json_set(e, "error", json_string(msg));
    return e;
}

json_t *w12_webserver_3794af_normalize_mcp_server_create(const struct json_t *body) {
    if (!body) return err_obj("Server name is required");

    const json_t *jn = json_obj_get(body, "name");
    char *name = py_strip(jn && jn->type == JSON_STRING ? jn->str_val : "");
    if (!name || !name[0]) { free(name); return err_obj("Server name is required"); }

    const json_t *ju = json_obj_get(body, "url");
    const json_t *jc = json_obj_get(body, "command");
    char *url = py_strip(ju && ju->type == JSON_STRING ? ju->str_val : "");
    char *command = py_strip(jc && jc->type == JSON_STRING ? jc->str_val : "");

    const json_t *ja = json_obj_get(body, "auth");
    char *auth_raw = py_strip(ja && ja->type == JSON_STRING && ja->str_val && ja->str_val[0]
                              ? ja->str_val : "none");
    char *auth = py_lower(auth_raw);
    free(auth_raw);

    const json_t *jt = json_obj_get(body, "bearer_token");
    char *token = (jt && jt->type == JSON_STRING) ? strdup(jt->str_val) : NULL;

    const json_t *args = json_obj_get(body, "args");
    const json_t *env = json_obj_get(body, "env");
    int has_url = url && url[0];
    int has_command = command && command[0];
    json_t *result = NULL;
    json_t *cfg = NULL;

    if (has_url == has_command) {
        result = err_obj("Provide exactly one of URL (HTTP/SSE) or command (stdio)");
        goto done;
    }
    if (strcmp(auth, "none") && strcmp(auth, "header") && strcmp(auth, "oauth")) {
        size_t n = strlen(auth) + 32;
        char *msg = (char *)malloc(n);
        if (msg) { snprintf(msg, n, "Unsupported auth mode: %s", auth); result = err_obj(msg); free(msg); }
        else result = err_obj("Unsupported auth mode");
        goto done;
    }

    cfg = json_object();
    if (has_url) {
        if (json_truthy(args)) { result = err_obj("Arguments are only supported for stdio MCP servers"); goto done; }
        if (json_truthy(env)) { result = err_obj("Environment variables are only supported for stdio MCP servers"); goto done; }
        if (!strcmp(auth, "header")) {
            char *normalized = token ? strip_bearer_prefix(token) : strdup("");
            char *low = py_lower(normalized ? normalized : "");
            if (!normalized || !normalized[0] || !strcmp(low, "bearer")) {
                free(normalized); free(low);
                result = err_obj("Bearer token is required");
                goto done;
            }
            free(low);
            json_set(cfg, "headers", bearer_auth_headers(name));
            free(normalized);
        } else if (token) {
            result = err_obj("Bearer token requires header authentication");
            goto done;
        }
        json_set(cfg, "url", json_string(url));
        if (!strcmp(auth, "oauth")) json_set(cfg, "auth", json_string("oauth"));
    } else {
        if (strcmp(auth, "none") || token) {
            result = err_obj("HTTP authentication is not supported for stdio MCP servers");
            goto done;
        }
        json_set(cfg, "command", json_string(command));
        if (json_truthy(args)) json_set(cfg, "args", json_copy(args));
        if (json_truthy(env)) json_set(cfg, "env", json_copy(env));
    }

    /* validate_mcp_server_entry(name, cfg) -> [] in this port's seam. */
    result = json_object();
    json_set(result, "name", json_string(name));
    json_set(result, "config", cfg);
    json_set(result, "token", token ? json_string(token) : json_null());
    cfg = NULL;
    goto done;

done:
    json_free(cfg);
    free(name); free(url); free(command); free(auth); free(token);
    return result;
}

/* mask_secret(value, head=4, tail=4, floor=12, placeholder="***", empty="(not set)") */
static char *mask_secret(const char *value) {
    size_t n = value ? strlen(value) : 0;
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    size_t k = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c < 0x20 || c == 0x7f) continue;  /* ASCII display controls */
        s[k++] = value[i];
    }
    s[k] = '\0';
    if (k == 0) { free(s); return strdup("(not set)"); }
    if (k < 12) { free(s); return strdup("***"); }
    char *out = (char *)malloc(4 + 3 + 4 + 1);
    if (out) {
        memcpy(out, s, 4);
        memcpy(out + 4, "...", 3);
        memcpy(out + 7, s + k - 4, 4);
        out[11] = '\0';
    }
    free(s);
    return out;
}

/* Python str(v) for the value shapes used by the dashboard env map. */
static char *py_str_value(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(buf, sizeof(buf), "%lld", (long long)d);
        else snprintf(buf, sizeof(buf), "%g", d);
        return strdup(buf);
    }
    return strdup("");
}

json_t *w12_webserver_3794af_redact_mcp_env(const struct json_t *env) {
    json_t *out = json_object();
    if (!env || env->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(env); ++i) {
        const char *k = json_object_get_key_at(env, i);
        json_t *v = json_object_get_at(env, i);
        if (!k) continue;
        char *val;
        if (json_truthy(v)) {
            char *sv = py_str_value(v);
            val = mask_secret(sv ? sv : "");
            free(sv);
        } else {
            val = strdup("");
        }
        json_set(out, k, json_string(val ? val : ""));
        free(val);
    }
    return out;
}
