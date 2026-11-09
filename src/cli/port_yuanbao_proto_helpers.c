/*
 * port_yuanbao_proto_helpers.c
 *
 * Pure, portable hand-written protobuf wire-format codec ported from
 * gateway/platforms/yuanbao_proto.py. The Python original implements the
 * varint / field / message encoding by hand (no google.protobuf). Every
 * function here is pure binary logic: it reads/writes protobuf bytes and
 * returns malloc'd byte buffers (encode) or JSON strings (decode). No
 * network, no config, no filesystem.
 *
 * Encode functions return malloc'd bytes via an out-length pointer.
 * Decode functions return malloc'd JSON strings (using libjson).
 *
 * Module prefix used by the scanner for gateway/platforms/yuanbao_proto.py is
 * "yuanbao_proto_". (The pre-existing gateway tree port yuanbao_proto.c does
 * NOT define these 15 functions, so this file adds them without collision.)
 *
 * C name <- python name (yuanbao_proto_ prefix):
 *   next_seq_no, encode_conn_msg_full, encode_biz_msg, decode_biz_msg,
 *   decode_inbound_push, decode_forward_msg_data, encode_forward_msg_data,
 *   encode_send_c2c_message, encode_send_group_message, encode_ping,
 *   encode_push_ack, encode_send_private_heartbeat, encode_send_group_heartbeat,
 *   decode_query_group_info_rsp, decode_get_group_member_list_rsp
 */

#include <stdio.h>
#include "yuanbao_proto_decode.h"
#include "yuanbao_proto_encode.h"
#include "yuanbao_proto_wire.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "hermes_json.h"
#include "yuanbao_proto_buf.h"

/* wire types */

/* cmd_type enum */
#define CT_REQUEST 0
#define CT_PUSH    2
#define CT_PUSHACK 3

#define BIZ_PKG "yuanbao_openclaw_proxy"
#define MOD_CONN_ACCESS "conn_access"
#define CMD_PING "ping"

#define SEQ_MAX 0xFFFFFFFFu

/* ---- sequence counter (thread-safe) ---- */
static unsigned int g_seq = 0;
static pthread_mutex_t g_seq_lock = PTHREAD_MUTEX_INITIALIZER;

/* ---------------------------------------------------------------------- */
/* PoP: next_seq_no @ gateway/platforms/yuanbao_proto.py:next_seq_no */
unsigned int yuanbao_proto_next_seq_no(void)
{
    pthread_mutex_lock(&g_seq_lock);
    unsigned int v = g_seq;
    g_seq = (g_seq + 1) & SEQ_MAX;
    pthread_mutex_unlock(&g_seq_lock);
    return v;
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_conn_msg_full @ gateway/platforms/yuanbao_proto.py:encode_conn_msg_full */
unsigned char *yuanbao_proto_encode_conn_msg_full(int cmd_type, const char *cmd, uint64_t seq_no,
    const char *msg_id, const char *module, const unsigned char *data, size_t data_len, int need_ack,
    size_t *out_len)
{
    yb_buf head; yb_init(&head);
    yb_encode_head(&head, cmd_type, cmd, seq_no, msg_id, module, need_ack, 0);
    yb_buf buf; yb_init(&buf);
    yb_f_sub(&buf, 1, &head);
    if (data && data_len) yb_f_len(&buf, 2, data, data_len);
    yb_buf *r = &buf;
    unsigned char *out = malloc(r->len ? r->len : 1);
    memcpy(out, r->p, r->len);
    *out_len = r->len;
    yb_free(&head); yb_free(&buf);
    return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_biz_msg @ gateway/platforms/yuanbao_proto.py:encode_biz_msg */
unsigned char *yuanbao_proto_encode_biz_msg(const char *service, const char *method, const char *req_id,
    const unsigned char *body, size_t body_len, size_t *out_len)
{
    uint64_t seq = yuanbao_proto_next_seq_no();
    return yuanbao_proto_encode_conn_msg_full(CT_REQUEST, method, seq, req_id, service, body, body_len, 0, out_len);
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_send_c2c_message @ gateway/platforms/yuanbao_proto.py:encode_send_c2c_message */
unsigned char *yuanbao_proto_encode_send_c2c_message(const char *to_account, const char *msg_body_json,
    const char *from_account, const char *msg_id, uint64_t msg_random, uint64_t msg_seq,
    const char *group_code, const char *trace_id, size_t *out_len)
{
    size_t biz_len; unsigned char *biz = yb_encode_send_c2c_req(to_account, from_account, msg_body_json,
        msg_id, msg_random, msg_seq, group_code, trace_id, &biz_len);
    char req_id[64]; snprintf(req_id, sizeof(req_id), "c2c_%u", yuanbao_proto_next_seq_no());
    unsigned char *out = yuanbao_proto_encode_conn_msg_full(CT_REQUEST, "send_c2c_message",
        yuanbao_proto_next_seq_no(), req_id, BIZ_PKG, biz, biz_len, 0, out_len);
    free(biz); return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_send_group_message @ gateway/platforms/yuanbao_proto.py:encode_send_group_message */
unsigned char *yuanbao_proto_encode_send_group_message(const char *group_code, const char *msg_body_json,
    const char *from_account, const char *msg_id, const char *to_account, const char *random,
    uint64_t msg_seq, const char *ref_msg_id, const char *trace_id, size_t *out_len)
{
    size_t biz_len; unsigned char *biz = yb_encode_send_group_req(group_code, from_account, msg_body_json,
        msg_id, to_account, random, msg_seq, ref_msg_id, trace_id, &biz_len);
    char req_id[64]; snprintf(req_id, sizeof(req_id), "grp_%u", yuanbao_proto_next_seq_no());
    unsigned char *out = yuanbao_proto_encode_conn_msg_full(CT_REQUEST, "send_group_message",
        yuanbao_proto_next_seq_no(), req_id, BIZ_PKG, biz, biz_len, 0, out_len);
    free(biz); return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_ping @ gateway/platforms/yuanbao_proto.py:encode_ping */
unsigned char *yuanbao_proto_encode_ping(const char *msg_id, size_t *out_len)
{
    return yuanbao_proto_encode_conn_msg_full(CT_REQUEST, CMD_PING, yuanbao_proto_next_seq_no(),
        msg_id, MOD_CONN_ACCESS, NULL, 0, 0, out_len);
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_push_ack @ gateway/platforms/yuanbao_proto.py:encode_push_ack */
/* original_head_json: JSON {"cmd":..,"msg_id":..,"module":..} */
unsigned char *yuanbao_proto_encode_push_ack(const char *original_head_json, size_t *out_len)
{
    char cmd[512]="", msg_id[512]="", module[512]="";
    json_t *h = json_parse(original_head_json, NULL);
    if (h && h->type==JSON_OBJECT) {
        json_t *v;
        if ((v=json_object_get(h,"cmd")) && v->type==JSON_STRING) strncpy(cmd, json_string_value(v), sizeof(cmd)-1);
        if ((v=json_object_get(h,"msg_id")) && v->type==JSON_STRING) strncpy(msg_id, json_string_value(v), sizeof(msg_id)-1);
        if ((v=json_object_get(h,"module")) && v->type==JSON_STRING) strncpy(module, json_string_value(v), sizeof(module)-1);
        json_free(h);
    }
    return yuanbao_proto_encode_conn_msg_full(CT_PUSHACK, cmd, yuanbao_proto_next_seq_no(),
        msg_id, module, NULL, 0, 0, out_len);
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_send_private_heartbeat @ gateway/platforms/yuanbao_proto.py:encode_send_private_heartbeat */
unsigned char *yuanbao_proto_encode_send_private_heartbeat(const char *from_account, const char *to_account,
    uint64_t heartbeat, size_t *out_len)
{
    yb_buf buf; yb_init(&buf);
    yb_f_str(&buf, 1, from_account);
    yb_f_str(&buf, 2, to_account);
    yb_f_varint(&buf, 3, heartbeat);
    char req_id[64]; snprintf(req_id, sizeof(req_id), "hb_priv_%u", yuanbao_proto_next_seq_no());
    unsigned char *out = yuanbao_proto_encode_biz_msg(BIZ_PKG, "send_private_heartbeat", req_id, buf.p, buf.len, out_len);
    yb_free(&buf); return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_send_group_heartbeat @ gateway/platforms/yuanbao_proto.py:encode_send_group_heartbeat */
unsigned char *yuanbao_proto_encode_send_group_heartbeat(const char *from_account, const char *group_code,
    uint64_t heartbeat, uint64_t send_time, size_t *out_len)
{
    yb_buf buf; yb_init(&buf);
    yb_f_str(&buf, 1, from_account);
    yb_f_str(&buf, 2, "");
    yb_f_str(&buf, 3, group_code);
    yb_f_varint(&buf, 4, send_time);
    yb_f_varint(&buf, 5, heartbeat);
    char req_id[64]; snprintf(req_id, sizeof(req_id), "hb_grp_%u", yuanbao_proto_next_seq_no());
    unsigned char *out = yuanbao_proto_encode_biz_msg(BIZ_PKG, "send_group_heartbeat", req_id, buf.p, buf.len, out_len);
    yb_free(&buf); return out;
}

/* ---------------------------------------------------------------------- */
/* PoP: encode_forward_msg_data @ gateway/platforms/yuanbao_proto.py:encode_forward_msg_data */
unsigned char *yuanbao_proto_encode_forward_msg_data(const char *data_json, size_t *out_len)
{
    json_t *d = json_parse(data_json, NULL);
    yb_buf buf; yb_init(&buf);
    if (d && d->type==JSON_OBJECT) {
        json_t *v;
        if ((v=json_object_get(d,"sub_type"))&&v->type==JSON_NUMBER) yb_f_varint(&buf,1,(uint64_t)(int)json_number_value(v));
        yb_fld inb[]={{2,"begin_time"},{3,"end_time"}};
        int ninb=sizeof(inb)/sizeof(inb[0]);
        for (int i=0;i<ninb;i++){ if ((v=json_object_get(d,inb[i].key))&&v->type==JSON_NUMBER&&(int)json_number_value(v)!=0) yb_f_varint(&buf,inb[i].fn,(uint64_t)(int)json_number_value(v)); }
        if ((v=json_object_get(d,"nick_name"))&&v->type==JSON_STRING&&json_string_value(v)[0]) yb_f_str(&buf,4,json_string_value(v));
        json_t *msgs=json_object_get(d,"msg");
        if (msgs&&msgs->type==JSON_ARRAY) for (size_t i=0;i<json_array_size(msgs);i++){ yb_buf mb; yb_init(&mb); yb_encode_forward_msg(&mb, json_array_get(msgs,i)); yb_f_sub(&buf,5,&mb); yb_free(&mb); }
        json_free(d);
    }
    unsigned char *out=malloc(buf.len?buf.len:1); memcpy(out,buf.p,buf.len); *out_len=buf.len; yb_free(&buf); return out;
}

/* ---- InboundMessagePush decode ---- */
/* Forward declaration: defined later (shared MsgContent decoder). */
char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len);

/* PoP: _dbg @ gateway/platforms/yuanbao_proto.py:_dbg */
/* Debug-mode hex dump of the first 64 bytes of a buffer. No-op unless
 * DEBUG_MODE is enabled (mirrors the Python guard `if DEBUG_MODE:`). */
#ifndef YUANBAO_PROTO_DEBUG_MODE
#define YUANBAO_PROTO_DEBUG_MODE 0
#endif

/* PoP: yuanbao_proto_dbg @ gateway/platforms/yuanbao_proto.py:_dbg */
void yuanbao_proto_dbg(const char *label, const unsigned char *data, size_t len)
{
    if (!YUANBAO_PROTO_DEBUG_MODE) return;
    size_t n = len < 64 ? len : 64;
    char hex[64 * 3 + 1];
    char *p = hex;
    for (size_t i = 0; i < n; i++) {
        p += sprintf(p, "%s%02x", (i ? " " : ""), data[i]);
    }
    const char *ellipsis = (len > 64) ? "..." : "";
    fprintf(stderr, "[yuanbao_proto] %s (%zdB): %s%s\n", label ? label : "", len, hex, ellipsis);
}
