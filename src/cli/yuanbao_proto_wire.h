/*
 * yuanbao_proto_wire.h - yuanbao protobuf wire helpers.
 *
 * Module-internal header (port_yuanbao_proto_helpers.c <-> yuanbao_proto_wire.c).
 */
#ifndef YUANBAO_PROTO_WIRE_H
#define YUANBAO_PROTO_WIRE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "yuanbao_proto_buf.h"   /* WT_* wire-type defines */



typedef struct { int fn; int wt; uint64_t vint; unsigned char *vbytes; size_t vlen; } yb_field;
typedef struct { yb_field *f; size_t n, cap; } yb_fields;

void yb_fields_free(yb_fields *fs);
unsigned char * yb_get_bytes(const yb_fields *fs, int fn, size_t *outlen);
size_t yb_get_repeated_bytes(const yb_fields *fs, int fn, unsigned char ***out, size_t **lens);
char * yb_get_string(const yb_fields *fs, int fn, char *buf, size_t bufsz);
uint64_t yb_get_varint(const yb_fields *fs, int fn, uint64_t def);
int yb_parse_fields(const unsigned char *d, size_t n, yb_fields *out);

#endif /* YUANBAO_PROTO_WIRE_H */
