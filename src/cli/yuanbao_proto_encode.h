/*
 * yuanbao_proto_encode.h - yuanbao protobuf frame encoders.
 *
 * Module-internal header (port_yuanbao_proto_helpers.c <-> yuanbao_proto_encode.c).
 */
#ifndef YUANBAO_PROTO_ENCODE_H
#define YUANBAO_PROTO_ENCODE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct { int fn; const char *key; } yb_fld;

#include "yuanbao_proto_buf.h"   /* yb_buf */
#include "hermes_json.h"          /* json_t */

void yb_encode_forward_msg(yb_buf *b, json_t *msg);
void yb_encode_head(yb_buf *b, int cmd_type, const char *cmd, uint64_t seq_no, const char *msg_id, const char *module, int need_ack, uint64_t status);
unsigned char * yb_encode_send_c2c_req(const char *to_account, const char *from_account, const char *msg_body_json, const char *msg_id, uint64_t msg_random, uint64_t msg_seq, const char *group_code, const char *trace_id, size_t *out_len);
unsigned char * yb_encode_send_group_req(const char *group_code, const char *from_account, const char *msg_body_json, const char *msg_id, const char *to_account, const char *random, uint64_t msg_seq, const char *ref_msg_id, const char *trace_id, size_t *out_len);

#endif /* YUANBAO_PROTO_ENCODE_H */
