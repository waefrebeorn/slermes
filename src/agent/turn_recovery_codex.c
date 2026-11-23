/*
 * turn_recovery_codex.c — faithful C11 port of agent/turn_recovery_codex.py.
 * Gate w11_turnrecove_ba5e00.
 *
 * PoP: w11_turnrecove_ba5e00_codex_soft_failure_error @ agent/turn_recovery_codex.py:_codex_soft_failure_error
 * PoP: w11_turnrecove_ba5e00_is_codex_token_expired @ agent/turn_recovery_codex.py:_is_codex_token_expired
 * PoP: w11_turnrecove_ba5e00_classify_codex_soft_failure @ agent/turn_recovery_codex.py:classify_codex_soft_failure
 */
#include "hermes_json.h"
#include "noc_w11_turnrecove_ba5e00.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0'; /* Python bool("") == False */
        default: return v->c.count > 0;
    }
}

static int is_blank(const char *s) {
    if (!s) return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Python str() coercion for the scalar shapes used by response.error. */
static char *py_str(const json_t *v) {
    char buf[64];
    const char *s = NULL;
    if (!v) return NULL;
    switch (v->type) {
        case JSON_NULL:   s = "None"; break;
        case JSON_BOOL:   s = v->bool_val ? "True" : "False"; break;
        case JSON_STRING: s = v->str_val ? v->str_val : ""; break;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%.0f", v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            s = buf;
            break;
        default: return NULL;
    }
    {
        size_t n = strlen(s);
        char *out = (char *)malloc(n + 1);
        if (!out) return NULL;
        if (n) memcpy(out, s, n);
        out[n] = '\0';
        return out;
    }
}

json_t *w11_turnrecove_ba5e00_codex_soft_failure_error(const json_t *error) {
    json_t *out = json_object();
    json_t *fields = NULL;
    json_t *tmp = NULL;
    char *msg = NULL;
    if (!py_truthy(error)) return out;
    if (error->type == JSON_OBJECT) {
        fields = (json_t *)error;
    } else if ((error->type == JSON_STRING) || (error->type == JSON_NUMBER) ||
               (error->type == JSON_BOOL)) {
        msg = py_str(error);
        fields = tmp = json_object();
        json_set_hdr(tmp, "message", msg ? msg : "");
    }
    if (fields && fields->type == JSON_OBJECT) {
        size_t i;
        for (i = 0; i < json_object_size(fields); i++) {
            const char *k = json_object_get_key_at(fields, i);
            json_t *v = json_object_get_at(fields, i);
            if (k && v && v->type == JSON_STRING && v->str_val && !is_blank(v->str_val))
                json_set(out, k, json_copy(v));
        }
    }
    free(msg);
    json_free(tmp);
    return out;
}

int w11_turnrecove_ba5e00_is_codex_token_expired(const json_t *status_code, const char *reason) {
    char buf[256];
    size_t i, n;
    if (!status_code || status_code->type != JSON_NUMBER || status_code->num_val != 401.0) return 0;
    if (!reason) return 0;
    n = strlen(reason);
    if (n >= sizeof(buf)) return 0;
    for (i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)reason[i]);
    buf[n] = '\0';
    /* reason.strip().lower() == "token_expired" (trim ASCII whitespace) */
    {
        size_t a = 0, b = n;
        while (a < b && isspace((unsigned char)buf[a])) a++;
        while (b > a && isspace((unsigned char)buf[b - 1])) b--;
        return (b - a == strlen("token_expired")) && !memcmp(buf + a, "token_expired", b - a);
    }
}

json_t *w11_turnrecove_ba5e00_classify_codex_soft_failure(
    const char *api_mode, const char *status, const json_t *error,
    const char *provider, const char *model, const char *base_url, const json_t *api_key) {
    json_t *out = json_object(), *filtered, *classified;
    const char *s = status ? status : "";
    size_t a, b, n;
    char norm[128];
    json_t *cv;

    json_set(out, "classified", json_null());
    json_set(out, "error_context", json_object());
    if (!api_mode || strcmp(api_mode, "codex_responses")) return out;
    n = strlen(s);
    if (n >= sizeof(norm)) return out;
    memcpy(norm, s, n); norm[n] = '\0';
    for (a = 0; a < n; a++) norm[a] = (char)tolower((unsigned char)norm[a]);
    a = 0; b = n;
    while (a < b && isspace((unsigned char)norm[a])) a++;
    while (b > a && isspace((unsigned char)norm[b - 1])) b--;
    if (!(b - a == strlen("failed") && !memcmp(norm + a, "failed", b - a)) &&
        !(b - a == strlen("cancelled") && !memcmp(norm + a, "cancelled", b - a)))
        return out;

    filtered = w11_turnrecove_ba5e00_codex_soft_failure_error(error);
    classified = json_object();
    cv = json_obj_get(filtered, "code");
    json_set(classified, "code", cv && cv->type == JSON_STRING ? json_copy(cv) : json_null());
    cv = json_obj_get(filtered, "message");
    json_set(classified, "message", cv && cv->type == JSON_STRING ? json_copy(cv) : json_null());
    json_set_hdr(classified, "provider", provider ? provider : "");
    json_set_hdr(classified, "model", model ? model : "");
    json_set_hdr(classified, "base_url", base_url ? base_url : "");
    json_set(classified, "api_key", api_key ? json_copy(api_key) : json_null());
    json_set(out, "classified", classified);
    {
        json_t *ctx = json_object();
        json_set(ctx, "error", json_copy(filtered));
        json_set(out, "error_context", ctx);
    }
    json_free(filtered);
    return out;
}

