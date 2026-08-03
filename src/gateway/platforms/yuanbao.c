/*
 * yuanbao.c — Gateway platform adapter.
 * Port of Python gateway/platforms/yuanbao.py.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_gateway_yuanbao.h"
#include "websocket.h"
#include "protobuf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <ctype.h>

/* ================================================================
 *  Constants
 * Port of Python gateway/platforms/yuanbao.py.
 * ================================================================ */

#define YB_DEFAULT_WS_URL "wss://yuanbao.example.com/ws"
#define YB_DEFAULT_API_DOMAIN "https://api.yuanbao.example.com"
#define HEARTBEAT_INTERVAL_SEC 30
#define RECONNECT_DELAY_SEC 5
#define MAX_BUF 65536

/* ConnMsg Head fields */
#define HEAD_CMD_TYPE  1  /* varint */
#define HEAD_CMD       2  /* string */
#define HEAD_SEQ_NO    3  /* varint */
#define HEAD_MSG_ID    4  /* string */
#define HEAD_MODULE    5  /* string */

/* ConnMsg fields */
#define CONN_FIELD_HEAD 1  /* message */
#define CONN_FIELD_DATA 2  /* bytes */

/* CMD_TYPE values */
#define CT_REQUEST  0
#define CT_RESPONSE 1
#define CT_PUSH     2
#define CT_PUSH_ACK 3

/* CMD strings */
#define CMD_AUTH_BIND  "auth-bind"
#define CMD_PING       "ping"

/* Biz service */
#define BIZ_PKG "yuanbao_openclaw_proxy"

/* Yuanbao instance ID for protobuf encoding */
#define YB_INSTANCE_ID 17

/* ================================================================
 *  State
 * ================================================================ */

typedef struct {
    char ws_url[1024];
    char app_id[256];
    char app_secret[256];
    char bot_id[256];
    char api_domain[1024];
    bool running;
    ws_t *ws;
    uint32_t seq_no;
} yuanbao_state_t;

static yuanbao_state_t g_yb;
static pthread_mutex_t g_yb_lock = PTHREAD_MUTEX_INITIALIZER;

/* Pending response — synchronous request-response for group queries */
typedef struct {
    bool            pending;
    uint32_t        seq_no;
    uint8_t         body[8192];
    size_t          body_len;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} yb_pending_resp_t;

static yb_pending_resp_t g_yb_resp = {false, 0, {0}, 0,
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

/* Helper: send request and wait for response body. Returns strdup'd body or NULL. */
static char *yuanbao_send_and_wait(const uint8_t *buf, size_t len, uint32_t seq_no, int timeout_sec) {
    pthread_mutex_lock(&g_yb_resp.mutex);
    g_yb_resp.pending = true;
    g_yb_resp.seq_no = seq_no;
    g_yb_resp.body_len = 0;
    pthread_mutex_unlock(&g_yb_resp.mutex);

    pthread_mutex_lock(&g_yb_lock);
    int rc = ws_send(g_yb.ws, WS_OP_BIN, buf, len);
    pthread_mutex_unlock(&g_yb_lock);
    if (rc != 0) {
        pthread_mutex_lock(&g_yb_resp.mutex);
        g_yb_resp.pending = false;
        pthread_mutex_unlock(&g_yb_resp.mutex);
        return NULL;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_sec > 0 ? timeout_sec : 10;

    pthread_mutex_lock(&g_yb_resp.mutex);
    while (g_yb_resp.pending) {
        rc = pthread_cond_timedwait(&g_yb_resp.cond, &g_yb_resp.mutex, &ts);
        if (rc != 0) break;
    }
    char *result = NULL;
    if (!g_yb_resp.pending && g_yb_resp.body_len > 0) {
        result = malloc(g_yb_resp.body_len + 1);
        if (result) { memcpy(result, g_yb_resp.body, g_yb_resp.body_len); result[g_yb_resp.body_len] = '\0'; }
    }
    g_yb_resp.pending = false;
    pthread_mutex_unlock(&g_yb_resp.mutex);
    return result;
}

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
static int encode_conn_msg(uint8_t *buf, size_t buf_len,
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

/* Port of Python gateway/platforms/yuanbao_proto.py:encode_auth_bind(). */
/* Encode AuthBindReq */
static int encode_auth_bind(uint8_t *buf, size_t buf_len) {
    uint8_t body[2048];
    int body_pos = 0;
    int n;

    /* field 1: app_id (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)g_yb.app_id, strlen(g_yb.app_id));
    if (n <= 0) return -1; body_pos += n;

    /* field 2: app_secret (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 2,
                                   (const uint8_t *)g_yb.app_secret, strlen(g_yb.app_secret));
    if (n <= 0) return -1; body_pos += n;

    /* field 3: bot_id (string) */
    if (g_yb.bot_id[0]) {
        n = pb_encode_delimited_field(body, sizeof(body), 3,
                                       (const uint8_t *)g_yb.bot_id, strlen(g_yb.bot_id));
        if (n <= 0) return -1; body_pos += n;
    }

    /* Wrap in ConnMsg */
    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, CMD_AUTH_BIND,
                            g_yb.seq_no++, "auth-1", "conn_access",
                            body, (size_t)body_pos);
}

/* Encode PingReq */
static int encode_ping_req(uint8_t *buf, size_t buf_len) {
    /* Empty body for PingReq */
    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, CMD_PING,
                            g_yb.seq_no++, "ping-1", "conn_access",
                            NULL, 0);
}

/* Encode SendC2CMessageReq for text message */
static int encode_send_c2c(uint8_t *buf, size_t buf_len,
                            const char *to_uid, const char *text, int64_t client_msg_id) {
    uint8_t body[4096];
    int body_pos = 0;
    int n;

    /* field 1: receiver_id (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)to_uid, strlen(to_uid));
    if (n <= 0) return -1; body_pos += n;

    /* field 3: sdk_app_id (uint32) — use YB_INSTANCE_ID */
    n = pb_encode_varint_field(body, sizeof(body), 3, YB_INSTANCE_ID);
    if (n <= 0) return -1; body_pos += n;

    /* field 5: client_msg_id (int64 / sint64) */
    n = pb_encode_varint_field(body, sizeof(body), 5, (uint64_t)client_msg_id);
    if (n <= 0) return -1; body_pos += n;

    /* field 6: msg_body (repeated MsgBodyElement) — one text element */
    uint8_t elem[2048];
    int elem_pos = 0;
    /* msg_type = "TIMTextElem" (field 1) */
    n = pb_encode_delimited_field(elem, sizeof(elem), 1,
                                   (const uint8_t *)"TIMTextElem", 11);
    if (n <= 0) return -1; elem_pos += n;

    /* msg_content.text = text (field 2, nested MsgContent) */
    uint8_t content[2048];
    int content_pos = 0;
    n = pb_encode_delimited_field(content, sizeof(content), 1,
                                   (const uint8_t *)text, strlen(text));
    if (n <= 0) return -1; content_pos += n;

    /* Wrap content as nested message field 2 */
    uint8_t elem_content[2048];
    int ec_pos = pb_encode_delimited_field(elem_content, sizeof(elem_content), 2,
                                            content, (size_t)content_pos);
    if (ec_pos <= 0) return -1;

    /* Add to elem */
    if ((size_t)elem_pos + (size_t)ec_pos > sizeof(elem)) return -1;
    memcpy(elem + elem_pos, elem_content, (size_t)ec_pos);
    elem_pos += ec_pos;

    /* Add elem as field 6 (repeated) */
    n = pb_encode_delimited_field(body + body_pos, sizeof(body) - (size_t)body_pos,
                                   6, elem, (size_t)elem_pos);
    if (n <= 0) return -1; body_pos += n;

    /* field 7: msg_type (uint32) = 0 for C2C */
    n = pb_encode_varint_field(body, sizeof(body), 7, 0);
    if (n <= 0) return -1; body_pos += n;

    /* field 8: instance_id (uint32) */
    n = pb_encode_varint_field(body, sizeof(body), 8, YB_INSTANCE_ID);
    if (n <= 0) return -1; body_pos += n;

    /* Wrap as biz msg */
    char msg_id_str[64];
    snprintf(msg_id_str, sizeof(msg_id_str), "c2c-%ld", (long)client_msg_id);

    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, "send_c2c_message",
                            g_yb.seq_no++, msg_id_str, BIZ_PKG,
                            body, (size_t)body_pos);
}

/* Encode SendC2CMessageReq for sticker (TIMFaceElem) */
static int encode_send_sticker(uint8_t *buf, size_t buf_len,
                                const char *to_uid,
                                const char *sticker_id,
                                const char *sticker_name,
                                const char *package_id,
                                int width, int height,
                                int64_t client_msg_id) {
    uint8_t body[4096];
    int body_pos = 0;
    int n;

    /* field 1: receiver_id (string) */
    n = pb_encode_delimited_field(body, sizeof(body), 1,
                                   (const uint8_t *)to_uid, strlen(to_uid));
    if (n <= 0) return -1; body_pos += n;

    /* field 3: sdk_app_id (uint32) */
    n = pb_encode_varint_field(body, sizeof(body), 3, YB_INSTANCE_ID);
    if (n <= 0) return -1; body_pos += n;

    /* field 5: client_msg_id (int64) */
    n = pb_encode_varint_field(body, sizeof(body), 5, (uint64_t)client_msg_id);
    if (n <= 0) return -1; body_pos += n;

    /* field 6: msg_body (repeated MsgBodyElement) — one TIMFaceElem */
    uint8_t elem[2048];
    int elem_pos = 0;
    /* msg_type = "TIMFaceElem" (field 1) */
    n = pb_encode_delimited_field(elem, sizeof(elem), 1,
                                   (const uint8_t *)"TIMFaceElem", 15);
    if (n <= 0) return -1; elem_pos += n;

    /* msg_content: index=0 (field 1, varint) + data=sticker JSON (field 2, string) */
    uint8_t content[2048];
    int content_pos = 0;
    /* field 1: index = 0 */
    n = pb_encode_varint_field(content, sizeof(content), 1, 0);
    if (n <= 0) return -1; content_pos += n;

    /* field 2: data = sticker metadata JSON */
    char data_json[1024];
    snprintf(data_json, sizeof(data_json),
             "{\"sticker_id\":\"%s\",\"package_id\":\"%s\","
             "\"width\":%d,\"height\":%d,\"formats\":\"png\","
             "\"name\":\"%s\"}",
             sticker_id, package_id,
             width, height,
             sticker_name);
    n = pb_encode_delimited_field(content, sizeof(content), 2,
                                   (const uint8_t *)data_json, strlen(data_json));
    if (n <= 0) return -1; content_pos += n;

    /* Wrap content as nested message field 2 of elem */
    uint8_t elem_content[2048];
    int ec_pos = pb_encode_delimited_field(elem_content, sizeof(elem_content), 2,
                                            content, (size_t)content_pos);
    if (ec_pos <= 0) return -1;

    /* Add to elem */
    if ((size_t)elem_pos + (size_t)ec_pos > sizeof(elem)) return -1;
    memcpy(elem + elem_pos, elem_content, (size_t)ec_pos);
    elem_pos += ec_pos;

    /* Add elem as field 6 (repeated) */
    n = pb_encode_delimited_field(body + body_pos, sizeof(body) - (size_t)body_pos,
                                   6, elem, (size_t)elem_pos);
    if (n <= 0) return -1; body_pos += n;

    /* field 7: msg_type (uint32) = 0 for C2C */
    n = pb_encode_varint_field(body, sizeof(body), 7, 0);
    if (n <= 0) return -1; body_pos += n;

    /* field 8: instance_id (uint32) */
    n = pb_encode_varint_field(body, sizeof(body), 8, YB_INSTANCE_ID);
    if (n <= 0) return -1; body_pos += n;

    /* Wrap as biz msg */
    char msg_id_str[64];
    snprintf(msg_id_str, sizeof(msg_id_str), "sticker-%ld", (long)client_msg_id);

    return encode_conn_msg(buf, buf_len,
                            CT_REQUEST, "send_c2c_message",
                            g_yb.seq_no++, msg_id_str, BIZ_PKG,
                            body, (size_t)body_pos);
}

/* Port of Python gateway/platforms/yuanbao_proto.py:encode_query_group_info(). */
/*
 * Encode query_group_info request
 * Proto fields: 1=group_code (string)
 */
static int encode_query_group_info(uint8_t *buf, size_t buf_len,
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
static int encode_get_group_member_list(uint8_t *buf, size_t buf_len,
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
static int decode_conn_msg(const uint8_t *data, size_t data_len,
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

/* ================================================================
 *  Message processing
 * ================================================================ */

/* Extract text from InboundMessagePush body */
static void extract_push_text(const uint8_t *body, size_t body_len,
                               char *text_out, size_t text_sz,
                               char *sender_out, size_t sender_sz) {
    text_out[0] = '\0';
    sender_out[0] = '\0';

    /* field 2: sender_id (string) */
    size_t slen;
    const uint8_t *sptr = pb_read_delimited(body, body_len, 2, &slen);
    if (sptr && slen < sender_sz - 1) {
        memcpy(sender_out, sptr, slen);
        sender_out[slen] = '\0';
    }

    /* field 6: msg_body (repeated MsgBodyElement) */
    /* Each element has field 1: msg_type (string), field 2: msg_content (message) */
    /* We need to iterate the repeated field 6 manually */
    size_t offset = 0;
    while (offset < body_len) {
        uint32_t tag_no;
        int wire_type;
        int n = pb_parse_tag(body + offset, body_len - offset, &tag_no, &wire_type);
        if (n <= 0) break;
        offset += (size_t)n;

        if (tag_no == 6 && wire_type == PB_WIRE_LENDELIM) {
            uint64_t elem_len;
            int m = pb_decode_varint(body + offset, body_len - offset, &elem_len);
            if (m <= 0) break;
            offset += (size_t)m;

            if (offset + (size_t)elem_len > body_len) break;

            const uint8_t *elem_data = body + offset;
            size_t elem_data_len = (size_t)elem_len;
            offset += elem_data_len;

            /* Check msg_type is TIMTextElem */
            size_t mt_len;
            const uint8_t *mt = pb_read_delimited(elem_data, elem_data_len, 1, &mt_len);
            if (mt && mt_len == 11 && memcmp(mt, "TIMTextElem", 11) == 0) {
                /* Extract msg_content (field 2) */
                size_t mc_len;
                const uint8_t *mc = pb_read_delimited(elem_data, elem_data_len, 2, &mc_len);
                if (mc) {
                    /* Extract text (field 1 of MsgContent) */
                    size_t txt_len;
                    const uint8_t *txt = pb_read_delimited(mc, mc_len, 1, &txt_len);
                    if (txt && txt_len < text_sz - 1) {
                        memcpy(text_out, txt, txt_len);
                        text_out[txt_len] = '\0';
                    }
                }
            }
        } else {
            int skipped = pb_skip_field(body + offset, body_len - offset, wire_type);
            if (skipped <= 0) break;
            offset += (size_t)skipped;
        }
    }
}

/* ================================================================
 *  WS event loop
 * ================================================================ */

static void yuanbao_event_loop(void) {
    ws_frame_t frame;

    printf("[gateway:yuanbao] Connected, sending AUTH_BIND...\n");

    /* Send AUTH_BIND */
    uint8_t auth_buf[4096];
    int auth_len = encode_auth_bind(auth_buf, sizeof(auth_buf));
    if (auth_len <= 0) {
        fprintf(stderr, "[gateway:yuanbao] Failed to encode AUTH_BIND\n");
        return;
    }
    ws_send(g_yb.ws, WS_OP_BIN, auth_buf, (size_t)auth_len);

    /* Main loop: receive messages + periodic heartbeat */
    time_t last_heartbeat = time(NULL);
    char cmd_buf[128], module_buf[128];

    while (g_yb.running) {
        /* Check for heartbeat */
        time_t now = time(NULL);
        if (now - last_heartbeat >= HEARTBEAT_INTERVAL_SEC) {
            printf("[gateway:yuanbao] Sending heartbeat...\n");
            uint8_t ping_buf[256];
            int ping_len = encode_ping_req(ping_buf, sizeof(ping_buf));
            if (ping_len > 0) {
                ws_send(g_yb.ws, WS_OP_BIN, ping_buf, (size_t)ping_len);
            }
            last_heartbeat = now;
        }

        /* Wait for incoming frame with 5s timeout */
        int rc = ws_recv(g_yb.ws, &frame, 5);
        if (rc == 0) continue; /* timeout — loop back to check heartbeat */
        if (rc < 0) {
            fprintf(stderr, "[gateway:yuanbao] WS recv error\n");
            break;
        }

        if (frame.opcode == WS_OP_CLOSE) {
            printf("[gateway:yuanbao] Connection closed by server\n");
            ws_frame_free(&frame);
            break;
        }

        if (frame.opcode == WS_OP_BIN) {
            /* Decode ConnMsg */
            int cmd_type = -1;
            uint32_t seq_no = 0;
            const uint8_t *body = NULL;
            size_t body_len = 0;
            cmd_buf[0] = '\0';
            module_buf[0] = '\0';

            int dc = decode_conn_msg(frame.payload, frame.len,
                                      &cmd_type, cmd_buf, sizeof(cmd_buf),
                                      &seq_no, module_buf, sizeof(module_buf),
                                      &body, &body_len);
            ws_frame_free(&frame);

            if (dc < 0) continue;

            printf("[gateway:yuanbao] Recv: cmd=%s cmd_type=%d seq=%u\n",
                   cmd_buf, cmd_type, seq_no);

            if (cmd_type == CT_PUSH && strcmp(cmd_buf, "InboundMessagePush") == 0) {
                /* Extract text from push */
                char text[4096], sender[256];
                extract_push_text(body, body_len, text, sizeof(text), sender, sizeof(sender));

                if (text[0] && sender[0]) {
                    printf("[gateway:yuanbao] Message from %s: %.200s\n", sender, text);

                    /* Send PushAck (cmd_type = 3) */
                    uint8_t ack_buf[256];
                    int ack_len = encode_conn_msg(ack_buf, sizeof(ack_buf),
                                                   CT_PUSH_ACK, cmd_buf,
                                                   g_yb.seq_no++, "ack-1", module_buf,
                                                   body, body_len);
                    if (ack_len > 0)
                        ws_send(g_yb.ws, WS_OP_BIN, ack_buf, (size_t)ack_len);

                    /* Forward to agent */
                    extern gateway_state_t g_gw;
                    pthread_mutex_lock(&g_gw.agent_mutex);
                    char *resp = agent_chat(&g_gw.agent, text);
                    pthread_mutex_unlock(&g_gw.agent_mutex);

                    if (resp) {
                        /* Send reply via C2C message */
                        uint8_t reply_buf[8192];
                        int64_t client_id = (int64_t)(now * 1000);
                        int rlen = encode_send_c2c(reply_buf, sizeof(reply_buf),
                                                    sender, resp, client_id);
                        if (rlen > 0) {
                            ws_send(g_yb.ws, WS_OP_BIN, reply_buf, (size_t)rlen);
                            printf("[gateway:yuanbao] Reply sent via C2C\n");
                        }
                        free(resp);
                    }
                }
            } else if (cmd_type == CT_RESPONSE) {
                /* Check for pending query response match by seq_no */
                pthread_mutex_lock(&g_yb_resp.mutex);
                if (g_yb_resp.pending && g_yb_resp.seq_no == seq_no && body && body_len > 0) {
                    size_t copy_len = body_len < sizeof(g_yb_resp.body) ? body_len : sizeof(g_yb_resp.body);
                    memcpy(g_yb_resp.body, body, copy_len);
                    g_yb_resp.body_len = copy_len;
                    g_yb_resp.pending = false;
                    pthread_cond_signal(&g_yb_resp.cond);
                    pthread_mutex_unlock(&g_yb_resp.mutex);
                    printf("[gateway:yuanbao] Query response matched seq=%u\n", seq_no);
                } else if (strcmp(cmd_buf, CMD_PING) == 0) {
                    pthread_mutex_unlock(&g_yb_resp.mutex);
                } else if (strcmp(cmd_buf, CMD_AUTH_BIND) == 0) {
                    printf("[gateway:yuanbao] AUTH_BIND response received (seq=%u)\n", seq_no);
                    pthread_mutex_unlock(&g_yb_resp.mutex);
                } else {
                    pthread_mutex_unlock(&g_yb_resp.mutex);
                }
            }
        } else {
            ws_frame_free(&frame);
        }
    }
}

/* ================================================================
 *  Public API
 * ================================================================ */

bool yuanbao_init(const char *app_id, const char *app_secret,
                   const char *bot_id, const char *ws_url,
                   const char *api_domain) {
    memset(&g_yb, 0, sizeof(g_yb));
    if (app_id) snprintf(g_yb.app_id, sizeof(g_yb.app_id), "%s", app_id);
    if (app_secret) snprintf(g_yb.app_secret, sizeof(g_yb.app_secret), "%s", app_secret);
    if (bot_id) snprintf(g_yb.bot_id, sizeof(g_yb.bot_id), "%s", bot_id);
    snprintf(g_yb.ws_url, sizeof(g_yb.ws_url), "%s", ws_url ? ws_url : YB_DEFAULT_WS_URL);
    snprintf(g_yb.api_domain, sizeof(g_yb.api_domain), "%s", api_domain ? api_domain : YB_DEFAULT_API_DOMAIN);
    g_yb.seq_no = 1;
    return *g_yb.app_id && *g_yb.app_secret;
}

/* PoP: start @ gateway/platforms/yuanbao.py:start */
/* PoP: connect @ gateway/platforms/yuanbao.py:connect */
void yuanbao_start(void) {
    if (!*g_yb.app_id || !*g_yb.app_secret) {
        fprintf(stderr, "[gateway:yuanbao] app_id and app_secret required\n");
        return;
    }

    while (g_yb.running) {
        printf("[gateway:yuanbao] Connecting to %s...\n", g_yb.ws_url);

        g_yb.ws = ws_connect(g_yb.ws_url, 30);
        if (!g_yb.ws) {
            fprintf(stderr, "[gateway:yuanbao] WS connect failed, retry in %ds\n",
                    RECONNECT_DELAY_SEC);
            for (int i = 0; i < RECONNECT_DELAY_SEC && g_yb.running; i++) sleep(1);
            continue;
        }

        yuanbao_event_loop();

        ws_close(g_yb.ws);
        g_yb.ws = NULL;

        if (g_yb.running) {
            printf("[gateway:yuanbao] Disconnected, reconnecting in %ds...\n",
                   RECONNECT_DELAY_SEC);
            for (int i = 0; i < RECONNECT_DELAY_SEC && g_yb.running; i++) sleep(1);
        }
    }
}

/* PoP: stop @ gateway/platforms/yuanbao.py:stop */
/* PoP: close @ gateway/platforms/yuanbao.py:close */
void yuanbao_stop(void) {
    g_yb.running = false;
}

/* Send a sticker via the active WebSocket connection. Returns 0 on success, -1 on error. */
/* PoP: send_sticker @ gateway/platforms/yuanbao.py:send_sticker */
/* Port of Python gateway/platforms/yuanbao.py:send_sticker(). */
/* PoP: send @ gateway/platforms/yuanbao.py:send */
int yuanbao_send_sticker(const char *to_uid, const char *sticker_id,
                          const char *sticker_name, const char *package_id,
                          int width, int height) {
    if (!g_yb.ws || !g_yb.running) return -1;

    int64_t now_ms;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    now_ms = (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);

    uint8_t buf[8192];
    int len = encode_send_sticker(buf, sizeof(buf), to_uid,
                                   sticker_id, sticker_name,
                                   package_id, width, height,
                                   now_ms);
    if (len <= 0) return -1;

    pthread_mutex_lock(&g_yb_lock);
    int rc = ws_send(g_yb.ws, WS_OP_BIN, buf, (size_t)len);
    pthread_mutex_unlock(&g_yb_lock);

    return (rc == 0) ? 0 : -1;
}

/* Query group info synchronously. Returns JSON string or NULL. Caller must free(). */
/* PoP: query_group_info @ gateway/platforms/yuanbao.py:query_group_info */
/* Port of Python gateway/platforms/yuanbao.py:query_group_info(). */
char *yuanbao_query_group_info(const char *group_code, int timeout_sec) {
    if (!g_yb.ws || !g_yb.running || !group_code || !*group_code) return NULL;
    uint32_t seq_no;
    pthread_mutex_lock(&g_yb_lock); seq_no = g_yb.seq_no++; pthread_mutex_unlock(&g_yb_lock);
    uint8_t buf[4096];
    int len = encode_query_group_info(buf, sizeof(buf), group_code, seq_no);
    if (len <= 0) return NULL;
    return yuanbao_send_and_wait(buf, (size_t)len, seq_no, timeout_sec);
}

/* Query group members synchronously. Returns JSON string or NULL. Caller must free(). */
/* PoP: get_group_member_list @ gateway/platforms/yuanbao.py:get_group_member_list */
/* Port of Python gateway/platforms/yuanbao.py:get_group_member_list(). */
char *yuanbao_get_group_member_list(const char *group_code, uint32_t offset, uint32_t limit, int timeout_sec) {
    if (!g_yb.ws || !g_yb.running || !group_code || !*group_code) return NULL;
    uint32_t seq_no;
    pthread_mutex_lock(&g_yb_lock); seq_no = g_yb.seq_no++; pthread_mutex_unlock(&g_yb_lock);
    uint8_t buf[4096];
    int len = encode_get_group_member_list(buf, sizeof(buf), group_code, offset, limit, seq_no);
    if (len <= 0) return NULL;
    return yuanbao_send_and_wait(buf, (size_t)len, seq_no, timeout_sec);
}

/* Send a DM (C2C text message). Returns JSON string or NULL. Caller must free(). */
/* PoP: send_dm @ gateway/platforms/yuanbao.py:send_dm */
/* Port of Python gateway/platforms/yuanbao.py:send_dm(). */
char *yuanbao_send_dm(const char *to_uid, const char *text) {
    if (!g_yb.ws || !g_yb.running || !to_uid || !*to_uid || !text) return NULL;
    int64_t now_ms;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    now_ms = (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
    uint8_t buf[8192];
    int len = encode_send_c2c(buf, sizeof(buf), to_uid, text, now_ms);
    if (len <= 0) return NULL;
    pthread_mutex_lock(&g_yb_lock);
    int rc = ws_send(g_yb.ws, WS_OP_BIN, buf, (size_t)len);
    pthread_mutex_unlock(&g_yb_lock);
    if (rc != 0) return NULL;
    return strdup("{\"success\":true,\"note\":\"DM sent successfully.\"}");
}

/* ================================================================
 *  MarkdownProcessor — Pure string algorithms ported from
 *  gateway/platforms/yuanbao.py class MarkdownProcessor.
 *  All methods are static/class methods with no async, no network.
 * ================================================================ */

/* Check whether text has an unclosed code fence (odd number of ``` lines). */
/* PoP: yuanbao_md_has_unclosed_fence @ gateway/platforms/yuanbao.py:has_unclosed_fence */
bool yuanbao_md_has_unclosed_fence(const char *text)
{
    if (!text) return false;
    bool in_fence = false;
    const char *p = text;
    while (*p) {
        /* Find start of line */
        const char *line = p;
        const char *eol = strchr(p, '\n');
        size_t line_len = eol ? (size_t)(eol - p) : strlen(p);
        /* Check if line starts with ``` */
        if (line_len >= 3 && strncmp(line, "```", 3) == 0) {
            in_fence = !in_fence;
        }
        if (!eol) break;
        p = eol + 1;
    }
    return in_fence;
}

/* Check if the last non-empty line starts and ends with | (table row). */
/* PoP: yuanbao_md_ends_with_table_row @ gateway/platforms/yuanbao.py:ends_with_table_row */
bool yuanbao_md_ends_with_table_row(const char *text)
{
    if (!text || !*text) return false;
    /* rstrip: find last non-whitespace */
    size_t len = strlen(text);
    while (len > 0 && (text[len-1] == ' ' || text[len-1] == '\t' || text[len-1] == '\r' || text[len-1] == '\n'))
        len--;
    if (len == 0) return false;
    /* Find last line within the trimmed range */
    const char *last_line_start = text + len - 1;
    while (last_line_start > text && last_line_start[-1] != '\n') last_line_start--;
    /* strip leading whitespace of last line */
    const char *s = last_line_start;
    while (s < text + len && (*s == ' ' || *s == '\t')) s++;
    size_t s_len = (text + len) - s;
    if (s_len < 2) return false;
    /* strip trailing whitespace of last line */
    while (s_len > 0 && (s[s_len-1] == ' ' || s[s_len-1] == '\t' || s[s_len-1] == '\r')) s_len--;
    if (s_len < 2) return false;
    return s[0] == '|' && s[s_len-1] == '|';
}

/* Split text at nearest paragraph boundary within max_chars.
   Returns head (caller-free) and sets *tail_out (caller-free). */
/* PoP: yuanbao_md_split_at_paragraph_boundary @ gateway/platforms/yuanbao.py:split_at_paragraph_boundary */
char *yuanbao_md_split_at_paragraph_boundary(const char *text, size_t max_chars, char **tail_out)
{
    if (!text || !tail_out) return NULL;
    *tail_out = NULL;
    size_t total = strlen(text);
    if ((int)total <= max_chars) {
        char *head = strdup(text);
        return head;
    }
    int window_len = max_chars < (int)total ? max_chars : (int)total;
    const char *window_end = text + window_len;

    /* 1. Prefer last blank line (\n\n) in window */
    const char *pos = NULL;
    for (const char *q = text; q + 1 < window_end; q++) {
        if (q[0] == '\n' && q[1] == '\n') pos = q;
    }
    if (pos && pos > text) {
        size_t head_len = (size_t)(pos - text) + 2;
        char *head = malloc(head_len + 1);
        memcpy(head, text, head_len);
        head[head_len] = '\0';
        *tail_out = strdup(pos + 2);
        return head;
    }

    /* 2. Last sentence-ending punctuation followed by newline: 。！？.!? */
    const char *best_pos = NULL;
    for (const char *q = text; q < window_end; q++) {
        char c = *q;
        if ((c == '.' || c == '!' || c == '?' || c == '\xe3') && q + 1 < window_end && q[1] == '\n') {
            /* Check for Chinese punctuation: 0xE3 0x80 0x82/0x90/0x9A (。？！) */
            if (c == '\xe3' && q + 2 < window_end) {
                unsigned char c2 = (unsigned char)q[1];
                unsigned char c3 = (unsigned char)q[2];
                if (c2 == '\x80' && (c3 == '\x82' || c3 == '\x90' || c3 == '\x9A')) {
                    best_pos = q + 3; /* after the 3-byte CJK char */
                }
            } else {
                best_pos = q + 1; /* after the ASCII punct */
            }
        }
    }
    if (best_pos && best_pos > text) {
        size_t head_len = (size_t)(best_pos - text);
        char *head = malloc(head_len + 1);
        memcpy(head, text, head_len);
        head[head_len] = '\0';
        *tail_out = strdup(best_pos);
        return head;
    }

    /* 3. Fallback: last newline in window */
    pos = NULL;
    for (const char *q = text; q < window_end; q++) {
        if (*q == '\n') pos = q;
    }
    if (pos && pos > text) {
        size_t head_len = (size_t)(pos - text) + 1;
        char *head = malloc(head_len + 1);
        memcpy(head, text, head_len);
        head[head_len] = '\0';
        *tail_out = strdup(pos + 1);
        return head;
    }

    /* 4. Force split at window boundary */
    char *head = malloc((size_t)window_len + 1);
    memcpy(head, text, window_len);
    head[window_len] = '\0';
    *tail_out = strdup(text + window_len);
    return head;
}

/* Check if text (atom) is a code fence block (starts with ```). */
/* PoP: yuanbao_md_is_fence_atom @ gateway/platforms/yuanbao.py:is_fence_atom */
bool yuanbao_md_is_fence_atom(const char *text)
{
    if (!text) return false;
    while (*text == ' ' || *text == '\t') text++;
    return strncmp(text, "```", 3) == 0;
}

/* Check if text (atom) is a table (first line starts and ends with |). */
/* PoP: yuanbao_md_is_table_atom @ gateway/platforms/yuanbao.py:is_table_atom */
bool yuanbao_md_is_table_atom(const char *text)
{
    if (!text) return false;
    const char *eol = strchr(text, '\n');
    size_t first_len = eol ? (size_t)(eol - text) : strlen(text);
    /* strip leading whitespace */
    const char *s = text;
    while (s < text + first_len && (*s == ' ' || *s == '\t')) s++;
    size_t s_len = (text + first_len) - s;
    /* strip trailing whitespace */
    while (s_len > 0 && (s[s_len-1] == ' ' || s[s_len-1] == '\t' || s[s_len-1] == '\r')) s_len--;
    if (s_len < 2) return false;
    return s[0] == '|' && s[s_len-1] == '|';
}

/* Split text into atoms (code fences, tables, paragraphs separated by blank lines).
   Returns a dynamically-allocated NULL-terminated array of strings. Caller frees each + array. */
/* PoP: yuanbao_md_split_into_atoms @ gateway/platforms/yuanbao.py:split_into_atoms */
char **yuanbao_md_split_into_atoms(const char *text)
{
    if (!text) {
        char **arr = calloc(1, sizeof(char*));
        return arr;
    }
    size_t cap = 16, count = 0;
    char **atoms = calloc(cap, sizeof(char*));

    /* current_lines buffer */
    char *buf = NULL;
    size_t buf_len = 0, buf_cap = 0;
    bool in_fence = false;

    #define YB_FLUSH() do { \
        if (buf_len > 0) { \
            /* Check non-whitespace */ \
            bool has_content = false; \
            for (size_t _i = 0; _i < buf_len; _i++) if (!isspace((unsigned char)buf[_i])) { has_content = true; break; } \
            if (has_content) { \
                if (count >= cap - 1) { cap *= 2; atoms = realloc(atoms, cap * sizeof(char*)); } \
                /* Python joins lines with '\n' and the atom itself has NO trailing \
                 * newline, so strip the trailing '\n' that YB_APPEND_LINE added. */ \
                size_t out_len = buf_len; \
                if (out_len > 0 && buf[out_len - 1] == '\n') out_len--; \
                atoms[count++] = strndup(buf, out_len); \
            } \
            buf_len = 0; \
        } \
    } while(0)

    #define YB_APPEND_LINE(start, len) do { \
        if (buf_len + (len) + 1 > buf_cap) { \
            buf_cap = (buf_len + (len) + 1) * 2; if (buf_cap < 256) buf_cap = 256; \
            buf = realloc(buf, buf_cap); \
        } \
        if (len > 0) memcpy(buf + buf_len, start, len); \
        buf_len += len; \
        buf[buf_len++] = '\n'; \
    } while(0)

    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t ll = eol ? (size_t)(eol - p) : strlen(p);

        /* helper: is table line */
        bool is_tbl = false;
        {
            const char *s = p; size_t sl = ll;
            while (sl > 0 && (*s == ' ' || *s == '\t')) { s++; sl--; }
            while (sl > 0 && (s[sl-1] == ' ' || s[sl-1] == '\t' || s[sl-1] == '\r')) sl--;
            is_tbl = (sl >= 2 && s[0] == '|' && s[sl-1] == '|');
        }

        if (in_fence) {
            YB_APPEND_LINE(p, ll);
            if (ll >= 3 && strncmp(p, "```", 3) == 0 && buf_len > ll + 1) {
                in_fence = false;
                YB_FLUSH();
            }
        } else if (ll >= 3 && strncmp(p, "```", 3) == 0) {
            YB_FLUSH();
            in_fence = true;
            YB_APPEND_LINE(p, ll);
        } else if (is_tbl) {
            /* If previous line in buf was not table, flush */
            if (buf_len > 0) {
                /* Find last line in buf */
                size_t last_start = buf_len - 1;
                while (last_start > 0 && buf[last_start - 1] != '\n') last_start--;
                size_t last_real_len = buf_len - 1 - last_start;
                const char *ls = buf + last_start;
                while (last_real_len > 0 && (*ls == ' ' || *ls == '\t')) { ls++; last_real_len--; }
                while (last_real_len > 0 && (ls[last_real_len-1] == ' ' || ls[last_real_len-1] == '\t' || ls[last_real_len-1] == '\r')) last_real_len--;
                bool last_was_tbl = (last_real_len >= 2 && ls[0] == '|' && ls[last_real_len-1] == '|');
                if (!last_was_tbl) YB_FLUSH();
            }
            YB_APPEND_LINE(p, ll);
        } else {
            /* blank? */
            bool blank = true;
            for (size_t i = 0; i < ll; i++) if (!isspace((unsigned char)p[i])) { blank = false; break; }
            if (blank) {
                YB_FLUSH();
            } else {
                if (buf_len > 0) {
                    /* If previous was table, flush */
                    size_t last_start = buf_len - 1;
                    while (last_start > 0 && buf[last_start - 1] != '\n') last_start--;
                    size_t last_real_len = buf_len - 1 - last_start;
                    const char *ls = buf + last_start;
                    while (last_real_len > 0 && (*ls == ' ' || *ls == '\t')) { ls++; last_real_len--; }
                    while (last_real_len > 0 && (ls[last_real_len-1] == ' ' || ls[last_real_len-1] == '\t' || ls[last_real_len-1] == '\r')) last_real_len--;
                    bool last_was_tbl = (last_real_len >= 2 && ls[0] == '|' && ls[last_real_len-1] == '|');
                    if (last_was_tbl) YB_FLUSH();
                }
                YB_APPEND_LINE(p, ll);
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    YB_FLUSH();
    free(buf);

    atoms[count] = NULL;
    return atoms;

    #undef YB_FLUSH
    #undef YB_APPEND_LINE
}

/* Infer separator between two chunks: "\n" if fence/table continuation, else "\n\n". */
/* PoP: yuanbao_md_infer_block_separator @ gateway/platforms/yuanbao.py:infer_block_separator */
char *yuanbao_md_infer_block_separator(const char *prev_chunk, const char *next_chunk)
{
    if (!prev_chunk || !next_chunk) return strdup("\n\n");
    /* rstrip prev */
    size_t plen = strlen(prev_chunk);
    while (plen > 0 && isspace((unsigned char)prev_chunk[plen-1])) plen--;
    /* lstrip next */
    const char *ns = next_chunk;
    while (*ns == ' ' || *ns == '\t' || *ns == '\n') ns++;
    /* prev ends with ``` or next starts with ``` */
    if (plen >= 3 && strncmp(prev_chunk + plen - 3, "```", 3) == 0) return strdup("\n");
    if (strncmp(ns, "```", 3) == 0) return strdup("\n");
    /* table continuation */
    if (yuanbao_md_ends_with_table_row(prev_chunk)) {
        const char *eol = strchr(ns, '\n');
        size_t fl_len = eol ? (size_t)(eol - ns) : strlen(ns);
        const char *fs = ns;
        while (fs < ns + fl_len && (*fs == ' ' || *fs == '\t')) fs++;
        size_t fs_len = (ns + fl_len) - fs;
        while (fs_len > 0 && (fs[fs_len-1] == ' ' || fs[fs_len-1] == '\t' || fs[fs_len-1] == '\r')) fs_len--;
        if (fs_len >= 2 && fs[0] == '|' && fs[fs_len-1] == '|') return strdup("\n");
    }
    return strdup("\n\n");
}

/* Strip outer markdown fence when entire text is ```markdown\n...\n``` */
/* PoP: yuanbao_md_strip_outer_markdown_fence @ gateway/platforms/yuanbao.py:strip_outer_markdown_fence */
char *yuanbao_md_strip_outer_markdown_fence(const char *text)
{
    if (!text || !*text) return text ? strdup(text) : NULL;
    /* Count lines */
    int nlines = 1;
    for (const char *p = text; *p; p++) if (*p == '\n') nlines++;
    if (nlines < 3) return strdup(text);

    /* Find first and last line */
    const char *first = text;
    const char *first_end = strchr(first, '\n');
    if (!first_end) return strdup(text);
    size_t first_len = first_end - first;
    /* rstrip first line */
    while (first_len > 0 && (first[first_len-1] == ' ' || first[first_len-1] == '\t' || first[first_len-1] == '\r')) first_len--;

    /* Check first line matches ^```(?:markdown|md)?\s*$ (case-insensitive) */
    if (first_len < 3 || strncmp(first, "```", 3) != 0) return strdup(text);
    /* Remaining optional tag */
    const char *tag = first + 3;
    size_t tag_len = first_len - 3;
    /* skip trailing whitespace already stripped */
    bool match_md = false;
    if (tag_len == 0) {
        match_md = true; /* plain ``` */
    } else {
        /* case-insensitive match "markdown" or "md" */
        if (tag_len == 2 && (tag[0] == 'm' || tag[0] == 'M') && (tag[1] == 'd' || tag[1] == 'D'))
            match_md = true;
        else if (tag_len == 8 && strncasecmp(tag, "markdown", 8) == 0)
            match_md = true;
    }
    if (!match_md) return strdup(text);

    /* Find last line */
    const char *p = text + strlen(text);
    while (p > text && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t')) p--;
    /* p points to end of last non-empty content */
    const char *last_start = p;
    while (last_start > text && last_start[-1] != '\n') last_start--;
    size_t last_len = p - last_start;
    /* strip whitespace of last line */
    const char *ls = last_start;
    while (ls < last_start + last_len && (*ls == ' ' || *ls == '\t')) ls++;
    size_t ls_len = (last_start + last_len) - ls;
    while (ls_len > 0 && (ls[ls_len-1] == ' ' || ls[ls_len-1] == '\t' || ls[ls_len-1] == '\r')) ls_len--;
    if (ls_len != 3 || strncmp(ls, "```", 3) != 0) return strdup(text);

    /* Strip first and last lines, return inner content */
    const char *inner_start = first_end + 1;
    size_t inner_len = last_start > inner_start ? (size_t)(last_start - inner_start) : 0;
    /* Remove trailing newline before last_start if present */
    if (inner_len > 0 && inner_start[inner_len - 1] == '\n') inner_len--;
    char *result = malloc(inner_len + 1);
    memcpy(result, inner_start, inner_len);
    result[inner_len] = '\0';
    return result;
}

/* Sanitize markdown table: normalize separator rows, remove empty table rows. */
/* PoP: yuanbao_md_sanitize_markdown_table @ gateway/platforms/yuanbao.py:sanitize_markdown_table */
char *yuanbao_md_sanitize_markdown_table(const char *text)
{
    if (!text) return NULL;
    if (!strchr(text, '|')) return strdup(text);

    /* Build result line by line */
    size_t cap = strlen(text) + 256;
    char *result = malloc(cap);
    size_t rlen = 0;

    const char *p = text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t ll = eol ? (size_t)(eol - p) : strlen(p);
        /* strip the line */
        const char *ls = p;
        while (ls < p + ll && (*ls == ' ' || *ls == '\t')) ls++;
        size_t ls_len = (p + ll) - ls;
        while (ls_len > 0 && (ls[ls_len-1] == ' ' || ls[ls_len-1] == '\t' || ls[ls_len-1] == '\r')) ls_len--;

        bool is_table_row = (ls_len >= 2 && ls[0] == '|' && ls[ls_len-1] == '|');

        if (is_table_row) {
            /* Check if separator row: ^|[\s\-:]+(\|[\s\-:]+)+\|$ */
            bool is_sep = true;
            if (ls_len < 2) is_sep = false;
            /* Must contain only -, :, space, | */
            for (size_t i = 0; i < ls_len && is_sep; i++) {
                char c = ls[i];
                if (c != '-' && c != ':' && c != ' ' && c != '\t' && c != '|') is_sep = false;
            }
            if (is_sep) {
                /* Normalize: split by |, strip cells, rejoin, preserving the
                 * leading/trailing pipe (Python's stripped.split('|') keeps the
                 * empty edge cells, so the result is |cell|cell|...|). */
                char *scopy = strndup(ls, ls_len);
                char *norm = malloc(ls_len + 1);
                size_t nlen = 0;
                norm[nlen++] = '|';  /* leading pipe */
                char *save = NULL;
                char *tok = strtok_r(scopy, "|", &save);
                bool first = true;
                while (tok) {
                    char *ts = tok; while (*ts == ' ' || *ts == '\t') ts++;
                    char *te = ts + strlen(ts);
                    while (te > ts && (te[-1] == ' ' || te[-1] == '\t')) te--;
                    if (!first) norm[nlen++] = '|';
                    memcpy(norm + nlen, ts, te - ts); nlen += te - ts;
                    first = false;
                    tok = strtok_r(NULL, "|", &save);
                }
                norm[nlen++] = '|';  /* trailing pipe */
                norm[nlen] = '\0';
                free(scopy);
                /* Append normalized line */
                if (rlen + nlen + 1 > cap) { cap = (rlen + nlen + 1) * 2; result = realloc(result, cap); }
                memcpy(result + rlen, norm, nlen); rlen += nlen;
                free(norm);
            } else if (ls_len <= 2 || ls_len == 2) {
                /* Empty table row || → skip */
                /* Check if all non-| chars are whitespace */
                bool only_pipe_ws = true;
                for (size_t i = 0; i < ls_len; i++) {
                    if (ls[i] != '|' && !isspace((unsigned char)ls[i])) { only_pipe_ws = false; break; }
                }
                if (only_pipe_ws) {
                    /* skip empty table row */
                } else {
                    if (rlen + ls_len + 1 > cap) { cap = (rlen + ls_len + 1) * 2; result = realloc(result, cap); }
                    memcpy(result + rlen, ls, ls_len); rlen += ls_len;
                }
            } else {
                /* Non-empty, non-separator: append stripped */
                if (rlen + ls_len + 1 > cap) { cap = (rlen + ls_len + 1) * 2; result = realloc(result, cap); }
                memcpy(result + rlen, ls, ls_len); rlen += ls_len;
            }
        } else {
            /* Non-table line: append as-is (original, not stripped) */
            if (rlen + ll + 1 > cap) { cap = (rlen + ll + 1) * 2; result = realloc(result, cap); }
            memcpy(result + rlen, p, ll); rlen += ll;
        }
        if (eol) {
            if (rlen + 1 > cap) { cap = (rlen + 1) * 2; result = realloc(result, cap); }
            result[rlen++] = '\n';
            p = eol + 1;
        } else {
            break;
        }
    }
    result[rlen] = '\0';
    return result;
}

/* Return the markdown hint system prompt string (static). */
/* PoP: yuanbao_md_markdown_hint_system_prompt @ gateway/platforms/yuanbao.py:markdown_hint_system_prompt */
const char *yuanbao_md_markdown_hint_system_prompt(void)
{
    return "The current platform supports Markdown rendering. You can use the following formats:\n"
           "- Code blocks: ```language\\ncode\\n```\n"
           "- Tables: | col1 | col2 |\\n|---|---|\\n| val1 | val2 |\n"
           "- Bold: **text** / Italic: *text*\n"
           "Please use Markdown formatting when appropriate to improve readability.";
}

/* Merge adjacent chunks when current has unclosed fence and next starts with ```.
   Returns merged list (NULL-terminated array, caller frees each + array). */
/* PoP: yuanbao_md_merge_block_streaming_fences @ gateway/platforms/yuanbao.py:merge_block_streaming_fences */
char **yuanbao_md_merge_block_streaming_fences(char **chunks)
{
    if (!chunks || !chunks[0]) {
        char **arr = calloc(1, sizeof(char*));
        return arr;
    }
    size_t cap = 16, count = 0;
    char **result = calloc(cap, sizeof(char*));
    size_t i = 0;
    while (chunks[i]) {
        char *current = strdup(chunks[i]);
        while (yuanbao_md_has_unclosed_fence(current) && chunks[i + 1]) {
            char *sep = yuanbao_md_infer_block_separator(current, chunks[i + 1]);
            size_t clen = strlen(current);
            size_t slen = strlen(sep);
            size_t nlen = strlen(chunks[i + 1]);
            char *merged = malloc(clen + slen + nlen + 1);
            memcpy(merged, current, clen);
            memcpy(merged + clen, sep, slen);
            memcpy(merged + clen + slen, chunks[i + 1], nlen);
            merged[clen + slen + nlen] = '\0';
            free(current);
            free(sep);
            current = merged;
            i++;
        }
        if (count >= cap - 1) { cap *= 2; result = realloc(result, cap * sizeof(char*)); }
        result[count++] = current;
        i++;
    }
    result[count] = NULL;
    return result;
}

/* Split markdown text into chunks each <= max_chars.
   Guarantees: fences not split, tables not split, split at paragraph boundaries,
   small chunks merged. Returns NULL-terminated array, caller frees. */
/* PoP: yuanbao_md_chunk_markdown_text @ gateway/platforms/yuanbao.py:chunk_markdown_text */
char **yuanbao_md_chunk_markdown_text(const char *text, size_t max_chars)
{
    if (!text || !*text) {
        char **arr = calloc(1, sizeof(char*));
        return arr;
    }
    if ((int)strlen(text) <= max_chars) {
        char **arr = calloc(2, sizeof(char*));
        arr[0] = strdup(text);
        return arr;
    }

    /* Phase 1: Extract atomic blocks */
    char **atoms = yuanbao_md_split_into_atoms(text);
    size_t atom_count = 0;
    while (atoms[atom_count]) atom_count++;

    /* Phase 2: Greedy merge */
    char **chunks = calloc(atom_count + 1, sizeof(char*));
    size_t n_chunks = 0;
    bool *indivisible = calloc(atom_count + 1, sizeof(bool));

    char *cur_parts = NULL;
    size_t cur_parts_len = 0, cur_parts_cap = 0;
    int cur_len = 0;

    #define YB_FLUSH_PARTS() do { \
        if (cur_parts_len > 0) { \
            chunks[n_chunks++] = strndup(cur_parts, cur_parts_len); \
            cur_parts_len = 0; \
            cur_len = 0; \
        } \
    } while(0)

    for (size_t a = 0; a < atom_count; a++) {
        const char *atom = atoms[a];
        size_t atom_len_sz = strlen(atom);
        int atom_len = (int)atom_len_sz;
        int sep_len = cur_parts_len > 0 ? 2 : 0;
        int projected = cur_len + sep_len + atom_len;

        if (projected > max_chars && cur_parts_len > 0) {
            YB_FLUSH_PARTS();
            sep_len = 0;
        }

        if (cur_parts_len == 0 && atom_len > max_chars &&
            (yuanbao_md_is_fence_atom(atom) || yuanbao_md_is_table_atom(atom))) {
            indivisible[n_chunks] = true;
            chunks[n_chunks++] = strdup(atom);
            continue;
        }

        if (cur_parts_len + sep_len + atom_len_sz + 1 > cur_parts_cap) {
            cur_parts_cap = (cur_parts_len + sep_len + atom_len_sz + 1) * 2;
            if (cur_parts_cap < 256) cur_parts_cap = 256;
            cur_parts = realloc(cur_parts, cur_parts_cap);
        }
        if (sep_len > 0) {
            cur_parts[cur_parts_len++] = '\n';
            cur_parts[cur_parts_len++] = '\n';
        }
        memcpy(cur_parts + cur_parts_len, atom, atom_len_sz);
        cur_parts_len += atom_len_sz;
        cur_len += sep_len + atom_len;
    }
    YB_FLUSH_PARTS();
    free(cur_parts);

    #undef YB_FLUSH_PARTS

    /* Phase 3: Post-processing — split still-oversized chunks at paragraph boundaries */
    char **result = calloc(n_chunks + 1, sizeof(char*));
    size_t n_result = 0;
    for (size_t idx = 0; idx < n_chunks; idx++) {
        char *chunk = chunks[idx];
        if ((int)strlen(chunk) <= max_chars) {
            result[n_result++] = chunk;
            continue;
        }
        if (indivisible[idx]) {
            result[n_result++] = chunk;
            continue;
        }
        if (yuanbao_md_has_unclosed_fence(chunk)) {
            result[n_result++] = chunk;
            continue;
        }
        char *remaining = chunk;
        while ((int)strlen(remaining) > max_chars) {
            char *tail = NULL;
            char *head = yuanbao_md_split_at_paragraph_boundary(remaining, max_chars, &tail);
            if (head && head[0]) {
                result[n_result++] = head;
            } else {
                free(head);
                /* Force split at max_chars */
                head = strndup(remaining, max_chars);
                result[n_result++] = head;
                if (tail) free(tail);
                tail = strdup(remaining + max_chars);
            }
            if (remaining != chunk) free(remaining);
            remaining = tail;
        }
        if (remaining && remaining[0]) {
            result[n_result++] = remaining;
        } else {
            if (remaining && remaining != chunk) free(remaining);
        }
    }
    result[n_result] = NULL;
    free(chunks);
    free(indivisible);
    /* Free atoms */
    for (size_t a = 0; a < atom_count; a++) free(atoms[a]);
    free(atoms);

    /* Phase 4: Merge small trailing/leading chunks with neighbours */
    if (n_result > 1) {
        char **merged = calloc(n_result + 1, sizeof(char*));
        size_t n_merged = 1;
        merged[0] = result[0];
        for (size_t i = 1; i < n_result; i++) {
            char *prev = merged[n_merged - 1];
            size_t plen = strlen(prev);
            size_t clen = strlen(result[i]);
            size_t combined_len = plen + 2 + clen;
            if ((int)combined_len <= max_chars) {
                char *combined = malloc(combined_len + 1);
                memcpy(combined, prev, plen);
                combined[plen] = '\n';
                combined[plen + 1] = '\n';
                memcpy(combined + plen + 2, result[i], clen);
                combined[combined_len] = '\0';
                free(prev);
                merged[n_merged - 1] = combined;
                free(result[i]);
            } else {
                merged[n_merged++] = result[i];
            }
        }
        merged[n_merged] = NULL;
        free(result);
        result = merged;
    }

    /* Filter NULLs and return */
    return result;
}

/* ================================================================
 *  SignManager — WS auth token signing & caching (port of Python SignManager)
 * ================================================================ */

#include "libhash/hash.h"
#include "libhttp/http.h"
#include "libjson/json.h"

static pthread_mutex_t yb_sign_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct yuanbao_sign_entry {
    char app_key[128];
    yuanbao_token_entry_t entry;
    struct yuanbao_sign_entry *next;
} yuanbao_sign_entry_t;

static yuanbao_sign_entry_t *yb_sign_cache = NULL;  /* linked list of cached entries */

/* Internal: find cache entry by app_key (caller must hold mutex) */
static yuanbao_sign_entry_t *yb_sign_find(const char *app_key) {
    for (yuanbao_sign_entry_t *e = yb_sign_cache; e; e = e->next) {
        if (strcmp(e->app_key, app_key) == 0) return e;
    }
    return NULL;
}

/* Internal: add or update cache entry (caller must hold mutex) */
static void yb_sign_cache_set(const char *app_key, const char *token, const char *bot_id,
                               int duration, const char *product, const char *source,
                               double expire_ts) {
    yuanbao_sign_entry_t *e = yb_sign_find(app_key);
    if (!e) {
        e = calloc(1, sizeof(*e));
        if (!e) return;
        snprintf(e->app_key, sizeof(e->app_key), "%s", app_key);
        e->next = yb_sign_cache;
        yb_sign_cache = e;
    }
    snprintf(e->entry.token, sizeof(e->entry.token), "%s", token ? token : "");
    snprintf(e->entry.bot_id, sizeof(e->entry.bot_id), "%s", bot_id ? bot_id : "");
    e->entry.duration = duration;
    snprintf(e->entry.product, sizeof(e->entry.product), "%s", product ? product : "");
    snprintf(e->entry.source, sizeof(e->entry.source), "%s", source ? source : "");
    e->entry.expire_ts = expire_ts;
}

/* PoP: yuanbao_get_refresh_lock @ gateway/platforms/yuanbao.py:SignManager.get_refresh_lock */
pthread_mutex_t *yuanbao_get_refresh_lock(const char *app_key) {
    (void)app_key;
    return &yb_sign_mutex;
}

/* PoP: yuanbao_compute_signature @ gateway/platforms/yuanbao.py:SignManager.compute_signature */
char *yuanbao_compute_signature(const char *nonce, const char *timestamp,
                                 const char *app_key, const char *app_secret) {
    if (!nonce || !timestamp || !app_key || !app_secret) return NULL;
    char plain[1024];
    snprintf(plain, sizeof(plain), "%s%s%s%s", nonce, timestamp, app_key, app_secret);
    char *out = hash_hmac_sha256_hex((const unsigned char *)app_secret, strlen(app_secret),
                                      (const unsigned char *)plain, strlen(plain));
    return out;
}

/* PoP: yuanbao_build_timestamp @ gateway/platforms/yuanbao.py:SignManager.build_timestamp */
char *yuanbao_build_timestamp(void) {
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    /* Adjust to Beijing time (UTC+8) */
    tm_utc.tm_hour += 8;
    mktime(&tm_utc);  /* normalize */
    char *out = malloc(32);
    if (!out) return NULL;
    strftime(out, 32, "%Y-%m-%dT%H:%M:%S+08:00", &tm_utc);
    return out;
}

/* PoP: yuanbao_is_cache_valid @ gateway/platforms/yuanbao.py:SignManager.is_cache_valid */
bool yuanbao_is_cache_valid(const yuanbao_token_entry_t *entry) {
    if (!entry) return false;
    double now = (double)time(NULL);
    return (entry->expire_ts - now) > YUANBAO_SIGN_CACHE_REFRESH_MARGIN_S;
}

/* PoP: yuanbao_clear_locks @ gateway/platforms/yuanbao.py:SignManager.clear_locks */
void yuanbao_clear_locks(void) {
    pthread_mutex_lock(&yb_sign_mutex);
    /* In C we don't have per-app_key asyncio.Lock objects to clear;
     * we just note that the global mutex is the lock mechanism. */
    pthread_mutex_unlock(&yb_sign_mutex);
}

/* PoP: yuanbao_purge_expired @ gateway/platforms/yuanbao.py:SignManager.purge_expired */
int yuanbao_purge_expired(void) {
    pthread_mutex_lock(&yb_sign_mutex);
    double now = (double)time(NULL);
    int purged = 0;
    yuanbao_sign_entry_t **pp = &yb_sign_cache;
    while (*pp) {
        if (now - (*pp)->entry.expire_ts > 0) {
            yuanbao_sign_entry_t *victim = *pp;
            *pp = victim->next;
            free(victim);
            purged++;
        } else {
            pp = &(*pp)->next;
        }
    }
    pthread_mutex_unlock(&yb_sign_mutex);
    return purged;
}

/* PoP: yuanbao_fetch @ gateway/platforms/yuanbao.py:SignManager.fetch */
yuanbao_sign_entry_t *yuanbao_fetch(const char *app_key, const char *app_secret,
                                     const char *api_domain, const char *route_env) {
    if (!app_key || !app_secret || !api_domain) return NULL;

    char url[512];
    snprintf(url, sizeof(url), "%s%s", api_domain, YUANBAO_SIGN_TOKEN_PATH);

    for (int attempt = 0; attempt <= YUANBAO_SIGN_MAX_RETRIES; attempt++) {
        char *nonce = malloc(33);
        if (!nonce) return NULL;
        for (int i = 0; i < 16; i++) {
            unsigned char b = (unsigned char)rand();
            sprintf(nonce + i * 2, "%02x", b);
        }
        nonce[32] = '\0';

        char *timestamp = yuanbao_build_timestamp();
        if (!timestamp) { free(nonce); return NULL; }

        char *signature = yuanbao_compute_signature(nonce, timestamp, app_key, app_secret);
        if (!signature) { free(nonce); free(timestamp); return NULL; }

        /* Build JSON payload */
        json_t *payload = json_object();
        json_object_set(payload, "app_key", json_string(app_key));
        json_object_set(payload, "nonce", json_string(nonce));
        json_object_set(payload, "signature", json_string(signature));
        json_object_set(payload, "timestamp", json_string(timestamp));

        char *payload_str = json_serialize(payload);
        json_free(payload);

        /* Build headers */
        char *headers = malloc(1024);
        if (!headers) { free(payload_str); free(nonce); free(timestamp); free(signature); return NULL; }
        snprintf(headers, 1024,
                 "Content-Type: application/json\r\n"
                 "X-AppVersion: %s\r\n"
                 "X-OperationSystem: %s\r\n"
                 "X-Instance-Id: %llu\r\n"
                 "X-Bot-Version: %s\r\n",
                 YUANBAO_SIGN_APP_VERSION,
                 YUANBAO_SIGN_OPERATION_SYSTEM,
                 (unsigned long long)YUANBAO_SIGN_INSTANCE_ID,
                 YUANBAO_SIGN_BOT_VERSION);
        if (route_env && route_env[0]) {
            char hdr[128];
            snprintf(hdr, sizeof(hdr), "X-Route-Env: %s\r\n", route_env);
            strncat(headers, hdr, 1023 - strlen(headers));
        }

        /* POST request */
        http_t *client = http_new(30);
        http_resp_t *resp = http_post_json(client, url, payload_str);
        http_free(client);
        free(payload_str);
        free(nonce);
        free(timestamp);
        free(signature);
        free(headers);

        if (!resp) continue;

        if (resp->status != 200) {
            fprintf(stderr, "[gateway:yuanbao] Sign token API returned %d: %.*s\n",
                    resp->status, (int)resp->body_len, resp->body ? resp->body : "");
            http_resp_free(resp);
            if (attempt < YUANBAO_SIGN_MAX_RETRIES) {
                sleep(YUANBAO_SIGN_RETRY_DELAY_S);
                continue;
            }
            return NULL;
        }

        char *err_msg = NULL;
        json_t *result = json_parse(resp->body, &err_msg);
        http_resp_free(resp);
        if (!result) {
            free(err_msg);
            continue;
        }

        json_t *code_node = json_obj_get(result, "code");
        if (!code_node) { json_free(result); continue; }
        int code = (code_node->type == JSON_NUMBER) ? (int)code_node->num_val : 0;

        if (code == 0) {
            json_t *data = json_obj_get(result, "data");
            if (!data || data->type != JSON_OBJECT) {
                json_free(result);
                continue;
            }
            json_t *token_node = json_obj_get(data, "token");
            json_t *bot_id_node = json_obj_get(data, "bot_id");
            json_t *duration_node = json_obj_get(data, "duration");
            json_t *product_node = json_obj_get(data, "product");
            json_t *source_node = json_obj_get(data, "source");

            yuanbao_sign_entry_t *entry = calloc(1, sizeof(*entry));
            if (!entry) { json_free(result); return NULL; }
            snprintf(entry->app_key, sizeof(entry->app_key), "%s", app_key);
            if (token_node && token_node->type == JSON_STRING)
                snprintf(entry->entry.token, sizeof(entry->entry.token), "%s", token_node->str_val);
            if (bot_id_node && bot_id_node->type == JSON_STRING)
                snprintf(entry->entry.bot_id, sizeof(entry->entry.bot_id), "%s", bot_id_node->str_val);
            entry->entry.duration = duration_node ? (duration_node->type == JSON_NUMBER ? (int)duration_node->num_val : 0) : 0;
            if (product_node && product_node->type == JSON_STRING)
                snprintf(entry->entry.product, sizeof(entry->entry.product), "%s", product_node->str_val);
            if (source_node && source_node->type == JSON_STRING)
                snprintf(entry->entry.source, sizeof(entry->entry.source), "%s", source_node->str_val);
            entry->entry.expire_ts = (double)time(NULL) + (entry->entry.duration > 0 ? entry->entry.duration : 3600);
            json_free(result);
            return entry;
        }

        if (code == YUANBAO_SIGN_RETRYABLE_CODE && attempt < YUANBAO_SIGN_MAX_RETRIES) {
            fprintf(stderr, "[gateway:yuanbao] Sign token retryable: code=%d, retrying in %.0fs (attempt=%d/%d)\n",
                    code, YUANBAO_SIGN_RETRY_DELAY_S, attempt + 1, YUANBAO_SIGN_MAX_RETRIES);
            json_free(result);
            sleep(YUANBAO_SIGN_RETRY_DELAY_S);
            continue;
        }

        json_t *msg_node = json_obj_get(result, "msg");
        const char *msg = (msg_node && msg_node->type == JSON_STRING) ? msg_node->str_val : "";
        fprintf(stderr, "[gateway:yuanbao] Sign token error: code=%d, msg=%s\n", code, msg);
        json_free(result);
        return NULL;
    }
    return NULL;
}

/* PoP: yuanbao_get_token @ gateway/platforms/yuanbao.py:SignManager.get_token */
yuanbao_sign_entry_t *yuanbao_get_token(const char *app_key, const char *app_secret,
                                         const char *api_domain, const char *route_env) {
    if (!app_key || !app_secret || !api_domain) return NULL;

    yuanbao_purge_expired();

    pthread_mutex_lock(&yb_sign_mutex);
    yuanbao_sign_entry_t *cached = yb_sign_find(app_key);
    if (cached && yuanbao_is_cache_valid((const yuanbao_token_entry_t *)&cached->entry)) {
        yuanbao_sign_entry_t *copy = malloc(sizeof(*copy));
        if (copy) memcpy(copy, cached, sizeof(*copy));
        pthread_mutex_unlock(&yb_sign_mutex);
        return copy;
    }
    pthread_mutex_unlock(&yb_sign_mutex);

    /* Double-checked locking: fetch and re-check under lock */
    pthread_mutex_lock(&yb_sign_mutex);
    cached = yb_sign_find(app_key);
    if (cached && yuanbao_is_cache_valid((const yuanbao_token_entry_t *)&cached->entry)) {
        yuanbao_sign_entry_t *copy = malloc(sizeof(*copy));
        if (copy) memcpy(copy, cached, sizeof(*copy));
        pthread_mutex_unlock(&yb_sign_mutex);
        return copy;
    }

    yuanbao_sign_entry_t *entry = yuanbao_fetch(app_key, app_secret, api_domain, route_env);
    if (entry) {
        yb_sign_cache_set(app_key, entry->entry.token, entry->entry.bot_id, entry->entry.duration,
                          entry->entry.product, entry->entry.source, entry->entry.expire_ts);
    }
    pthread_mutex_unlock(&yb_sign_mutex);
    return entry;
}

/* PoP: yuanbao_force_refresh @ gateway/platforms/yuanbao.py:SignManager.force_refresh */
yuanbao_sign_entry_t *yuanbao_force_refresh(const char *app_key, const char *app_secret,
                                             const char *api_domain, const char *route_env) {
    if (!app_key || !app_secret || !api_domain) return NULL;

    fprintf(stderr, "[gateway:yuanbao] [force-refresh] Clearing cache and re-signing token: app_key=****%s\n",
            app_key + strlen(app_key) - 4);

    pthread_mutex_lock(&yb_sign_mutex);
    yuanbao_sign_entry_t **pp = &yb_sign_cache;
    while (*pp) {
        if (strcmp((*pp)->app_key, app_key) == 0) {
            yuanbao_sign_entry_t *victim = *pp;
            *pp = victim->next;
            free(victim);
            break;
        }
        pp = &(*pp)->next;
    }
    pthread_mutex_unlock(&yb_sign_mutex);

    return yuanbao_get_token(app_key, app_secret, api_domain, route_env);
}
