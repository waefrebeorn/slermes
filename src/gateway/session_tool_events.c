/*
 * session_tool_events.c — C port of upstream Hermes gateway/session_tool_events.py.
 * PoP: stool_bounded_args @ gateway/session_tool_events.py:bounded_args
 * PoP: stool_bounded_result @ gateway/session_tool_events.py:bounded_result
 *
 * The upstream functions measure and truncate the *serialized* value with
 * json.dumps(..., ensure_ascii=True, default=str). All fixture values are
 * JSON-serializable, so default=str never fires; the serializer below matches
 * Python's default separators (", " / ": ") and ensure_ascii escaping (including
 * surrogate pairs), which makes len(encoded) == strlen(encoded).
 */
#include "noc_stool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOOL_EVENT_CHARS (64 * 1024)

/* ---- tiny growable string buffer ---- */
typedef struct { char *data; size_t len, cap; } sbuf;

static void sb_init(sbuf *b) {
    b->cap = 256;
    b->data = (char *)malloc(b->cap);
    b->len = 0;
    if (b->data) b->data[0] = '\0';
}
static void sb_need(sbuf *b, size_t extra) {
    if (!b->data) return;
    if (b->len + extra + 1 <= b->cap) return;
    while (b->len + extra + 1 > b->cap) b->cap *= 2;
    b->data = (char *)realloc(b->data, b->cap);
}
static void sb_putc(sbuf *b, char c) {
    sb_need(b, 1);
    if (!b->data) return;
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
}
static void sb_puts(sbuf *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    sb_need(b, n);
    if (!b->data) return;
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

/* ---- json.dumps string escaping with ensure_ascii=True ---- */
static void sb_json_string(sbuf *b, const char *s) {
    if (!s) s = "";
    sb_putc(b, '"');
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p;
        switch (c) {
            case '"':  sb_puts(b, "\\\""); p++; continue;
            case '\\': sb_puts(b, "\\\\"); p++; continue;
            case '\b': sb_puts(b, "\\b");  p++; continue;
            case '\f': sb_puts(b, "\\f");  p++; continue;
            case '\n': sb_puts(b, "\\n");  p++; continue;
            case '\r': sb_puts(b, "\\r");  p++; continue;
            case '\t': sb_puts(b, "\\t");  p++; continue;
            default: break;
        }
        if (c < 0x20) {
            char t[8];
            snprintf(t, sizeof t, "\\u%04x", (unsigned)c);
            sb_puts(b, t);
            p++;
        } else if (c < 0x80) {
            sb_putc(b, (char)c);
            p++;
        } else {
            unsigned int cp;
            size_t step;
            if ((c & 0xE0) == 0xC0) { cp = c & 0x1Fu; step = 2; }
            else if ((c & 0xF0) == 0xE0) { cp = c & 0x0Fu; step = 3; }
            else if ((c & 0xF8) == 0xF0) { cp = c & 0x07u; step = 4; }
            else { cp = c; step = 1; }
            for (size_t i = 1; i < step && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3Fu);
            p += step;
            if (cp <= 0xFFFF) {
                char t[8];
                snprintf(t, sizeof t, "\\u%04x", cp);
                sb_puts(b, t);
            } else {
                cp -= 0x10000;
                unsigned hi = 0xD800 + (cp >> 10), lo = 0xDC00 + (cp & 0x3FF);
                char t[16];
                snprintf(t, sizeof t, "\\u%04x\\u%04x", hi, lo);
                sb_puts(b, t);
            }
        }
    }
    sb_putc(b, '"');
}

/* ---- json.dumps(value, ensure_ascii=True) for parsed JSON values ---- */
static void dumps_into(sbuf *b, json_t *v) {
    if (!v || v->type == JSON_NULL) { sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL:
            sb_puts(b, v->bool_val ? "true" : "false");
            break;
        case JSON_NUMBER: {
            double d = v->num_val;
            char t[64];
            if (d == (double)(long long)d && d > -9e18 && d < 9e18)
                snprintf(t, sizeof t, "%lld", (long long)d);
            else
                snprintf(t, sizeof t, "%.17g", d);
            sb_puts(b, t);
            break;
        }
        case JSON_STRING:
            sb_json_string(b, v->str_val ? v->str_val : "");
            break;
        case JSON_ARRAY:
            sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                dumps_into(b, v->c.items[i]);
            }
            sb_putc(b, ']');
            break;
        case JSON_OBJECT:
            sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                sb_json_string(b, v->c.keys[i] ? v->c.keys[i] : "");
                sb_puts(b, ": ");
                dumps_into(b, v->c.items[i]);
            }
            sb_putc(b, '}');
            break;
        default:
            sb_puts(b, "null");
            break;
    }
}

static char *py_dumps(json_t *v) {
    sbuf b;
    sb_init(&b);
    dumps_into(&b, v);
    return b.data ? b.data : strdup("");
}

/* Python s[:n] by code point. */
static char *py_slice(const char *s, size_t nchars) {
    if (!s) s = "";
    size_t nbytes = 0, cnt = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && cnt < nchars) {
        unsigned char c = *p;
        size_t step = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 :
                      ((c & 0xF0) == 0xE0) ? 3 : ((c & 0xF8) == 0xF0) ? 4 : 1;
        nbytes += step;
        p += step;
        cnt++;
    }
    char *r = (char *)malloc(nbytes + 1);
    if (!r) return strdup("");
    memcpy(r, s, nbytes);
    r[nbytes] = '\0';
    return r;
}

json_t *stool_bounded_args(json_t *args) {
    if (!args || args->type != JSON_OBJECT) return json_copy(args ? args : json_null());

    char *encoded = py_dumps(args);
    size_t enc_len = strlen(encoded);
    if (enc_len <= MAX_TOOL_EVENT_CHARS) {
        free(encoded);
        return json_copy(args);
    }
    free(encoded);

    json_t *out = json_object();
    long long remaining = MAX_TOOL_EVENT_CHARS / 2; /* 32768 */
    size_t kept = 0;
    for (size_t i = 0; i < args->c.count; i++) {
        if (remaining <= 0 || kept >= 64) break;

        const char *key = args->c.keys[i] ? args->c.keys[i] : "";
        char *name = py_slice(key, 256); /* str(key)[:256] */

        json_t *val = args->c.items[i];
        char *text;
        if (val && val->type == JSON_STRING)
            text = strdup(val->str_val ? val->str_val : "");
        else
            text = py_dumps(val);

        long long budget = 4096;
        if (remaining < budget) budget = remaining;
        char *trunc = py_slice(text, (size_t)budget); /* text[:min(4096, remaining)] */
        free(text);

        json_set(out, name, json_string(trunc));

        /* remaining -= len(json.dumps([name, text], ensure_ascii=True)) */
        json_t *pair = json_array();
        json_append(pair, json_string(name));
        json_append(pair, json_string(trunc));
        char *pd = py_dumps(pair);
        remaining -= (long long)strlen(pd);
        free(pd);
        json_free(pair);

        free(trunc);
        free(name);
        kept++;
    }
    return out;
}

json_t *stool_bounded_result(json_t *result, int is_error) {
    char *text;
    if (result && result->type == JSON_STRING)
        text = strdup(result->str_val ? result->str_val : "");
    else
        text = py_dumps(result);

    size_t len = strlen(text);
    if (len <= MAX_TOOL_EVENT_CHARS) {
        json_t *out = json_string(text);
        free(text);
        return out;
    }

    /* The return value is a STRING: json.dumps({...}), default separators. */
    char *head = py_slice(text, MAX_TOOL_EVENT_CHARS / 2); /* text[:32768] */
    free(text);
    json_t *d = json_object();
    json_set(d, is_error ? "error" : "output", json_string(head));
    json_set(d, "truncated", json_bool(true));
    char *ds = py_dumps(d);
    json_free(d);
    free(head);
    json_t *out = json_string(ds);
    free(ds);
    return out;
}
