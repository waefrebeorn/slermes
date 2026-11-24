/*
 * client.c - C11 port of upstream plugins/config_backends/remote/client.py
 * (gate w24_client_64e0a3).
 *
 * PoP: w24_client_64e0a3_parse_body @ plugins/config_backends/remote/client.py:_parse_body
 * PoP: w24_client_64e0a3_retry_after @ plugins/config_backends/remote/client.py:_retry_after
 * PoP: w24_client_64e0a3_base_url @ plugins/config_backends/remote/client.py:base_url
 * PoP: w24_client_64e0a3_instance_id @ plugins/config_backends/remote/client.py:instance_id
 * PoP: w24_client_64e0a3_response_error @ plugins/config_backends/remote/client.py:Response.error
 * PoP: w24_client_64e0a3_response_message @ plugins/config_backends/remote/client.py:Response.message
 * PoP: w24_client_64e0a3_constants @ plugins/config_backends/remote/client.py:<module constants>
 *
 * The transport is a seam: urllib, the TLS opener, the bearer store and the
 * credential-destination policy are never entered here, so nothing opens a
 * socket, reads auth.json or touches the environment. Every function is a pure
 * value->value transformation of what the transport hands back: the body bytes,
 * the Retry-After header, the two env vars, a status plus its parsed body.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "noc_w24_client_64e0a3.h"

/* Module-level literal constants mirrored from the Python source. */
static const char *const CLI_DEFAULT_URL = "https://config-config.nousresearch.com";
static const char *const CLI_URL_ENV = "HERMES_CONFIG_REMOTE_URL";
static const char *const CLI_INSTANCE_ENV = "HERMES_CONFIG_INSTANCE_ID";
static const double CLI_HTTP_TIMEOUT_S = 15.0;

static const json_t *cli_arg(const json_t *req, const char *key) {
    return req && req->type == JSON_OBJECT ? json_obj_get(req, key) : NULL;
}

/* Python truthiness of a JSON value. */
static int cli_truthy(const json_t *v) {
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

static int cli_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over the ASCII whitespace set. */
static char *cli_strip(const char *s) {
    const char *a, *b;
    char *out;
    size_t n;
    if (!s) s = "";
    a = s;
    while (*a && cli_isspace(*a)) a++;
    b = a + strlen(a);
    while (b > a && cli_isspace(b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* str(value) for the scalar shapes a header value / body field can hold. */
static char *cli_str(const json_t *v) {
    char buf[64];
    const char *s;
    char *dup;
    if (!v || v->type == JSON_NULL) s = "None";
    else if (v->type == JSON_STRING) s = v->str_val ? v->str_val : "";
    else if (v->type == JSON_BOOL) s = v->bool_val ? "True" : "False";
    else if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        s = buf;
    } else {
        return NULL;
    }
    dup = (char *)malloc(strlen(s) + 1);
    if (dup) memcpy(dup, s, strlen(s) + 1);
    return dup;
}

json_t *w24_client_64e0a3_parse_body(const json_t *req) {
    const json_t *text = cli_arg(req, "text");
    const char *raw = (text && text->type == JSON_STRING && text->str_val) ? text->str_val : "";
    json_t *parsed;

    if (raw[0] == '\0') return json_object();       /* if not raw: return {} */
    parsed = json_parse(raw, NULL);                 /* ValueError -> {} */
    if (!parsed) return json_object();
    if (parsed->type != JSON_OBJECT) {              /* not a dict -> {} */
        json_free(parsed);
        return json_object();
    }
    return parsed;
}

json_t *w24_client_64e0a3_retry_after(const json_t *req) {
    const json_t *value = cli_arg(req, "value");
    char *text, *end;
    double out;

    if (!cli_truthy(value)) return json_null();     /* if value else None */
    if (value->type == JSON_BOOL) return json_number(value->bool_val ? 1.0 : 0.0);
    if (value->type == JSON_NUMBER) return json_number(value->num_val);
    if (value->type != JSON_STRING || !value->str_val) return json_null();

    text = cli_strip(value->str_val);
    if (!text) return json_null();
    if (text[0] == '\0') { free(text); return json_null(); }
    /* float() has no hex-float form; strtod() does. */
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) { free(text); return json_null(); }
    errno = 0;
    end = NULL;
    out = strtod(text, &end);
    if (end == text) { free(text); return json_null(); }
    while (*end && cli_isspace(*end)) end++;
    if (*end != '\0') { free(text); return json_null(); }   /* ValueError -> None */
    free(text);
    return json_number(out);
}

json_t *w24_client_64e0a3_base_url(const json_t *req) {
    const json_t *env = cli_arg(req, "env");
    const char *raw = (env && env->type == JSON_STRING && env->str_val) ? env->str_val : NULL;
    char *stripped, *base;
    size_t n;
    json_t *out;

    stripped = cli_strip(raw);                      /* os.environ.get(URL_ENV, "") */
    if (!stripped) return json_string(CLI_DEFAULT_URL);
    base = stripped[0] ? stripped : NULL;           /* .strip() or DEFAULT_URL */
    n = base ? strlen(base) : strlen(CLI_DEFAULT_URL);
    if (!base) {
        free(stripped);
        return json_string(CLI_DEFAULT_URL);
    }
    while (n > 0 && base[n - 1] == '/') n--;        /* .rstrip("/") */
    {
        char *tmp = (char *)malloc(n + 1);
        if (!tmp) { free(stripped); return json_string(CLI_DEFAULT_URL); }
        memcpy(tmp, base, n);
        tmp[n] = '\0';
        out = json_string(tmp);
        free(tmp);
    }
    free(stripped);
    return out;
}

json_t *w24_client_64e0a3_instance_id(const json_t *req) {
    const json_t *env = cli_arg(req, "env");
    const char *raw = (env && env->type == JSON_STRING && env->str_val) ? env->str_val : NULL;
    char *stripped;
    json_t *out;

    stripped = cli_strip(raw);
    if (!stripped) return json_string("");
    out = json_string(stripped);
    free(stripped);
    return out;
}

/* str(body.get(field) or "<prefix><status>") */
static json_t *cli_response_field(const json_t *req, const char *field, const char *prefix) {
    const json_t *body = cli_arg(req, "body");
    const json_t *status = cli_arg(req, "status");
    const json_t *v = (body && body->type == JSON_OBJECT) ? json_obj_get(body, field) : NULL;
    char *text;
    json_t *out;

    if (cli_truthy(v)) {
        text = cli_str(v);
        if (!text) return json_null();
        out = json_string(text);
        free(text);
        return out;
    }
    text = cli_str(status);
    if (!text) return json_null();
    {
        size_t need = strlen(prefix) + strlen(text) + 1;
        char *buf = (char *)malloc(need);
        if (!buf) { free(text); return json_null(); }
        snprintf(buf, need, "%s%s", prefix, text);
        out = json_string(buf);
        free(buf);
        free(text);
    }
    return out;
}

json_t *w24_client_64e0a3_response_error(const json_t *req) {
    return cli_response_field(req, "error", "http_");
}

json_t *w24_client_64e0a3_response_message(const json_t *req) {
    return cli_response_field(req, "message", "HTTP ");
}

json_t *w24_client_64e0a3_constants(void) {
    json_t *o = json_object();
    json_set(o, "default_url", json_string(CLI_DEFAULT_URL));
    json_set(o, "url_env", json_string(CLI_URL_ENV));
    json_set(o, "instance_env", json_string(CLI_INSTANCE_ENV));
    json_set(o, "http_timeout_s", json_number(CLI_HTTP_TIMEOUT_S));
    return o;
}
