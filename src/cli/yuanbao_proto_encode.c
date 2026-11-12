/*
 * yuanbao_proto_encode.c - yuanbao protobuf frame encoders.
 *
 * Split from port_yuanbao_proto_helpers.c (soul.md: self-contained modules).
 * Head / body-element / content encoders plus the C2C, group and forward
 * frame builders. No file-scope state.
 */
#include "yuanbao_proto_encode.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "yuanbao_proto_buf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>



/* ---- parsed fields ---- */

/* ---- Head encode/decode ---- */
/* PoP: _encode_head @ plugins/platforms/yuanbao/yuanbao_proto.py:_encode_head */

/* PoP: _decode_head @ plugins/platforms/yuanbao/yuanbao_proto.py:_decode_head */ void yb_encode_head(yb_buf *b, int cmd_type, const char *cmd, uint64_t seq_no,
                            const char *msg_id, const char *module, int need_ack, uint64_t status)
{
    if (cmd_type != 0) yb_f_varint(b, 1, (uint64_t)cmd_type);
    if (cmd && *cmd) yb_f_str(b, 2, cmd);
    if (seq_no != 0) yb_f_varint(b, 3, seq_no);
    if (msg_id && *msg_id) yb_f_str(b, 4, msg_id);
    if (module && *module) yb_f_str(b, 5, module);
    if (need_ack) yb_f_varint(b, 6, 1);
    if (status != 0) yb_f_varint(b, 10, status & 0xFFFFFFFFULL);
}


/* ---- MsgContent encode/decode ---- */
/* PoP: _encode_msg_content @ plugins/platforms/yuanbao/yuanbao_proto.py:_encode_msg_content */

/* PoP: _decode_map_entry @ gateway/platforms/yuanbao_proto.py:_decode_map_entry */
/* PoP: _encode_map_entry @ gateway/platforms/yuanbao_proto.py:_encode_map_entry */static void yb_encode_msg_content(yb_buf *b, const char *json_content)
{
    /* content is a JSON object; we parse and encode known fields */
    json_t *c = json_parse(json_content, NULL);
    if (!c || c->type != JSON_OBJECT) { if (c) json_free(c); return; }
    /* string fields */
    yb_fld strf[] = {{1,"text"},{2,"uuid"},{4,"data"},{5,"desc"},{6,"ext"},{7,"sound"},{10,"url"},{12,"file_name"}};
    int nstrf = sizeof(strf)/sizeof(strf[0]);
    for (int i=0;i<nstrf;i++) {
        json_t *v = json_object_get(c, strf[i].key);
        if (v && v->type==JSON_STRING && json_string_value(v)[0]) yb_f_str(b, strf[i].fn, json_string_value(v));
    }
    /* varint fields */
    yb_fld intf[] = {{3,"image_format"},{9,"index"},{11,"file_size"}};
    int nintf = sizeof(intf)/sizeof(intf[0]);
    for (int i=0;i<nintf;i++) {
        json_t *v = json_object_get(c, intf[i].key);
        if (v && v->type==JSON_NUMBER && (int)json_number_value(v) != 0) yb_f_varint(b, intf[i].fn, (uint64_t)(int)json_number_value(v));
    }
    /* image_info_array (repeated message, field 8) */
    json_t *imgs = json_object_get(c, "image_info_array");
    if (imgs && imgs->type==JSON_ARRAY) {
        for (size_t i=0;i<json_array_size(imgs);i++) {
            json_t *img = json_array_get(imgs, i);
            if (!img || img->type!=JSON_OBJECT) continue;
            yb_buf ib; yb_init(&ib);
            yb_fld ifs[] = {{1,"type"},{2,"size"},{3,"width"},{4,"height"}};
            int nifs = sizeof(ifs)/sizeof(ifs[0]);
            for (int k=0;k<nifs;k++) { json_t *v=json_object_get(img, ifs[k].key); if (v&&v->type==JSON_NUMBER&&(int)json_number_value(v)!=0) yb_f_varint(&ib, ifs[k].fn, (uint64_t)(int)json_number_value(v)); }
            json_t *url = json_object_get(img, "url"); if (url&&url->type==JSON_STRING&&json_string_value(url)[0]) yb_f_str(&ib,5,json_string_value(url));
            yb_f_sub(b, 8, &ib); yb_free(&ib);
        }
    }
    /* ext_map (field 999) */
    json_t *em = json_object_get(c, "ext_map");
    if (em && em->type==JSON_OBJECT) {
        for (size_t i=0;i<json_object_size(em);i++) {
            const char *k = json_object_get_key_at(em, i);
            json_t *v = json_object_get_at(em, i);
            if (!v || v->type!=JSON_STRING) continue;
            yb_buf eb; yb_init(&eb);
            yb_f_str(&eb, 1, k); yb_f_str(&eb, 2, json_string_value(v));
            yb_f_sub(b, 999, &eb); yb_free(&eb);
        }
    }
    json_free(c);
}


/* ---- MsgBodyElement encode ---- */
/* PoP: _encode_msg_body_element @ plugins/platforms/yuanbao/yuanbao_proto.py:_encode_msg_body_element */
static void yb_encode_msg_body_element(yb_buf *b, const char *json_el)
{
    json_t *e = json_parse(json_el, NULL);
    if (!e || e->type != JSON_OBJECT) { if (e) json_free(e); return; }
    json_t *mt = json_object_get(e, "msg_type");
    if (mt && mt->type==JSON_STRING && json_string_value(mt)[0]) yb_f_str(b, 1, json_string_value(mt));
    json_t *content = json_object_get(e, "msg_content");
    if (content && content->type==JSON_OBJECT) {
        yb_buf cb; yb_init(&cb);
        /* re-dump to pass content JSON to yb_encode_msg_content */
        char *cj = json_dumps(content, 0);
        yb_encode_msg_content(&cb, cj);
        free(cj);
        yb_f_sub(b, 2, &cb); yb_free(&cb);
    }
    json_free(e);
}


/* ---- SendC2CMessageReq / SendGroupMessageReq encode ---- */
/* PoP: _encode_send_c2c_req @ gateway/platforms/yuanbao_proto.py:_encode_send_c2c_req */

/* PoP: _encode_log_ext @ gateway/platforms/yuanbao_proto.py:_encode_log_ext */ unsigned char *yb_encode_send_c2c_req(const char *to_account, const char *from_account,
    const char *msg_body_json, const char *msg_id, uint64_t msg_random, uint64_t msg_seq,
    const char *group_code, const char *trace_id, size_t *out_len)
{
    yb_buf buf; yb_init(&buf);
    if (msg_id && *msg_id) yb_f_str(&buf, 1, msg_id);
    yb_f_str(&buf, 2, to_account);
    if (from_account && *from_account) yb_f_str(&buf, 3, from_account);
    if (msg_random) yb_f_varint(&buf, 4, msg_random);
    json_t *arr = json_parse(msg_body_json, NULL);
    if (arr && arr->type==JSON_ARRAY) {
        for (size_t i=0;i<json_array_size(arr);i++) {
            json_t *el = json_array_get(arr, i);
            if (!el) continue;
            char *ej = json_dumps(el, 0);
            yb_buf eb; yb_init(&eb); yb_encode_msg_body_element(&eb, ej); free(ej);
            yb_f_sub(&buf, 5, &eb); yb_free(&eb);
        }
        json_free(arr);
    }
    if (group_code && *group_code) yb_f_str(&buf, 6, group_code);
    if (msg_seq) yb_f_varint(&buf, 7, msg_seq);
    if (trace_id && *trace_id) { yb_buf lb; yb_init(&lb); yb_f_str(&lb,1,trace_id); yb_f_sub(&buf,8,&lb); yb_free(&lb); }
    unsigned char *out = malloc(buf.len?buf.len:1); memcpy(out, buf.p, buf.len); *out_len = buf.len;
    yb_free(&buf); return out;
}


/* PoP: _encode_send_group_req @ gateway/platforms/yuanbao_proto.py:_encode_send_group_req */ unsigned char *yb_encode_send_group_req(const char *group_code, const char *from_account,
    const char *msg_body_json, const char *msg_id, const char *to_account, const char *random,
    uint64_t msg_seq, const char *ref_msg_id, const char *trace_id, size_t *out_len)
{
    yb_buf buf; yb_init(&buf);
    if (msg_id && *msg_id) yb_f_str(&buf, 1, msg_id);
    yb_f_str(&buf, 2, group_code);
    if (from_account && *from_account) yb_f_str(&buf, 3, from_account);
    if (to_account && *to_account) yb_f_str(&buf, 4, to_account);
    if (random && *random) yb_f_str(&buf, 5, random);
    json_t *arr = json_parse(msg_body_json, NULL);
    if (arr && arr->type==JSON_ARRAY) {
        for (size_t i=0;i<json_array_size(arr);i++) {
            json_t *el = json_array_get(arr, i);
            if (!el) continue;
            char *ej = json_dumps(el, 0);
            yb_buf eb; yb_init(&eb); yb_encode_msg_body_element(&eb, ej); free(ej);
            yb_f_sub(&buf, 6, &eb); yb_free(&eb);
        }
        json_free(arr);
    }
    if (ref_msg_id && *ref_msg_id) yb_f_str(&buf, 7, ref_msg_id);
    if (msg_seq) yb_f_varint(&buf, 8, msg_seq);
    if (trace_id && *trace_id) { yb_buf lb; yb_init(&lb); yb_f_str(&lb,1,trace_id); yb_f_sub(&buf,9,&lb); yb_free(&lb); }
    unsigned char *out = malloc(buf.len?buf.len:1); memcpy(out, buf.p, buf.len); *out_len = buf.len;
    yb_free(&buf); return out;
}


/* ---- ForwardMsgData encode/decode ---- */
/* PoP: _encode_forward_multimedia @ gateway/platforms/yuanbao_proto.py:_encode_forward_multimedia */
static void yb_encode_forward_multimedia(yb_buf *b, json_t *media)
{
    if (!media || media->type!=JSON_OBJECT) return;
    yb_fld s[]={{1,"type"},{2,"url"},{4,"file_name"},{15,"media_id"}};
    int ns=sizeof(s)/sizeof(s[0]);
    for (int i=0;i<ns;i++){ json_t *v=json_object_get(media, s[i].key); if (v&&v->type==JSON_STRING&&json_string_value(v)[0]) yb_f_str(b, s[i].fn, json_string_value(v)); }
    yb_fld in[]={{5,"file_size"},{6,"width"},{7,"height"}};
    int nin=sizeof(in)/sizeof(in[0]);
    for (int i=0;i<nin;i++){ json_t *v=json_object_get(media, in[i].key); if (v&&v->type==JSON_NUMBER&&(int)json_number_value(v)!=0) yb_f_varint(b, in[i].fn, (uint64_t)(int)json_number_value(v)); }
}

/* PoP: _encode_forward_msg_content @ gateway/platforms/yuanbao_proto.py:_encode_forward_msg_content */
static void yb_encode_forward_msg_content(yb_buf *b, json_t *content)
{
    if (!content || content->type!=JSON_OBJECT) return;
    json_t *t = json_object_get(content, "type");
    yb_f_varint(b, 1, (uint64_t)(t&&t->type==JSON_NUMBER?(int)json_number_value(t):0));
    json_t *txt = json_object_get(content, "text");
    if (txt&&txt->type==JSON_STRING&&json_string_value(txt)[0]) yb_f_str(b, 2, json_string_value(txt));
    json_t *mm = json_object_get(content, "multimedia");
    if (mm&&mm->type==JSON_ARRAY) for (size_t i=0;i<json_array_size(mm);i++){ yb_buf mb; yb_init(&mb); yb_encode_forward_multimedia(&mb, json_array_get(mm,i)); yb_f_sub(b,3,&mb); yb_free(&mb); }
}

/* PoP: _encode_forward_msg @ gateway/platforms/yuanbao_proto.py:_encode_forward_msg */ void yb_encode_forward_msg(yb_buf *b, json_t *msg)
{
    if (!msg||msg->type!=JSON_OBJECT) return;
    json_t *s=json_object_get(msg,"sender"); if (s&&s->type==JSON_STRING&&json_string_value(s)[0]) yb_f_str(b,1,json_string_value(s));
    json_t *tm=json_object_get(msg,"time"); if (tm&&tm->type==JSON_NUMBER&&(int)json_number_value(tm)!=0) yb_f_varint(b,2,(uint64_t)(int)json_number_value(tm));
    json_t *p=json_object_get(msg,"plainText"); if (p&&p->type==JSON_STRING&&json_string_value(p)[0]) yb_f_str(b,3,json_string_value(p));
    json_t *mc=json_object_get(msg,"msgContent"); if (mc&&mc->type==JSON_ARRAY) for (size_t i=0;i<json_array_size(mc);i++){ yb_buf cb; yb_init(&cb); yb_encode_forward_msg_content(&cb, json_array_get(mc,i)); yb_f_sub(b,4,&cb); yb_free(&cb); }
}
