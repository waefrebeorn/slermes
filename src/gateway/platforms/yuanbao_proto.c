/*
 * yuanbao_proto.c - Yuanbao wire protocol codecs.
 *
 * Split from yuanbao.c (soul.md: self-contained modules). Head/frame
 * encoding, the group-query codecs and connection-message decoding. No
 * file-scope state.
 *
 * The encode_auth_bind / encode_ping_req / encode_send_c2c /
 * encode_send_sticker codecs stay in yuanbao.c: they read the connection
 * state g_yb (app_id, app_secret, bot_id, seq_no).
 */
#include "yuanbao_proto.h"
#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "websocket.h"
#include "protobuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>
#include "libhash/hash.h"
#include "libhttp/http.h"
#include "libjson/json.h"


/* ================================================================
 *  Protobuf helpers specific to Yuanbao
 * ================================================================ */

/* Port of Python gateway/platforms/yuanbao_proto.py:_encode_head(). */
/* Encode a Head message */
static int encode_head(uint8_t *buf, size_t buf_len,
                        int cmd_type, const char *cmd,
                        uint32_t seq_no, const char *msg_id,
                        const char *module) {
    int pos = 0;
    int n;

    n = pb_encode_varint_field(buf + pos, buf_len - (size_t)pos,
                               HEAD_CMD_TYPE, (uint64_t)cmd_type);
    if (n <= 0) return -1; pos += n;

    if (cmd && *cmd) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       HEAD_CMD, (const uint8_t *)cmd, strlen(cmd));
        if (n <= 0) return -1; pos += n;
    }

    n = pb_encode_varint_field(buf + pos, buf_len - (size_t)pos,
                               HEAD_SEQ_NO, seq_no);
    if (n <= 0) return -1; pos += n;

    if (msg_id && *msg_id) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       HEAD_MSG_ID, (const uint8_t *)msg_id, strlen(msg_id));
        if (n <= 0) return -1; pos += n;
    }

    if (module && *module) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       HEAD_MODULE, (const uint8_t *)module, strlen(module));
        if (n <= 0) return -1; pos += n;
    }

    return pos;
}


/* Port of Python gateway/platforms/yuanbao_proto.py:encode_conn_msg(). */
/* Encode a ConnMsg: Head + optional Data */
int encode_conn_msg(uint8_t *buf, size_t buf_len,
                            int cmd_type, const char *cmd,
                            uint32_t seq_no, const char *msg_id,
                            const char *module,
                            const uint8_t *data, size_t data_len) {
    int pos = 0;
    int n;

    /* Encode Head as nested message */
    uint8_t head_buf[1024];
    int head_len = encode_head(head_buf, sizeof(head_buf),
                                cmd_type, cmd, seq_no, msg_id, module);
    if (head_len <= 0) return -1;

    n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                   CONN_FIELD_HEAD, head_buf, (size_t)head_len);
    if (n <= 0) return -1; pos += n;

    if (data && data_len > 0) {
        n = pb_encode_delimited_field(buf + pos, buf_len - (size_t)pos,
                                       CONN_FIELD_DATA, data, data_len);
        if (n <= 0) return -1; pos += n;
    }

    return pos;
}


/* Port of Python gateway/platforms/yuanbao_proto.py:encode_query_group_info(). */
/*
 * Encode query_group_info request
 * Proto fields: 1=group_code (string)
 */
int encode_query_group_info(uint8_t *buf, size_t buf_len,
                                    const char *group_code, uint32_t seq_no) {
    uint8_t body[1024];
    int body_pos = 0, n;
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)group_code, strlen(group_code));
    if (n <= 0) return -1; body_pos += n;
    char msg_id[64];
    snprintf(msg_id, sizeof(msg_id), "qgi-%u", seq_no);
    return encode_conn_msg(buf, buf_len, CT_REQUEST, "query_group_info",
                            seq_no, msg_id, BIZ_PKG, body, (size_t)body_pos);
}


/* Port of Python gateway/platforms/yuanbao_proto.py:encode_get_group_member_list(). */
/*
 * Encode get_group_member_list request
 * Proto: 1=group_code (string), 2=offset (uint32), 3=limit (uint32)
 */
int encode_get_group_member_list(uint8_t *buf, size_t buf_len,
                                         const char *group_code,
                                         uint32_t offset,
                                         uint32_t limit,
                                         uint32_t seq_no) {
    uint8_t body[1024];
    int body_pos = 0, n;
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)group_code, strlen(group_code));
    if (n <= 0) return -1; body_pos += n;
    if (offset > 0) {
        n = pb_encode_varint_field(body, sizeof(body), 2, offset);
        if (n <= 0) return -1; body_pos += n;
    }
    n = pb_encode_varint_field(body, sizeof(body), 3, limit);
    if (n <= 0) return -1; body_pos += n;
    char msg_id[64];
    snprintf(msg_id, sizeof(msg_id), "gml-%u", seq_no);
    return encode_conn_msg(buf, buf_len, CT_REQUEST, "get_group_member_list",
                            seq_no, msg_id, BIZ_PKG, body, (size_t)body_pos);
}


/* Decode a ConnMsg — extract head fields and optional data */
/* Port of Python gateway/platforms/yuanbao_proto.py:decode_conn_msg(). */
/* Returns: 0 = decoded, -1 = error */
int decode_conn_msg(const uint8_t *data, size_t data_len,
                            int *out_cmd_type, char *out_cmd, size_t cmd_sz,
                            uint32_t *out_seq_no, char *out_module, size_t mod_sz,
                            const uint8_t **out_body, size_t *out_body_len) {
    *out_body = NULL;
    *out_body_len = 0;

    /* Extract Head message (field 1) */
    size_t head_len;
    const uint8_t *head_data = pb_read_delimited(data, data_len, 1, &head_len);
    if (!head_data) return -1;

    /* Parse head fields */
    if (out_cmd_type) {
        uint64_t v;
        if (pb_read_varint(head_data, head_len, 1, &v))
            *out_cmd_type = (int)v;
    }
    if (out_seq_no) {
        uint64_t v;
        if (pb_read_varint(head_data, head_len, 3, &v))
            *out_seq_no = (uint32_t)v;
    }
    if (out_cmd && cmd_sz > 0) {
        size_t cmd_len;
        const uint8_t *cmd_ptr = pb_read_delimited(head_data, head_len, 2, &cmd_len);
        if (cmd_ptr && cmd_len < cmd_sz) {
            memcpy(out_cmd, cmd_ptr, cmd_len);
            out_cmd[cmd_len] = '\0';
        }
    }
    if (out_module && mod_sz > 0) {
        size_t mod_len;
        const uint8_t *mod_ptr = pb_read_delimited(head_data, head_len, 5, &mod_len);
        if (mod_ptr && mod_len < mod_sz) {
            memcpy(out_module, mod_ptr, mod_len);
            out_module[mod_len] = '\0';
        }
    }

    /* Extract body data (field 2) */
    size_t body_len;
    const uint8_t *body_data = pb_read_delimited(data, data_len, 2, &body_len);
    if (body_data) {
        *out_body = body_data;
        *out_body_len = body_len;
    }

    return 0;
}
