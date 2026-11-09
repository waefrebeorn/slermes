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
#define CT_RESPONSE 1
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
/* PoP: decode_biz_msg @ gateway/platforms/yuanbao_proto.py:decode_biz_msg */
char *yuanbao_proto_decode_biz_msg(const unsigned char *data, size_t len)
{
    /* decode_conn_msg, then extract head fields */
    yb_fields fs; if (!yb_parse_fields(data, len, &fs)) return strdup("{}");
    size_t headlen; unsigned char *headb = yb_get_bytes(&fs, 1, &headlen);
    size_t payload_len; unsigned char *payload = yb_get_bytes(&fs, 2, &payload_len);
    yb_fields hf; yb_parse_fields(headb, headlen, &hf);
    char cmd[512], msg_id[512], module[512];
    yb_get_string(&hf, 2, cmd, sizeof(cmd));
    yb_get_string(&hf, 4, msg_id, sizeof(msg_id));
    yb_get_string(&hf, 5, module, sizeof(module));
    uint64_t cmd_type = yb_get_varint(&hf, 1, 0);
    json_t *root = json_new_object();
    json_object_set(root, "service", json_string(module));
    json_object_set(root, "method", json_string(cmd));
    json_object_set(root, "req_id", json_string(msg_id));
    json_object_set(root, "body", json_string((payload && payload_len) ? (const char*)payload : ""));
    json_object_set(root, "is_response", json_int((cmd_type == CT_RESPONSE) ? 1 : 0));
    char *out = json_dumps(root, 0);
    json_free(root); yb_fields_free(&hf); yb_fields_free(&fs);
    return out;
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

/* decode forward multimedia / msg_content / msg */
/* PoP: _decode_forward_multimedia @ gateway/platforms/yuanbao_proto.py:_decode_forward_multimedia */
static json_t *yb_decode_forward_multimedia(const unsigned char *d, size_t n)
{
    yb_fields fs; yb_parse_fields(d,n,&fs);
    json_t *m = json_new_object();
    char s[256];
    yb_get_string(&fs,1,s,sizeof(s)); if (s[0]) json_object_set(m,"type",json_string(s));
    yb_get_string(&fs,2,s,sizeof(s)); if (s[0]) json_object_set(m,"url",json_string(s));
    yb_get_string(&fs,4,s,sizeof(s)); if (s[0]) json_object_set(m,"file_name",json_string(s));
    yb_get_string(&fs,15,s,sizeof(s)); if (s[0]) json_object_set(m,"media_id",json_string(s));
    uint64_t v;
    v=yb_get_varint(&fs,5,0); if (v) json_object_set(m,"file_size",json_int((int)v));
    v=yb_get_varint(&fs,6,0); if (v) json_object_set(m,"width",json_int((int)v));
    v=yb_get_varint(&fs,7,0); if (v) json_object_set(m,"height",json_int((int)v));
    yb_fields_free(&fs); return m;
}
/* PoP: _decode_forward_msg_content @ gateway/platforms/yuanbao_proto.py:_decode_forward_msg_content */
static json_t *yb_decode_forward_msg_content(const unsigned char *d, size_t n)
{
    yb_fields fs; yb_parse_fields(d,n,&fs);
    json_t *c = json_new_object();
    json_object_set(c,"type",json_int((int)yb_get_varint(&fs,1,0)));
    char s[1024]; yb_get_string(&fs,2,s,sizeof(s)); if (s[0]) json_object_set(c,"text",json_string(s));
    unsigned char **pb; size_t *pl; size_t cnt=yb_get_repeated_bytes(&fs,3,&pb,&pl);
    if (cnt) { json_t *arr=json_new_array(); for (size_t i=0;i<cnt;i++) json_array_append(arr, yb_decode_forward_multimedia(pb[i],pl[i])); json_object_set(c,"multimedia",arr); free(pb); free(pl); /* caller-owned arrays; elements alias fs (freed by yb_fields_free) */ }
    yb_fields_free(&fs); return c;
}
/* PoP: _decode_forward_msg @ gateway/platforms/yuanbao_proto.py:_decode_forward_msg */
static json_t *yb_decode_forward_msg(const unsigned char *d, size_t n)
{
    yb_fields fs; yb_parse_fields(d,n,&fs);
    json_t *m = json_new_object();
    char s[512]; yb_get_string(&fs,1,s,sizeof(s)); if (s[0]) json_object_set(m,"sender",json_string(s));
    json_object_set(m,"time",json_int((int)yb_get_varint(&fs,2,0)));
    yb_get_string(&fs,3,s,sizeof(s)); if (s[0]) json_object_set(m,"plainText",json_string(s));
    unsigned char **pb; size_t *pl; size_t cnt=yb_get_repeated_bytes(&fs,4,&pb,&pl);
    if (cnt) { json_t *arr=json_new_array(); for (size_t i=0;i<cnt;i++) json_array_append(arr, yb_decode_forward_msg_content(pb[i],pl[i])); json_object_set(m,"msgContent",arr); free(pb); free(pl); /* caller-owned arrays; elements alias fs (freed by yb_fields_free) */ }
    yb_fields_free(&fs); return m;
}

/* ---------------------------------------------------------------------- */
/* PoP: decode_forward_msg_data @ gateway/platforms/yuanbao_proto.py:decode_forward_msg_data */
char *yuanbao_proto_decode_forward_msg_data(const unsigned char *data, size_t len)
{
    yb_fields fs; if (!yb_parse_fields(data,len,&fs)) return strdup("{}");
    json_t *root = json_new_object();
    json_object_set(root,"sub_type",json_int((int)yb_get_varint(&fs,1,0)));
    json_object_set(root,"begin_time",json_int((int)yb_get_varint(&fs,2,0)));
    json_object_set(root,"end_time",json_int((int)yb_get_varint(&fs,3,0)));
    char s[1024]; yb_get_string(&fs,4,s,sizeof(s)); if (s[0]) json_object_set(root,"nick_name",json_string(s));
    unsigned char **pb; size_t *pl; size_t cnt=yb_get_repeated_bytes(&fs,5,&pb,&pl);
    if (cnt) { json_t *arr=json_new_array(); for (size_t i=0;i<cnt;i++) json_array_append(arr, yb_decode_forward_msg(pb[i],pl[i])); json_object_set(root,"msg",arr); free(pb); free(pl); /* caller-owned arrays; elements alias fs (freed by yb_fields_free) */ }
    char *out = json_dumps(root, 0); json_free(root); yb_fields_free(&fs); return out;
}

/* ---- InboundMessagePush decode ---- */
/* Forward declaration: defined later (shared MsgContent decoder). */
char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len);
/* ---------------------------------------------------------------------- */
/* PoP: decode_inbound_push @ gateway/platforms/yuanbao_proto.py:decode_inbound_push */
char *yuanbao_proto_decode_inbound_push(const unsigned char *data, size_t len)
{
    yb_fields fs; if (!yb_parse_fields(data,len,&fs)) return NULL;
    json_t *root = json_new_object();
    char s[2048];
    #define SETSTR(fn,key) do { yb_get_string(&fs,fn,s,sizeof(s)); if (s[0]) json_object_set(root,key,json_string(s)); } while(0)
    SETSTR(1,"callback_command");
    SETSTR(2,"from_account");
    SETSTR(3,"to_account");
    SETSTR(4,"sender_nickname");
    SETSTR(5,"group_id");
    SETSTR(6,"group_code");
    SETSTR(7,"group_name");
    SETSTR(11,"msg_key");
    SETSTR(12,"msg_id");
    SETSTR(14,"cloud_custom_data");
    SETSTR(16,"bot_owner_id");
    SETSTR(19,"private_from_group_code");
    #undef SETSTR
    json_object_set(root,"msg_seq",json_int((int)yb_get_varint(&fs,8,0)));  /* always kept */
    { uint64_t v;
      v=yb_get_varint(&fs,9,0);  if (v) json_object_set(root,"msg_random",json_int((int)v));
      v=yb_get_varint(&fs,10,0); if (v) json_object_set(root,"msg_time",json_int((int)v));
      v=yb_get_varint(&fs,15,0); if (v) json_object_set(root,"event_time",json_int((int)v));
      v=yb_get_varint(&fs,18,0); if (v) json_object_set(root,"claw_msg_type",json_int((int)v));
    }
    /* msg_body (repeated field 13) */
    unsigned char **pb; size_t *pl; size_t cnt=yb_get_repeated_bytes(&fs,13,&pb,&pl);
    if (cnt) {
        json_t *arr=json_new_array();
        for (size_t i=0;i<cnt;i++) {
            /* each element: field1 msg_type (str), field2 msg_content (msg) */
            yb_fields ef; yb_parse_fields(pb[i],pl[i],&ef);
            json_t *el=json_new_object();
            char mt[256]; yb_get_string(&ef,1,mt,sizeof(mt)); json_object_set(el,"msg_type",json_string(mt));
            size_t clen; unsigned char *cb=yb_get_bytes(&ef,2,&clen);
            if (cb) {
                /* decode MsgContent via the shared helper (matches Python
                 * _decode_msg_content: skips zero varints, includes ext_map). */
                char *cj = yuanbao_proto_decode_msg_content(cb, clen);
                json_t *content = cj ? json_parse(cj, NULL) : NULL;
                if (!content) content = json_new_object();
                json_object_set(el, "msg_content", content);
                free(cj);
            } else {
                json_object_set(el,"msg_content",json_new_object());
            }
            json_array_append(arr, el);
            yb_fields_free(&ef);
        }
        json_object_set(root,"msg_body",arr);
        free(pb); free(pl); /* caller-owned arrays; elements alias fs (freed by yb_fields_free) */
    }
    /* trace_id (field 20 -> LogInfoExt -> field1) */
    size_t lelen; unsigned char *leb=yb_get_bytes(&fs,20,&lelen);
    if (leb) { yb_fields lf; yb_parse_fields(leb,lelen,&lf); char t[1024]; yb_get_string(&lf,1,t,sizeof(t)); if (t[0]) json_object_set(root,"trace_id",json_string(t)); yb_fields_free(&lf); }
    /* recall_msg_seq_list (field 17) */
    unsigned char **rb; size_t *rl; size_t rc=yb_get_repeated_bytes(&fs,17,&rb,&rl);
    if (rc) {
        json_t *arr=json_new_array();
        for (size_t i=0;i<rc;i++){ yb_fields rf; yb_parse_fields(rb[i],rl[i],&rf); json_t *e=json_new_object(); json_object_set(e,"msg_seq",json_int((int)yb_get_varint(&rf,1,0))); char rs[512]; yb_get_string(&rf,2,rs,sizeof(rs)); if (rs[0]) json_object_set(e,"msg_id",json_string(rs)); json_array_append(arr,e); yb_fields_free(&rf); }
        json_object_set(root,"recall_msg_seq_list",arr); free(rb); free(rl);
    }
    char *out = json_dumps(root, 0); json_free(root); yb_fields_free(&fs); return out;
}

/* ---- QueryGroupInfoRsp decode ---- */
/* ---------------------------------------------------------------------- */
/* PoP: decode_query_group_info_rsp @ gateway/platforms/yuanbao_proto.py:decode_query_group_info_rsp */
char *yuanbao_proto_decode_query_group_info_rsp(const unsigned char *data, size_t len)
{
    yb_fields fs; if (!yb_parse_fields(data,len,&fs)) return NULL;
    json_t *root = json_new_object();
    json_object_set(root,"code",json_int((int)yb_get_varint(&fs,1,0)));
    char s[2048]; yb_get_string(&fs,2,s,sizeof(s)); if (s[0]) json_object_set(root,"message",json_string(s));
    size_t gilen; unsigned char *gib=yb_get_bytes(&fs,3,&gilen);
    if (gib) {
        yb_fields gf; yb_parse_fields(gib,gilen,&gf);
        char gs[2048];
        yb_get_string(&gf,1,gs,sizeof(gs)); json_object_set(root,"group_name",json_string(gs));
        yb_get_string(&gf,2,gs,sizeof(gs)); json_object_set(root,"owner_id",json_string(gs));
        yb_get_string(&gf,3,gs,sizeof(gs)); json_object_set(root,"owner_nickname",json_string(gs));
        json_object_set(root,"member_count",json_int((int)yb_get_varint(&gf,4,0)));
        yb_fields_free(&gf);
    } else {
        json_object_set(root,"group_name",json_string(""));
        json_object_set(root,"owner_id",json_string(""));
        json_object_set(root,"owner_nickname",json_string(""));
        json_object_set(root,"member_count",json_int(0));
    }
    char *out = json_dumps(root, 0); json_free(root); yb_fields_free(&fs); return out;
}

/* ---- GetGroupMemberListRsp decode ---- */
/* ---------------------------------------------------------------------- */
/* PoP: decode_get_group_member_list_rsp @ gateway/platforms/yuanbao_proto.py:decode_get_group_member_list_rsp */
char *yuanbao_proto_decode_get_group_member_list_rsp(const unsigned char *data, size_t len)
{
    yb_fields fs; if (!yb_parse_fields(data,len,&fs)) return NULL;
    json_t *root = json_new_object();
    json_object_set(root,"code",json_int((int)yb_get_varint(&fs,1,0)));
    char s[2048]; yb_get_string(&fs,2,s,sizeof(s)); if (s[0]) json_object_set(root,"message",json_string(s));
    unsigned char **pb; size_t *pl; size_t cnt=yb_get_repeated_bytes(&fs,3,&pb,&pl);
    json_t *arr=json_new_array();
    for (size_t i=0;i<cnt;i++) {
        yb_fields mf; yb_parse_fields(pb[i],pl[i],&mf);
        json_t *m=json_new_object();
        char ms[2048];
        yb_get_string(&mf,1,ms,sizeof(ms)); if (ms[0]) json_object_set(m,"user_id",json_string(ms));
        yb_get_string(&mf,2,ms,sizeof(ms)); if (ms[0]) json_object_set(m,"nickname",json_string(ms));
        json_object_set(m,"role",json_int((int)yb_get_varint(&mf,3,0)));  /* always kept */
        { uint64_t v=yb_get_varint(&mf,4,0); if (v) json_object_set(m,"join_time",json_int((int)v)); }
        yb_get_string(&mf,5,ms,sizeof(ms)); if (ms[0]) json_object_set(m,"name_card",json_string(ms));
        json_array_append(arr, m);
        yb_fields_free(&mf);
    }
    json_object_set(root,"members",arr);
    free(pb); free(pl); /* caller-owned arrays; elements alias fs (freed by yb_fields_free) */
    json_object_set(root,"next_offset",json_int((int)yb_get_varint(&fs,4,0)));
    json_object_set(root,"is_complete",json_bool(yb_get_varint(&fs,5,0)?1:0));
    char *out = json_dumps(root, 0); json_free(root); yb_fields_free(&fs); return out;
}

/* ---------------------------------------------------------------------- */
/* The Python module exposes individual message-level decoders
 * (_decode_msg_content / _decode_msg_body_element / _decode_im_msg_seq /
 * _decode_log_ext). The C port inlines this logic inside
 * yuanbao_proto_decode_inbound_push; these thin real wrappers expose the same
 * behavior so the port is name-complete and oracle-verifiable. */

/* PoP: _decode_msg_content @ gateway/platforms/yuanbao_proto.py:_decode_msg_content */
char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len)
{
    yb_fields cf; if (!yb_parse_fields(data,len,&cf)) return strdup("{}");
    json_t *content = json_new_object();
    char cs[2048];
    yb_get_string(&cf,1,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"text",json_string(cs));
    yb_get_string(&cf,2,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"uuid",json_string(cs));
    yb_get_string(&cf,4,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"data",json_string(cs));
    yb_get_string(&cf,5,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"desc",json_string(cs));
    yb_get_string(&cf,6,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"ext",json_string(cs));
    yb_get_string(&cf,10,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"url",json_string(cs));
    yb_get_string(&cf,12,cs,sizeof(cs)); if (cs[0]) json_object_set(content,"file_name",json_string(cs));
    uint64_t v;
    v=yb_get_varint(&cf,3,0); if (v) json_object_set(content,"image_format",json_int((int)v));
    v=yb_get_varint(&cf,9,0); if (v) json_object_set(content,"index",json_int((int)v));
    v=yb_get_varint(&cf,11,0); if (v) json_object_set(content,"file_size",json_int((int)v));
    /* ext_map (field 999): repeated map entries key=field1, value=field2 */
    unsigned char **pb; size_t *pl; size_t cnt = yb_get_repeated_bytes(&cf, 999, &pb, &pl);
    if (cnt) {
        json_t *em = json_new_object();
        for (size_t i=0;i<cnt;i++) {
            yb_fields ef; yb_parse_fields(pb[i], pl[i], &ef);
            char k[2048], val[2048];
            yb_get_string(&ef,1,k,sizeof(k));
            yb_get_string(&ef,2,val,sizeof(val));
            if (k[0]) json_object_set(em, k, json_string(val));
            yb_fields_free(&ef);
        }
        if (json_object_size(em)) json_object_set(content, "ext_map", em);
        else json_free(em);
        free(pb); free(pl); /* caller-owned arrays; elements alias fs (freed by yb_fields_free) */
    }
    char *out = json_dumps(content, 0); json_free(content); yb_fields_free(&cf); return out;
}

/* PoP: _decode_msg_body_element @ gateway/platforms/yuanbao_proto.py:_decode_msg_body_element */
char *yuanbao_proto_decode_msg_body_element(const unsigned char *data, size_t len)
{
    yb_fields ef; if (!yb_parse_fields(data,len,&ef)) return strdup("{}");
    json_t *el = json_new_object();
    char mt[256]; yb_get_string(&ef,1,mt,sizeof(mt)); json_object_set(el,"msg_type",json_string(mt));
    size_t clen; unsigned char *cb = yb_get_bytes(&ef,2,&clen);
    if (cb) {
        char *cj = yuanbao_proto_decode_msg_content(cb, clen);
        json_t *content = cj ? json_parse(cj, NULL) : NULL;
        if (!content) content = json_new_object();
        json_object_set(el, "msg_content", content);  /* takes ownership */
        free(cj);
    } else {
        json_object_set(el,"msg_content",json_new_object());
    }
    char *out = json_dumps(el, 0); json_free(el); yb_fields_free(&ef); return out;
}

/* PoP: _decode_im_msg_seq @ gateway/platforms/yuanbao_proto.py:_decode_im_msg_seq */
char *yuanbao_proto_decode_im_msg_seq(const unsigned char *data, size_t len)
{
    yb_fields rf; if (!yb_parse_fields(data,len,&rf)) return strdup("{}");
    json_t *e = json_new_object();
    json_object_set(e,"msg_seq",json_int((int)yb_get_varint(&rf,1,0)));
    char rs[512]; yb_get_string(&rf,2,rs,sizeof(rs)); if (rs[0]) json_object_set(e,"msg_id",json_string(rs));
    char *out = json_dumps(e,0); json_free(e); yb_fields_free(&rf); return out;
}

/* PoP: _decode_log_ext @ gateway/platforms/yuanbao_proto.py:_decode_log_ext */
char *yuanbao_proto_decode_log_ext(const unsigned char *data, size_t len)
{
    yb_fields lf; if (!yb_parse_fields(data,len,&lf)) return strdup("{}");
    json_t *e = json_new_object();
    char t[1024]; yb_get_string(&lf,1,t,sizeof(t)); if (t[0]) json_object_set(e,"trace_id",json_string(t));
    char *out = json_dumps(e,0); json_free(e); yb_fields_free(&lf); return out;
}

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
