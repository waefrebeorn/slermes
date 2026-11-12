/*
 * yuanbao_proto_decode.c - yuanbao protobuf frame decoders.
 *
 * Split from port_yuanbao_proto_helpers.c (soul.md: self-contained modules).
 * Per-frame-type decoders for inbound push, forward messages, group member
 * lists, message content and body elements. No file-scope state.
 */
#include "yuanbao_proto_decode.h"
#include "yuanbao_proto_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "yuanbao_proto_buf.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>



/* ---------------------------------------------------------------------- */
/* PoP: decode_biz_msg @ plugins/platforms/yuanbao/yuanbao_proto.py:decode_biz_msg */
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

/* PoP: _decode_forward_msg_content @ plugins/platforms/yuanbao/yuanbao_proto.py:_decode_forward_msg_content */
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

/* PoP: _decode_forward_msg @ plugins/platforms/yuanbao/yuanbao_proto.py:_decode_forward_msg */
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
/* PoP: decode_forward_msg_data @ plugins/platforms/yuanbao/yuanbao_proto.py:decode_forward_msg_data */
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

/* ---------------------------------------------------------------------- */
/* PoP: decode_inbound_push @ plugins/platforms/yuanbao/yuanbao_proto.py:decode_inbound_push */
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
/* PoP: decode_query_group_info_rsp @ plugins/platforms/yuanbao/yuanbao_proto.py:decode_query_group_info_rsp */
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
/* PoP: decode_get_group_member_list_rsp @ plugins/platforms/yuanbao/yuanbao_proto.py:decode_get_group_member_list_rsp */
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

/* PoP: _decode_msg_content @ plugins/platforms/yuanbao/yuanbao_proto.py:_decode_msg_content */
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


/* PoP: _decode_msg_body_element @ plugins/platforms/yuanbao/yuanbao_proto.py:_decode_msg_body_element */
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
