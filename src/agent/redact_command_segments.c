/*
 * redact_command_segments.c — real C11 port of upstream Hermes
 * agent/redact_command_segments.py.
 *
 * PoP: w17_redactcomm_2044a9_command_segments @ agent/redact_command_segments.py:_command_segments
 */
#include "noc_w17_redactcomm_2044a9.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uint32_t *v; size_t n, cap; } Cps;

static void cps_push(Cps *a, uint32_t cp) {
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 64;
        uint32_t *nv = (uint32_t *)realloc(a->v, nc * sizeof(uint32_t));
        if (!nv) return;
        a->v = nv; a->cap = nc;
    }
    a->v[a->n++] = cp;
}

static Cps decode_utf8(const char *s) {
    Cps a = {NULL, 0, 0};
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        uint32_t cp;
        unsigned char b0 = p[0];
        if (b0 < 0x80) { cp = b0; p++; }
        else if ((b0 & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(b0 & 0x1F) << 6) | (p[1] & 0x3F); p += 2;
        } else if ((b0 & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((uint32_t)(b0 & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3;
        } else if ((b0 & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 &&
                   (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((uint32_t)(b0 & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) |
                 ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4;
        } else { cp = b0; p++; }
        cps_push(&a, cp);
    }
    return a;
}

static int py_ws(uint32_t cp) {
    if (cp >= 0x09 && cp <= 0x0D) return 1;
    if (cp >= 0x1C && cp <= 0x1F) return 1;
    return cp == 0x20 || cp == 0x85 || cp == 0xA0 || cp == 0x1680 ||
           (cp >= 0x2000 && cp <= 0x200A) || cp == 0x2028 || cp == 0x2029 ||
           cp == 0x202F || cp == 0x205F || cp == 0x3000;
}

/* Emit a trimmed segment (str.strip()) into arr, skipping empties. */
static void emit_segment(Cps *buf, json_t *arr) {
    size_t lo = 0, hi = buf->n, i;
    size_t cap;
    char *s;
    while (lo < hi && py_ws(buf->v[lo])) lo++;
    while (hi > lo && py_ws(buf->v[hi - 1])) hi--;
    if (hi == lo) return;
    cap = (hi - lo) * 4 + 1;
    s = (char *)malloc(cap);
    if (!s) return;
    {
        size_t k = 0;
        for (i = lo; i < hi; i++) {
            uint32_t cp = buf->v[i];
            if (cp < 0x80) s[k++] = (char)cp;
            else if (cp < 0x800) {
                s[k++] = (char)(0xC0 | (cp >> 6));
                s[k++] = (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                s[k++] = (char)(0xE0 | (cp >> 12));
                s[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                s[k++] = (char)(0x80 | (cp & 0x3F));
            } else {
                s[k++] = (char)(0xF0 | (cp >> 18));
                s[k++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                s[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                s[k++] = (char)(0x80 | (cp & 0x3F));
            }
        }
        s[k] = 0;
    }
    json_append(arr, json_string(s));
    free(s);
}

json_t *w17_redactcomm_2044a9_command_segments(const char *command) {
    Cps in = decode_utf8(command);
    Cps buf = {NULL, 0, 0};
    json_t *arr = json_array();
    uint32_t quote = 0;
    size_t i;

    for (i = 0; i < in.n; i++) {
        uint32_t cp = in.v[i];
        if (quote) {
            cps_push(&buf, cp);
            if (cp == quote) quote = 0;
            continue;
        }
        if (cp == '\'' || cp == '"') {
            quote = cp;
            cps_push(&buf, cp);
            continue;
        }
        if (cp == '|' || cp == ';' || cp == '&') {
            emit_segment(&buf, arr);
            free(buf.v); buf.v = NULL; buf.n = buf.cap = 0;
            continue;
        }
        cps_push(&buf, cp);
    }
    emit_segment(&buf, arr);
    free(buf.v);
    free(in.v);
    return arr;
}
