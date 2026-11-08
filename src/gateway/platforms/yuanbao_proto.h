/*
 * yuanbao_proto.h - Yuanbao wire protocol codecs.
 *
 * Module-internal header (yuanbao.c <-> yuanbao_proto.c). Also carries the
 *
 * wire constants shared by both sides (BIZ_PKG, CT_REQUEST, the HEAD_* field
 * ids).
 */
#ifndef YUANBAO_PROTO_H
#define YUANBAO_PROTO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define BIZ_PKG "yuanbao_openclaw_proxy"
#define CONN_FIELD_DATA 2  /* bytes */
#define CONN_FIELD_HEAD 1  /* message */
#define CT_REQUEST  0
#define HEAD_CMD       2  /* string */
#define HEAD_CMD_TYPE  1  /* varint */
#define HEAD_MODULE    5  /* string */
#define HEAD_MSG_ID    4  /* string */
#define HEAD_SEQ_NO    3  /* varint */

int decode_conn_msg(const uint8_t *data, size_t data_len, int *out_cmd_type, char *out_cmd, size_t cmd_sz, uint32_t *out_seq_no, char *out_module, size_t mod_sz, const uint8_t **out_body, size_t *out_body_len);
int encode_conn_msg(uint8_t *buf, size_t buf_len, int cmd_type, const char *cmd, uint32_t seq_no, const char *msg_id, const char *module, const uint8_t *data, size_t data_len);
int encode_get_group_member_list(uint8_t *buf, size_t buf_len, const char *group_code, uint32_t offset, uint32_t limit, uint32_t seq_no);
int encode_query_group_info(uint8_t *buf, size_t buf_len, const char *group_code, uint32_t seq_no);

#endif /* YUANBAO_PROTO_H */
