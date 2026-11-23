/*
 * tts_tool_speaker.c -- faithful C11 port of upstream Hermes tools/tts_tool_speaker.py.
 *
 * PoP: w9_ttstoolspe_4e393d_align_int16_chunks @ tools/tts_tool_speaker.py:_align_int16_chunks
 * PoP: w9_ttstoolspe_4e393d_drain_chunks      @ tools/tts_tool_speaker.py:_drain_chunks
 *
 * The streaming / threading / PortAudio surface is not portable; the two pure byte-chunk
 * helpers are ported verbatim (including the odd-byte carry and the tail zero pad).
 */
#include "noc_w9_ttstoolspe_4e393d.h"
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *b; size_t n, cap; } buf_t;

static void buf_init(buf_t *x) {
    x->cap = 64; x->n = 0;
    x->b = (unsigned char *)malloc(x->cap);
}
static void buf_add(buf_t *x, const unsigned char *p, size_t n) {
    if (!x->b || !n) return;
    if (x->n + n > x->cap) {
        size_t nc = x->cap * 2 + n + 1;
        unsigned char *nb = (unsigned char *)realloc(x->b, nc);
        if (!nb) return;
        x->b = nb; x->cap = nc;
    }
    memcpy(x->b + x->n, p, n);
    x->n += n;
}
static void buf_clear(buf_t *x) { x->n = 0; }
static void buf_free(buf_t *x) { free(x->b); x->b = NULL; x->n = x->cap = 0; }
static json_t *buf_as_json(const unsigned char *b, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_number((double)b[i]));
    return a;
}

json_t *w9_ttstoolspe_4e393d_align_int16_chunks(const json_t *chunks, int stop_after, int pad_tail) {
    json_t *out = json_array();
    buf_t leftover;
    buf_init(&leftover);
    size_t nchunks = (chunks && chunks->type == JSON_ARRAY) ? json_len(chunks) : 0;
    for (size_t i = 0; i < nchunks; i++) {
        /* Synthetic stop event: is_set() returns (i >= stop_after), never for -1. */
        if (stop_after >= 0 && (int)i >= stop_after) break;
        json_t *chunk = json_get(chunks, i);
        buf_t buf;
        buf_init(&buf);
        buf_add(&buf, leftover.b, leftover.n);
        if (chunk && chunk->type == JSON_ARRAY) {
            size_t cn = json_len(chunk);
            for (size_t j = 0; j < cn; j++) {
                json_t *e = json_get(chunk, j);
                unsigned char v = (e && e->type == JSON_NUMBER) ? (unsigned char)e->num_val : 0;
                buf_add(&buf, &v, 1);
            }
        }
        size_t aligned = buf.n - (buf.n % 2);
        if (aligned >= 2) json_append(out, buf_as_json(buf.b, aligned));
        buf_clear(&leftover);
        buf_add(&leftover, buf.b + aligned, buf.n - aligned);
        buf_free(&buf);
    }
    if (leftover.n > 0 && pad_tail) json_append(out, buf_as_json((const unsigned char *)"\0", 1));
    buf_free(&leftover);
    return out;
}

json_t *w9_ttstoolspe_4e393d_drain_chunks(const json_t *items) {
    json_t *out = json_array();
    size_t n = (items && items->type == JSON_ARRAY) ? json_len(items) : 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(items, i);
        if (!e || e->type == JSON_NULL) break;  /* iter(get, None) sentinel */
        json_append(out, json_copy(e));
    }
    return out;
}
