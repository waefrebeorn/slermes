/*
 * yuanbao.c — Gateway platform adapter.
 * Port of Python gateway/platforms/yuanbao.py.
 */

#include "hermes_agent.h"
#include "yuanbao_md.h"
#include "yuanbao_proto.h"
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

/* ConnMsg fields */

/* CMD_TYPE values */
#define CT_RESPONSE 1
#define CT_PUSH     2
#define CT_PUSH_ACK 3

/* CMD strings */
#define CMD_AUTH_BIND  "auth-bind"
#define CMD_PING       "ping"

/* Biz service */

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

/* PoP: yuanbao_start @ gateway/platforms/yuanbao.py:start */
/* PoP: yuanbao_start @ gateway/platforms/yuanbao.py:connect */
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

/* PoP: yuanbao_stop @ gateway/platforms/yuanbao.py:stop */
/* PoP: yuanbao_stop @ gateway/platforms/yuanbao.py:close */
void yuanbao_stop(void) {
    g_yb.running = false;
}

bool yuanbao_is_running(void) {
    return g_yb.running;
}

/* Send a sticker via the active WebSocket connection. Returns 0 on success, -1 on error. */
/* PoP: send_sticker @ gateway/platforms/yuanbao.py:send_sticker */
/* Port of Python gateway/platforms/yuanbao.py:send_sticker(). */
/* PoP: send_sticker @ gateway/platforms/yuanbao.py:send */
int send_sticker(const char *to_uid, const char *sticker_id,
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
/* PoP: yuanbao_query_group_info @ gateway/platforms/yuanbao.py:query_group_info */
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
/* PoP: yuanbao_get_group_member_list @ gateway/platforms/yuanbao.py:get_group_member_list */
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
/* PoP: yuanbao_send_dm @ gateway/platforms/yuanbao.py:send_dm */
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

/* PoP: is_cache_valid @ gateway/platforms/yuanbao.py:SignManager.is_cache_valid */
bool is_cache_valid(const yuanbao_token_entry_t *entry) {
    if (!entry) return false;
    double now = (double)time(NULL);
    return (entry->expire_ts - now) > YUANBAO_SIGN_CACHE_REFRESH_MARGIN_S;
}

/* PoP: clear_locks @ gateway/platforms/yuanbao.py:SignManager.clear_locks */
void clear_locks(void) {
    pthread_mutex_lock(&yb_sign_mutex);
    /* In C we don't have per-app_key asyncio.Lock objects to clear;
     * we just note that the global mutex is the lock mechanism. */
    pthread_mutex_unlock(&yb_sign_mutex);
}

/* PoP: purge_expired @ gateway/platforms/yuanbao.py:SignManager.purge_expired */
int purge_expired(void) {
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

    purge_expired();

    pthread_mutex_lock(&yb_sign_mutex);
    yuanbao_sign_entry_t *cached = yb_sign_find(app_key);
    if (cached && is_cache_valid((const yuanbao_token_entry_t *)&cached->entry)) {
        yuanbao_sign_entry_t *copy = malloc(sizeof(*copy));
        if (copy) memcpy(copy, cached, sizeof(*copy));
        pthread_mutex_unlock(&yb_sign_mutex);
        return copy;
    }
    pthread_mutex_unlock(&yb_sign_mutex);

    /* Double-checked locking: fetch and re-check under lock */
    pthread_mutex_lock(&yb_sign_mutex);
    cached = yb_sign_find(app_key);
    if (cached && is_cache_valid((const yuanbao_token_entry_t *)&cached->entry)) {
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
