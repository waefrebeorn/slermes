/*
 * auth_openrouter.c — faithful C11 port of _openrouter_exchange_code from
 * hermes_cli/auth_openrouter.py (gate w13_authopenro_0d3257).
 * PoP: w13_authopenro_0d3257_exchange_code @ hermes_cli/auth_openrouter.py:_openrouter_exchange_code
 *
 * The live function's only I/O is httpx.post; that response (status, text,
 * parsed JSON / json() failure, and a transport exception message) is injected
 * so the error-code / message branching is replayed exactly.
 */
#include "noc_w13_authopenro_0d3257.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_BODY_LIMIT 2048

static char *strip_dup(const char *s) {
    if (!s) return strdup("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = s + strlen(s);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    memcpy(r, a, (size_t)(b - a));
    r[b - a] = '\0';
    return r;
}

static char *truncate_2048(const char *s) {
    size_t n = strlen(s);
    if (n > ERROR_BODY_LIMIT) n = ERROR_BODY_LIMIT;
    char *r = (char *)malloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static json_t *err_result(const char *code, const char *message, int relogin) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "key", json_null());
    json_set(o, "code", json_string(code ? code : ""));
    json_set(o, "message", json_string(message ? message : ""));
    json_set(o, "provider", json_string("openrouter"));
    json_set(o, "relogin", json_bool(relogin != 0));
    return o;
}

/* Python str(value) for the JSON scalar shapes fixtures use. */
static char *key_to_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9e15)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    }
    char *s = json_dumps(v, 0);
    return s ? s : strdup("");
}

json_t *w13_authopenro_0d3257_exchange_code(const char *code,
                                            const char *verifier,
                                            long long status,
                                            const char *text,
                                            const json_t *payload,
                                            int json_error,
                                            const char *post_error) {
    (void)code; (void)verifier;  /* transmitted in the (stubbed) POST body */

    if (post_error && post_error[0]) {
        size_t cap = strlen(post_error) + 64;
        char *m = (char *)malloc(cap);
        snprintf(m, cap, "OpenRouter code exchange failed: %s", post_error);
        json_t *r = err_result("openrouter_token_exchange_failed", m, 0);
        free(m);
        return r;
    }

    if (status == 403) {
        return err_result(
            "openrouter_token_exchange_denied",
            "OpenRouter rejected the authorization code (invalid, already used, or older than "
            "10 minutes). Run the login again.",
            1);
    }
    if (status >= 400) {
        char *stripped = strip_dup(text);
        char *detail = truncate_2048(stripped);
        free(stripped);
        size_t cap = strlen(detail) + 128;
        char *m = (char *)malloc(cap);
        if (detail[0])
            snprintf(m, cap, "OpenRouter code exchange failed (HTTP %lld). Response: %s",
                     status, detail);
        else
            snprintf(m, cap, "OpenRouter code exchange failed (HTTP %lld).", status);
        free(detail);
        json_t *r = err_result("openrouter_token_exchange_failed", m, 0);
        free(m);
        return r;
    }

    if (json_error) {
        return err_result("openrouter_token_exchange_invalid",
                          "OpenRouter code exchange returned a non-JSON body.", 0);
    }

    char *raw = NULL;
    if (payload && payload->type == JSON_OBJECT) {
        json_t *kv = json_obj_get(payload, "key");
        if (kv && kv->type != JSON_NULL) raw = key_to_str(kv);
    }
    char *key = raw ? strip_dup(raw) : strdup("");
    free(raw);
    if (!key[0]) {
        free(key);
        return err_result("openrouter_token_exchange_invalid",
                          "OpenRouter code exchange response did not include a 'key'.", 0);
    }

    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "key", json_string(key));
    json_set(o, "code", json_null());
    json_set(o, "message", json_null());
    json_set(o, "provider", json_null());
    json_set(o, "relogin", json_bool(false));
    free(key);
    return o;
}

