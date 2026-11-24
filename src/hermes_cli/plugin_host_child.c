/*
 * plugin_host_child.c - C home for upstream Hermes hermes_cli/plugin_host_child.py.
 * PoP: w16_pluginhost_f7395c_decode @ hermes_cli/plugin_host_child.py:decode
 * PoP: w16_pluginhost_f7395c_host_unsupported_reason @ hermes_cli/plugin_host_child.py:_host_unsupported_reason
 *
 * Faithful ports of the host-child value codec (decode half, as re-exported
 * from plugin_host_wire and used by HostRuntime._decode_args) and the pure
 * create_client override check. resolve/proxy callbacks stay in Python; the
 * injected boolean stands in for `type(profile).create_client`.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_pluginhost_f7395c.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static int has_key(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return 0;
    for (size_t i = 0; i < json_object_size(obj); i++) {
        const char *k = json_object_get_key_at(obj, i);
        if (k && !strcmp(k, key)) return 1;
    }
    return 0;
}

/* Python str(v) for the scalar types decode() coerces via str(). */
static char *py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dupstr("None");
    switch (v->type) {
        case JSON_STRING: return dupstr(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return dupstr(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return dupstr(buf);
        default: return dupstr("");
    }
}

static int b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* base64.b64decode of an ASCII payload; returned NUL-terminated (payloads in
 * fixtures decode to ASCII text, matching the oracle's latin-1 canon). */
static char *b64decode(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 2);
    if (!out) return dupstr("");
    size_t o = 0;
    int acc = 0, bits = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '=') break;
        int v = b64val(c);
        if (v < 0) continue;  /* base64.b64decode default: discard non-alphabet */
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out[o++] = (char)((acc >> bits) & 0xFF); }
    }
    out[o] = '\0';
    return out;
}

static json_t *decode_node(const json_t *v);

static json_t *decode_dict(const json_t *obj) {
    /* __record__ -> Record(decode(fields or {})) -> plain fields dict */
    if (has_key(obj, "__record__")) {
        json_t *fields = json_obj_get(obj, "fields");
        int empty = !fields || fields->type == JSON_NULL ||
                    (fields->type == JSON_OBJECT && fields->c.count == 0);
        return decode_node(empty ? NULL : fields);
    }
    /* __bytes__ -> base64-decoded bytes (canon: decoded text) */
    if (has_key(obj, "__bytes__")) {
        char *s = py_str(json_obj_get(obj, "__bytes__"));
        char *raw = b64decode(s);
        json_t *res = json_string(raw);
        free(s); free(raw);
        return res;
    }
    /* __opaque__ -> Opaque(str(type), str(repr or "")) */
    if (has_key(obj, "__opaque__")) {
        char *t = py_str(json_obj_get(obj, "__opaque__"));
        json_t *rv = json_obj_get(obj, "repr");
        int falsy = !rv || rv->type == JSON_NULL ||
                    (rv->type == JSON_STRING && (!rv->str_val || !rv->str_val[0])) ||
                    (rv->type == JSON_NUMBER && rv->num_val == 0.0) ||
                    (rv->type == JSON_BOOL && !rv->bool_val);
        char *r = falsy ? dupstr("") : py_str(rv);
        json_t *res = json_object();
        json_set(res, "__opaque__", json_string(t));
        json_set(res, "repr", json_string(r));
        free(t); free(r);
        return res;
    }
    /* generic dict: {k: decode(v)} (resolve is None, so ref markers pass through) */
    json_t *res = json_object();
    for (size_t i = 0; i < json_object_size(obj); i++) {
        const char *k = json_object_get_key_at(obj, i);
        if (!k) continue;
        char *key = dupstr(k);                 /* copy before any mutation/free */
        json_set(res, key, decode_node(json_object_get_at(obj, i)));
        free(key);
    }
    return res;
}

static json_t *decode_node(const json_t *v) {
    if (!v) return json_object();  /* decode(None) is None; caller passes fields-or-{} as NULL */
    if (v->type == JSON_ARRAY) {
        json_t *res = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(res, decode_node(v->c.items[i]));
        return res;
    }
    if (v->type == JSON_OBJECT) return decode_dict(v);
    return json_copy(v);
}

struct json_t *w16_pluginhost_f7395c_decode(const struct json_t *value) {
    if (!value || value->type == JSON_NULL) return json_null();
    return decode_node(value);
}

struct json_t *w16_pluginhost_f7395c_host_unsupported_reason(const struct json_t *overrides_create_client) {
    int overrides = overrides_create_client && overrides_create_client->type == JSON_BOOL &&
                    overrides_create_client->bool_val;
    if (overrides)
        return json_string("it overrides create_client(), which returns a live SDK client");
    return json_null();
}
