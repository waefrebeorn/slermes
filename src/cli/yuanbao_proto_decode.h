/*
 * yuanbao_proto_decode.h - yuanbao protobuf frame decoders.
 *
 * Module-internal header (port_yuanbao_proto_helpers.c <-> yuanbao_proto_decode.c).
 */
#ifndef YUANBAO_PROTO_DECODE_H
#define YUANBAO_PROTO_DECODE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define CT_RESPONSE 1

#include "yuanbao_proto_wire.h"   /* yb_fields, yb_get_*, yb_parse_fields */



/* called by yuanbao_proto_decode_inbound_push before its definition */
char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len);

#endif /* YUANBAO_PROTO_DECODE_H */
