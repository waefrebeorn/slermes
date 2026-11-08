/*
 * yuanbao_proto_buf.h - growable byte buffer for the Yuanbao codec.
 *
 * Module-internal header (port_yuanbao_proto_helpers.c <-> yuanbao_proto_buf.c).
 */
#ifndef YUANBAO_PROTO_BUF_H
#define YUANBAO_PROTO_BUF_H

#include <stddef.h>
#include <stdint.h>

typedef struct { unsigned char *p; size_t len, cap; } yb_buf;

#define WT_VARINT 0
#define WT_64BIT  1
#define WT_LEN    2
#define WT_32BIT  5

void yb_f_len(yb_buf *b, int fn, const unsigned char *d, size_t n);
void yb_f_str(yb_buf *b, int fn, const char *s);
void yb_f_sub(yb_buf *b, int fn, const yb_buf *sub);
void yb_f_varint(yb_buf *b, int fn, uint64_t v);
void yb_free(yb_buf *b);
void yb_init(yb_buf *b);

#endif /* YUANBAO_PROTO_BUF_H */
