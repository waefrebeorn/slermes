/*
 * turn_scripted_prelude.c — C port of upstream Hermes agent/turn_scripted_prelude.py.
 *
 * PoP: w14_turnscript_3a854a_prelude_call_id @ agent/turn_scripted_prelude.py:prelude_call_id
 * PoP: w14_turnscript_3a854a_play_prelude @ agent/turn_scripted_prelude.py:play_prelude
 *
 * prelude_call_id's real content is a canonical json.dumps(..., sort_keys=True)
 * followed by a deterministic hash of the seeded string; the hash seam is
 * injected by the fixture so the C side can reproduce it byte-for-byte.
 * play_prelude is the (action, result) decision around run_scripted_prelude.
 */
#include "noc_w14_turnscript_3a854a.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUF_CAP 65536

typedef struct { char buf[BUF_CAP]; size_t len; } buf_t;

static void put(buf_t *b, const char *s, size_t n) {
    if (b->len + n + 1 > BUF_CAP) n = BUF_CAP - b->len - 1;
    if (n) memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}
static void puts_(buf_t *b, const char *s) { put(b, s, strlen(s)); }

/* Python json.dumps(ensure_ascii=True) string escaping. */
static void put_json_string(buf_t *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char esc[16];
    put(b, "\"", 1);
    while (*p) {
        unsigned int cp;
        int extra;
        if (*p < 0x80) { cp = *p; extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((*p & 0x1F) << 6) | (p[1] & 0x3F); extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { cp = ((*p & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); extra = 2; }
        else { cp = ((*p & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F); extra = 3; }
        p += extra + 1;
        if (cp == '"') puts_(b, "\\\"");
        else if (cp == '\\') puts_(b, "\\\\");
        else if (cp == '\b') puts_(b, "\\b");
        else if (cp == '\f') puts_(b, "\\f");
        else if (cp == '\n') puts_(b, "\\n");
        else if (cp == '\r') puts_(b, "\\r");
        else if (cp == '\t') puts_(b, "\\t");
        else if (cp < 0x20) { snprintf(esc, sizeof esc, "\\u%04x", cp); puts_(b, esc); }
        else if (cp < 0x80) { char c = (char)cp; put(b, &c, 1); }
        else if (cp <= 0xFFFF) { snprintf(esc, sizeof esc, "\\u%04x", cp); puts_(b, esc); }
        else {
            unsigned int v = cp - 0x10000;
            snprintf(esc, sizeof esc, "\\u%04x\\u%04x", 0xD800 + (v >> 10), 0xDC00 + (v & 0x3FF));
            puts_(b, esc);
        }
    }
    put(b, "\"", 1);
}

typedef struct { const char *key; const struct json_t *val; } kv_t;
static int kv_cmp(const void *a, const void *b) {
    const kv_t *x = (const kv_t *)a, *y = (const kv_t *)b;
    return strcmp(x->key, y->key);
}

static void canon_dump(buf_t *b, const struct json_t *v) {
    if (!v || v->type == JSON_NULL) { puts_(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL:
            puts_(b, v->bool_val ? "true" : "false");
            return;
        case JSON_NUMBER: {
            char num[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(num, sizeof num, "%lld", (long long)v->num_val);
            else
                snprintf(num, sizeof num, "%g", v->num_val);
            puts_(b, num);
            return;
        }
        case JSON_STRING:
            put_json_string(b, v->str_val);
            return;
        case JSON_ARRAY: {
            size_t i, n = json_len(v);
            put(b, "[", 1);
            for (i = 0; i < n; i++) {
                if (i) puts_(b, ", ");
                canon_dump(b, json_get(v, i));
            }
            put(b, "]", 1);
            return;
        }
        case JSON_OBJECT: {
            size_t i, n = json_object_size(v);
            kv_t *pairs = n ? (kv_t *)malloc(n * sizeof(kv_t)) : NULL;
            put(b, "{", 1);
            for (i = 0; i < n; i++) {
                pairs[i].key = json_object_get_key_at(v, i);
                pairs[i].val = json_object_get_at(v, i);
            }
            if (pairs && n > 1) qsort(pairs, n, sizeof(kv_t), kv_cmp);
            for (i = 0; i < n; i++) {
                if (i) puts_(b, ", ");
                put_json_string(b, pairs[i].key);
                puts_(b, ": ");
                canon_dump(b, pairs[i].val);
            }
            free(pairs);
            put(b, "}", 1);
            return;
        }
        default:
            puts_(b, "null");
            return;
    }
}

char *w14_turnscript_3a854a_prelude_call_id(const char *session_id, const char *name,
                                           const struct json_t *args, long long index) {
    buf_t b;
    char tail[64];
    size_t total;
    char *out;
    b.len = 0;
    b.buf[0] = '\0';
    puts_(&b, "dcid:");
    puts_(&b, name ? name : "");
    puts_(&b, ":");
    puts_(&b, session_id ? session_id : "");
    puts_(&b, ":");
    canon_dump(&b, args);
    snprintf(tail, sizeof tail, ":%lld", index);
    puts_(&b, tail);
    total = b.len + 1;
    out = (char *)malloc(total);
    if (out) memcpy(out, b.buf, total);
    return out;
}

struct json_t *w14_turnscript_3a854a_play_prelude(int prelude_present, const struct json_t *verdict) {
    struct json_t *out = json_array();
    const char *action = "run";
    const struct json_t *result = NULL;
    if (!out) return NULL;
    if (prelude_present && verdict && verdict->type == JSON_OBJECT) {
        const struct json_t *a = json_obj_get(verdict, "action");
        if (a && a->type == JSON_STRING && a->str_val &&
            (!strcmp(a->str_val, "return") || !strcmp(a->str_val, "break"))) {
            action = a->str_val;
            result = json_obj_get(verdict, "result");
        }
    }
    json_append(out, json_string(action));
    json_append(out, result ? json_copy(result) : json_null());
    return out;
}
