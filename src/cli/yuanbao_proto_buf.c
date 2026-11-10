/*
 * yuanbao_proto_buf.c - growable byte buffer for the Yuanbao codec.
 *
 * Split from port_yuanbao_proto_helpers.c (soul.md: self-contained modules).
 * Append/reserve/varint helpers plus the field-descriptor builders. No
 * file-scope state.
 */
#include "yuanbao_proto_buf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


void yb_init(yb_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }

void yb_free(yb_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }

static void yb_reserve(yb_buf *b, size_t extra)
{
    if (b->len + extra <= b->cap) return;
    size_t ncap = b->cap ? b->cap * 2 : 64;
    while (ncap < b->len + extra) ncap *= 2;
    b->p = realloc(b->p, ncap);
    b->cap = ncap;
}

static void yb_append(yb_buf *b, const unsigned char *d, size_t n)
{
    if (n == 0) return;
    yb_reserve(b, n);
    memcpy(b->p + b->len, d, n);
    b->len += n;
}

static void yb_append_byte(yb_buf *b, unsigned char c) { yb_reserve(b, 1); b->p[b->len++] = c; }


/* varint encode */
/* PoP: _encode_varint @ gateway/platforms/yuanbao_proto.py:_encode_varint */
static void yb_append_varint(yb_buf *b, uint64_t v)
{
    if ((int64_t)v < 0) v &= 0xFFFFFFFFFFFFFFFFULL;
    while (1) {
        unsigned char bits = (unsigned char)(v & 0x7F);
        v >>= 7;
        if (v) yb_append_byte(b, bits | 0x80);
        else { yb_append_byte(b, bits); break; }
    }
}


/* field tag = (field_number << 3) | wire_type, then value */
/* PoP: _encode_field @ gateway/platforms/yuanbao_proto.py:_encode_field */
static void yb_append_field(yb_buf *b, int fn, int wt, const unsigned char *val, size_t n)
{
    yb_append_varint(b, (uint64_t)((fn << 3) | wt));
    if (wt == WT_LEN || wt == WT_64BIT || wt == WT_32BIT) {
        yb_append_varint(b, (uint64_t)n);
        yb_append(b, val, n);
    } else {
        yb_append_varint(b, *(uint64_t *)val); /* not used for varint path */
    }
}

/* convenience: varint field */
void yb_f_varint(yb_buf *b, int fn, uint64_t v)
{ yb_append_varint(b, (uint64_t)((fn << 3) | WT_VARINT)); yb_append_varint(b, v); }

/* convenience: length-delimited field from raw bytes */

/* PoP: _encode_bytes @ gateway/platforms/yuanbao_proto.py:_encode_bytes */void yb_f_len(yb_buf *b, int fn, const unsigned char *d, size_t n)
{ yb_append_varint(b, (uint64_t)((fn << 3) | WT_LEN)); yb_append_varint(b, n); yb_append(b, d, n); }

/* string field */

/* PoP: _encode_string @ gateway/platforms/yuanbao_proto.py:_encode_string */void yb_f_str(yb_buf *b, int fn, const char *s)
{ if (!s || !*s) return; size_t n = strlen(s); yb_append_varint(b, (uint64_t)((fn<<3)|WT_LEN)); yb_append_varint(b, n); yb_append(b, (const unsigned char*)s, n); }

/* length-delimited from a sub-buffer (message/bytes wrapper) */

/* PoP: _encode_message @ gateway/platforms/yuanbao_proto.py:_encode_message */void yb_f_sub(yb_buf *b, int fn, const yb_buf *sub)
{ yb_f_len(b, fn, sub->p, sub->len); }
