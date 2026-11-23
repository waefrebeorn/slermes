/*
 * web_server_oauth.c — C port of upstream Hermes hermes_cli/web_server_oauth.py.
 *
 * PoP: w15_webserver_a210ca_truncate_token @ hermes_cli/web_server_oauth.py:_truncate_token
 * PoP: w15_webserver_a210ca_oauth_profile_name @ hermes_cli/web_server_oauth.py:_oauth_profile_name
 * PoP: w15_webserver_a210ca_token_status @ hermes_cli/web_server_oauth.py:_token_status
 *
 * Token truncation is codepoint-aware (Python len/slicing counts code points,
 * not UTF-8 bytes) and uses the exact U+2026 ellipsis. A callable credential is
 * injected as is_callable/access_token_callable and yields the live placeholder
 * without ever being invoked.
 */
#include "noc_w15_webserver_a210ca.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define ELLIPSIS "\xE2\x80\xA6"   /* … */

static size_t utf8_len(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;   /* count lead bytes */
    return n;
}
/* Byte offset of the start of the last `k` codepoints. */
static size_t utf8_tail_offset(const char *s, size_t k) {
    size_t total = utf8_len(s);
    if (k >= total) return 0;
    size_t want = total - k;
    size_t seen = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) {
            if (seen == want) return (size_t)((const char *)p - s);
            seen++;
        }
    }
    return strlen(s);
}

struct json_t *w15_webserver_a210ca_truncate_token(const char *value, int visible, int is_callable) {
    if (!value || !*value) return json_string("");
    if (is_callable) return json_string("<entra-id-bearer>");
    const char *s = value;

    /* JWT: keep only the tail of the signature segment when >= 2 dots. */
    int dots = 0;
    const char *last_dot = NULL;
    for (const char *p = s; *p; p++) {
        if (*p == '.') { dots++; last_dot = p; }
    }
    if (dots >= 2 && last_dot) s = last_dot + 1;

    size_t n = utf8_len(s);
    if (visible <= 0) { /* Python s[-0:] is the whole string, still prefixed by ellipsis */
        size_t sl = strlen(s);
        char *out0 = (char *)malloc(strlen(ELLIPSIS) + sl + 1);
        if (!out0) return json_string("");
        memcpy(out0, ELLIPSIS, strlen(ELLIPSIS));
        memcpy(out0 + strlen(ELLIPSIS), s, sl);
        out0[strlen(ELLIPSIS) + sl] = '\0';
        struct json_t *r0 = json_string(out0);
        free(out0);
        return r0;
    }
    if (n <= (size_t)visible) return json_string(s);

    size_t off = utf8_tail_offset(s, (size_t)visible);
    size_t tail_len = strlen(s) - off;
    size_t total = strlen(ELLIPSIS) + tail_len + 1;
    char *out = (char *)malloc(total);
    if (!out) return json_string("");
    memcpy(out, ELLIPSIS, strlen(ELLIPSIS));
    memcpy(out + strlen(ELLIPSIS), s + off, tail_len);
    out[strlen(ELLIPSIS) + tail_len] = '\0';
    struct json_t *res = json_string(out);
    free(out);
    return res;
}

struct json_t *w15_webserver_a210ca_oauth_profile_name(const char *profile) {
    const char *p = profile ? profile : "";
    const char *e = p + strlen(p);
    while (p < e && isspace((unsigned char)*p)) p++;
    while (e > p && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - p);
    if (n == 0) return json_null();
    if (n == 7) {
        int current = 1;
        const char *want = "current";
        for (size_t i = 0; i < n; i++)
            if (tolower((unsigned char)p[i]) != want[i]) { current = 0; break; }
        if (current) return json_null();
    }
    char *name = (char *)malloc(n + 1);
    if (!name) return json_null();
    memcpy(name, p, n);
    name[n] = '\0';                 /* NUL at the shortened length */
    struct json_t *res = json_string(name);
    free(name);
    return res;
}

static int json_truthy(const struct json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:  return 0;
        case JSON_BOOL:  return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return 0;
    }
}

struct json_t *w15_webserver_a210ca_token_status(const char *source, const char *source_label,
                                                const struct json_t *creds, int access_token_callable) {
    struct json_t *out = json_object();
    json_set(out, "logged_in", json_bool(true));
    json_set(out, "source", json_string(source ? source : ""));
    json_set(out, "source_label", json_string(source_label ? source_label : ""));

    struct json_t *access = json_obj_get(creds, "accessToken");
    struct json_t *preview = w15_webserver_a210ca_truncate_token(
        access && access->type == JSON_STRING ? access->str_val : "",
        6, access_token_callable);
    json_set(out, "token_preview", preview);

    struct json_t *expires = json_obj_get(creds, "expiresAt");
    json_set(out, "expires_at", expires ? json_copy(expires) : json_null());
    json_set(out, "has_refresh_token",
             json_bool(json_truthy(json_obj_get(creds, "refreshToken")) ? true : false));
    return out;
}
