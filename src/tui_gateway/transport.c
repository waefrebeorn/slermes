/*
 * transport.c -- C home for upstream Hermes tui_gateway/transport.py
 * (wave8 gate w8_transport_607bb2).
 *
 * PoP: w8_transport_607bb2_serialize_frame @ tui_gateway/transport.py:serialize_frame
 * PoP: w8_transport_607bb2_tee_write @ tui_gateway/transport.py:TeeTransport.write
 *
 * serialize_frame reproduces json.dumps(obj, ensure_ascii=False) with Python's
 * default separators using a small string builder; the injected secondary
 * seams let the TeeTransport.write decision be replayed without real streams.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_transport_607bb2.h"

typedef struct { char *p; size_t len, cap; } w8_sb;

static void w8_sb_reserve(w8_sb *s, size_t need) {
    if (s->len + need + 1 <= s->cap) return;
    size_t nc = s->cap ? s->cap : 128;
    while (nc < s->len + need + 1) nc *= 2;
    char *np = (char *)realloc(s->p, nc);
    if (!np) return;
    s->p = np; s->cap = nc;
}
static void w8_sb_putc(w8_sb *s, char c) {
    w8_sb_reserve(s, 1);
    if (!s->p) return;
    s->p[s->len++] = c; s->p[s->len] = '\0';
}
static void w8_sb_puts(w8_sb *s, const char *x) {
    size_t n = strlen(x);
    w8_sb_reserve(s, n);
    if (!s->p) return;
    memcpy(s->p + s->len, x, n); s->len += n; s->p[s->len] = '\0';
}
static void w8_sb_str(w8_sb *s, const char *str) {
    const unsigned char *p = (const unsigned char *)(str ? str : "");
    w8_sb_putc(s, '"');
    for (; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
            case '"':  w8_sb_puts(s, "\\\""); break;
            case '\\': w8_sb_puts(s, "\\\\"); break;
            case '\b': w8_sb_puts(s, "\\b");  break;
            case '\f': w8_sb_puts(s, "\\f");  break;
            case '\n': w8_sb_puts(s, "\\n");  break;
            case '\r': w8_sb_puts(s, "\\r");  break;
            case '\t': w8_sb_puts(s, "\\t");  break;
            default:
                if (c < 32) { char esc[8]; snprintf(esc, sizeof(esc), "\\u%04x", c); w8_sb_puts(s, esc); }
                else w8_sb_putc(s, (char)c);  /* ensure_ascii=False: pass UTF-8 through */
        }
    }
    w8_sb_putc(s, '"');
}
static void w8_sb_node(w8_sb *s, const json_t *n) {
    if (!n) { w8_sb_puts(s, "null"); return; }
    switch (n->type) {
        case JSON_NULL:   w8_sb_puts(s, "null"); break;
        case JSON_BOOL:   w8_sb_puts(s, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            if (n->num_val == (double)(long long)n->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", n->num_val);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", n->num_val);
            w8_sb_puts(s, tmp);
            break;
        }
        case JSON_STRING: w8_sb_str(s, n->str_val); break;
        case JSON_ARRAY:
            w8_sb_putc(s, '[');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) { w8_sb_putc(s, ','); w8_sb_putc(s, ' '); }
                w8_sb_node(s, n->c.items[i]);
            }
            w8_sb_putc(s, ']');
            break;
        case JSON_OBJECT:
            w8_sb_putc(s, '{');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i) { w8_sb_putc(s, ','); w8_sb_putc(s, ' '); }
                w8_sb_str(s, n->c.keys[i]);
                w8_sb_puts(s, ": ");
                w8_sb_node(s, n->c.items[i]);
            }
            w8_sb_putc(s, '}');
            break;
        default:
            w8_sb_puts(s, "null");
            break;
    }
}

char *w8_transport_607bb2_serialize_frame(const json_t *obj) {
    w8_sb s;
    memset(&s, 0, sizeof(s));
    w8_sb_node(&s, obj);
    if (!s.p) {
        char *fallback = (char *)malloc(5);
        if (fallback) memcpy(fallback, "null", 5);
        return fallback;
    }
    return s.p;
}

json_t *w8_transport_607bb2_tee_write(int primary_ok, const json_t *secondaries) {
    (void)secondaries;  /* secondary failures never change the primary's verdict */
    return json_bool(primary_ok ? true : false);
}
