/*
 * api_request_hooks.c — C home for upstream Hermes agent/api_request_hooks.py.
 *
 * Faithful C11 ports of the JSON-safe, secret-redacting, size-capped helpers
 * used to build API-request lifecycle hook payloads. Every exported function
 * reads its arguments from one json_t * object and returns a NEW json_t *.
 *
 * PoP: w9_apirequest_302923_hook_payload_max_chars @ agent/api_request_hooks.py:_hook_payload_max_chars
 * PoP: w9_apirequest_302923_is_sensitive_hook_key @ agent/api_request_hooks.py:_is_sensitive_hook_key
 * PoP: w9_apirequest_302923_hook_jsonable @ agent/api_request_hooks.py:_hook_jsonable
 * PoP: w9_apirequest_302923_sanitize_hook_payload @ agent/api_request_hooks.py:_sanitize_hook_payload
 * PoP: w9_apirequest_302923_request_payload @ agent/api_request_hooks.py:_api_request_payload_for_hook
 */
#include "noc_w9_apirequest_302923.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long long py_int_value(const json_t *v, int *ok)
{
    *ok = 0;
    if (!v) return 0;
    if (json_is_bool(v)) { *ok = 1; return json_bool_value(v) ? 1 : 0; }
    if (json_is_number(v)) { *ok = 1; return (long long)json_number_value(v); }
    if (!json_is_string(v)) return 0;
    const char *s = json_string_value(v);
    if (!s) return 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    if (!*s) return 0;
    long long n = 0;
    int lastdigit = 0;
    for (; *s; s++) {
        if (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); lastdigit = 1; }
        else if (*s == '_' && lastdigit) { lastdigit = 0; }
        else { return 0; }
    }
    if (!lastdigit) return 0;
    *ok = 1;
    return sign * n;
}

static char ascii_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

/* ApiRequestHooksMixin._is_sensitive_hook_key (staticmethod). */
static int is_sensitive(const char *key)
{
    if (!key) return 0;
    char buf[512];
    size_t i;
    for (i = 0; key[i] && i + 1 < sizeof(buf); i++) {
        char c = key[i];
        buf[i] = (c == '-') ? '_' : ascii_lower(c);
    }
    buf[i] = '\0';
    static const char *const seeds[] = {
        "api_key", "authorization", "proxy_authorization", "cookie", "set_cookie"
    };
    for (size_t k = 0; k < sizeof(seeds) / sizeof(seeds[0]); k++)
        if (strcmp(buf, seeds[k]) == 0) return 1;
    size_t n = strlen(buf);
    if (n >= 8 && strcmp(buf + n - 8, "_api_key") == 0) return 1;
    return 0;
}

static int in_obj(const json_t *obj, const char *key, const char *const *set, size_t n)
{
    if (!key) return 0;
    const json_t *v = json_obj_get(obj, key);
    if (!json_is_string(v)) return 0;
    const char *s = json_string_value(v);
    for (size_t i = 0; i < n; i++) if (s && strcmp(s, set[i]) == 0) return 1;
    return 0;
}

static const char *const EXCLUDED_BODY_KEYS[] = {"timeout", "http_client"};

static const char *py_typename(const json_t *v)
{
    if (json_is_null(v)) return "NoneType";
    if (json_is_bool(v)) return "bool";
    if (json_is_number(v)) {
        double d = json_number_value(v);
        return d == (double)(long long)d ? "int" : "float";
    }
    if (json_is_string(v)) return "str";
    if (json_is_array(v)) return "list";
    if (json_is_object(v)) return "dict";
    return "NoneType";
}

static json_t *hook_jsonable_core(json_t *value, int depth, int max_depth,
                                  int max_string, int max_sequence);

static json_t *hook_jsonable_recurse(json_t *item, int depth, int max_depth,
                                     int max_string, int max_sequence)
{
    return hook_jsonable_core(item, depth + 1, max_depth, max_string, max_sequence);
}

static json_t *hook_jsonable_core(json_t *value, int depth, int max_depth,
                                  int max_string, int max_sequence)
{
    if (depth > max_depth) {
        char b[64];
        snprintf(b, sizeof(b), "<%s depth limit>", py_typename(value));
        return json_string(b);
    }
    if (!value || json_is_null(value)) return json_null();
    if (json_is_bool(value)) return json_bool(json_bool_value(value));
    if (json_is_number(value)) return json_number(json_number_value(value));
    if (json_is_string(value)) {
        const char *s = json_string_value(value) ? json_string_value(value) : "";
        int n = (int)strlen(s);
        if (n > max_string) {
            char b[64];
            snprintf(b, sizeof(b), "...[truncated %d chars]", n - max_string);
            size_t keep = (size_t)max_string;
            char *out = (char *)malloc(keep + sizeof(b) + 1);
            if (!out) return json_string("");
            memcpy(out, s, keep);
            memcpy(out + keep, b, strlen(b) + 1);
            json_t *r = json_string(out);
            free(out);
            return r;
        }
        return json_string(s);
    }
    if (json_is_object(value)) {
        json_t *out = json_object();
        size_t n = json_object_size(value);
        for (size_t idx = 0; idx < n; idx++) {
            if ((int)idx >= max_sequence) {
                json_set(out, "_truncated_items", json_number((double)((int)n - max_sequence)));
                break;
            }
            const char *k = json_object_get_key_at(value, idx) ? json_object_get_key_at(value, idx) : "";
            json_t *item = json_object_get_at(value, idx);
            if (is_sensitive(k)) json_set(out, k, json_string("<redacted>"));
            else json_set(out, k, hook_jsonable_recurse(item, depth, max_depth, max_string, max_sequence));
        }
        return out;
    }
    if (json_is_array(value)) {
        json_t *out = json_array();
        size_t n = json_len(value);
        size_t take = n < (size_t)max_sequence ? n : (size_t)max_sequence;
        for (size_t idx = 0; idx < take; idx++)
            json_append(out, hook_jsonable_recurse(json_get(value, idx), depth, max_depth, max_string, max_sequence));
        if ((int)n > max_sequence) {
            json_t *t = json_object();
            json_set(t, "_truncated_items", json_number((double)((int)n - max_sequence)));
            json_append(out, t);
        }
        return out;
    }
    return json_null(); /* unreachable for JSON input */
}

/* Python json.dumps(obj, ensure_ascii=False) with default separators ', ' / ': '. */
static void py_dump(const json_t *v, char *buf, size_t cap, size_t *pos)
{
    if (*pos >= cap) return;
    if (!v || json_is_null(v)) {
        snprintf(buf + *pos, cap - *pos, "null"); *pos += 4; return;
    }
    if (json_is_bool(v)) {
        const char *s = json_bool_value(v) ? "true" : "false";
        size_t n = strlen(s); if (*pos + n >= cap) n = cap - *pos - 1;
        memcpy(buf + *pos, s, n); *pos += n; return;
    }
    if (json_is_number(v)) {
        double d = json_number_value(v);
        char t[64];
        if (d == (double)(long long)d) snprintf(t, sizeof(t), "%lld", (long long)d);
        else snprintf(t, sizeof(t), "%.17g", d);
        size_t n = strlen(t); if (*pos + n >= cap) n = cap - *pos - 1;
        memcpy(buf + *pos, t, n); *pos += n; return;
    }
    if (json_is_string(v)) {
        const char *s = json_string_value(v) ? json_string_value(v) : "";
        buf[(*pos)++] = '"';
        for (const char *p = s; *p && *pos + 6 < cap; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"') { buf[(*pos)++] = '\\'; buf[(*pos)++] = '"'; }
            else if (c == '\\') { buf[(*pos)++] = '\\'; buf[(*pos)++] = '\\'; }
            else if (c == '\n') { buf[(*pos)++] = '\\'; buf[(*pos)++] = 'n'; }
            else if (c == '\r') { buf[(*pos)++] = '\\'; buf[(*pos)++] = 'r'; }
            else if (c == '\t') { buf[(*pos)++] = '\\'; buf[(*pos)++] = 't'; }
            else if (c < 0x20) { snprintf(buf + *pos, 8, "\\u%04x", c); *pos += 6; }
            else buf[(*pos)++] = (char)c;
        }
        if (*pos < cap) buf[(*pos)++] = '"';
        return;
    }
    if (json_is_array(v)) {
        buf[(*pos)++] = '[';
        size_t n = json_len(v);
        for (size_t i = 0; i < n; i++) {
            if (i) { if (*pos + 2 < cap) { buf[(*pos)++] = ','; buf[(*pos)++] = ' '; } }
            py_dump(json_get(v, i), buf, cap, pos);
        }
        if (*pos < cap) buf[(*pos)++] = ']';
        return;
    }
    if (json_is_object(v)) {
        buf[(*pos)++] = '{';
        size_t n = json_object_size(v);
        for (size_t i = 0; i < n; i++) {
            if (i) { if (*pos + 2 < cap) { buf[(*pos)++] = ','; buf[(*pos)++] = ' '; } }
            py_dump(json_string(json_object_get_key_at(v, i)), buf, cap, pos);
            if (*pos + 2 < cap) { buf[(*pos)++] = ':'; buf[(*pos)++] = ' '; }
            py_dump(json_object_get_at(v, i), buf, cap, pos);
        }
        if (*pos < cap) buf[(*pos)++] = '}';
        return;
    }
}

static char *py_dumps_alloc(const json_t *v)
{
    size_t cap = 1 << 20;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    size_t pos = 0;
    py_dump(v, buf, cap, &pos);
    buf[pos < cap ? pos : cap - 1] = '\0';
    return buf;
}

/* ---- exported wrappers -------------------------------------------------- */

/* PoP: w9_apirequest_302923_hook_payload_max_chars @ agent/api_request_hooks.py:_hook_payload_max_chars */
json_t *w9_apirequest_302923_hook_payload_max_chars(json_t *a)
{
    const json_t *env = json_obj_get(a, "env_value");
    const char *raw = json_is_string(env) ? json_string_value(env) : "50000";
    if (!raw) raw = "50000";
    int ok = 0;
    long long n = py_int_value(json_string(raw), &ok);
    if (!ok) return json_number(50000);
    if (n < 1000) n = 1000;
    return json_number((double)n);
}

/* PoP: w9_apirequest_302923_is_sensitive_hook_key @ agent/api_request_hooks.py:_is_sensitive_hook_key */
json_t *w9_apirequest_302923_is_sensitive_hook_key(json_t *a)
{
    const json_t *key = json_obj_get(a, "key");
    if (!key || !json_is_string(key)) return json_bool(0);
    return json_bool(is_sensitive(json_string_value(key)));
}

/* PoP: w9_apirequest_302923_hook_jsonable @ agent/api_request_hooks.py:_hook_jsonable */
json_t *w9_apirequest_302923_hook_jsonable(json_t *a)
{
    json_t *value = json_obj_get(a, "value");
    int depth = (int)json_get_num(a, "depth", 0);
    int max_depth = (int)json_get_num(a, "max_depth", 8);
    int max_string = (int)json_get_num(a, "max_string", 8000);
    int max_sequence = (int)json_get_num(a, "max_sequence", 200);
    return hook_jsonable_core(value, depth, max_depth, max_string, max_sequence);
}

/* PoP: w9_apirequest_302923_sanitize_hook_payload @ agent/api_request_hooks.py:_sanitize_hook_payload */
json_t *w9_apirequest_302923_sanitize_hook_payload(json_t *a)
{
    json_t *value = json_obj_get(a, "value");
    long long limit = (long long)json_get_num(a, "limit", 50000);
    json_t *payload = NULL;
    char *encoded = NULL;
    int caps[2][2] = {{8000, 200}, {1000, 50}}; /* {max_string, max_sequence} */
    for (int i = 0; i < 2; i++) {
        payload = hook_jsonable_core(value, 0, 8, caps[i][0], caps[i][1]);
        encoded = py_dumps_alloc(payload);
        size_t elen = encoded ? strlen(encoded) : 0;
        if ((long long)elen <= limit) {
            free(encoded);
            return payload;
        }
        json_free(payload);
        payload = NULL;
        free(encoded);
        encoded = NULL;
    }
    payload = hook_jsonable_core(value, 0, 8, 1000, 50);
    encoded = py_dumps_alloc(payload);
    json_free(payload);
    size_t elen = encoded ? strlen(encoded) : 0;
    size_t keep = (size_t)((long long)elen <= limit ? (long long)elen : limit);
    char *preview = (char *)malloc(keep + 1);
    if (preview) { memcpy(preview, encoded ? encoded : "", keep); preview[keep] = '\0'; }
    json_t *out = json_object();
    json_set(out, "_truncated", json_bool(1));
    json_set(out, "original_type", json_string(py_typename(value)));
    json_set(out, "preview", json_string(preview ? preview : ""));
    free(preview);
    free(encoded);
    return out;
}

/* PoP: w9_apirequest_302923_request_payload @ agent/api_request_hooks.py:_api_request_payload_for_hook */
json_t *w9_apirequest_302923_request_payload(json_t *a)
{
    json_t *kwargs = json_obj_get(a, "api_kwargs");
    long long limit = (long long)json_get_num(a, "limit", 50000);
    json_t *body = json_object();
    if (json_is_object(kwargs)) {
        size_t n = json_object_size(kwargs);
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(kwargs, i);
            int excluded = 0;
            for (size_t j = 0; j < sizeof(EXCLUDED_BODY_KEYS) / sizeof(EXCLUDED_BODY_KEYS[0]); j++)
                if (k && strcmp(k, EXCLUDED_BODY_KEYS[j]) == 0) { excluded = 1; break; }
            if (excluded) continue;
            json_set(body, k, json_copy(json_object_get_at(kwargs, i)));
        }
    }
    json_t *inner = json_object();
    json_set(inner, "method", json_string("POST"));
    json_set(inner, "body", body);
    json_t *san_in = json_object();
    json_set(san_in, "value", inner);   /* moved */
    json_set(san_in, "limit", json_number((double)limit));
    json_t *result = w9_apirequest_302923_sanitize_hook_payload(san_in);
    json_free(san_in);
    return result;
}
