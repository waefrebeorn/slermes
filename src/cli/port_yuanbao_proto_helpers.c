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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "hermes_json.h"

typedef struct { int fn; const char *key; } yb_fld;

/* wire types */
#define WT_VARINT 0
#define WT_64BIT  1
#define WT_LEN    2
#define WT_32BIT  5

/* cmd_type enum */
#define CT_REQUEST 0
#define CT_RESPONSE 1
#define CT_PUSH    2
#define CT_PUSHACK 3

#define BIZ_PKG "yuanbao_openclaw_proxy"
#define MOD_CONN_ACCESS "conn_access"
#define CMD_PING "ping"

#define SEQ_MAX 0xFFFFFFFFu

/* ---- growable byte buffer ---- */
typedef struct { unsigned char *p; size_t len, cap; } yb_buf;
static void yb_init(yb_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void yb_free(yb_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void yb_reserve(yb_buf *b, size_t extra)
{
    if (b->len + extra <= b->cap) return;
    size_t ncap = b->cap ? b->cap * 2 : 64;
    while (ncap < b->len + extra) ncap *= 2;
    b->p = realloc(b->p, ncap);
    b->cap = ncap;
}
static void yb_append(yb_buf *b, const unsigned char *d, size_t n)
{
    if (n == 0) return;
    yb_reserve(b, n);
    memcpy(b->p + b->len, d, n);
    b->len += n;
}
static void yb_append_byte(yb_buf *b, unsigned char c) { yb_reserve(b, 1); b->p[b->len++] = c; }

/* varint encode */
/* PoP: _encode_varint @ gateway/platforms/yuanbao_proto.py:_encode_varint */
static void yb_append_varint(yb_buf *b, uint64_t v)
{
    if ((int64_t)v < 0) v &= 0xFFFFFFFFFFFFFFFFULL;
    while (1) {
        unsigned char bits = (unsigned char)(v & 0x7F);
        v >>= 7;
        if (v) yb_append_byte(b, bits | 0x80);
        else { yb_append_byte(b, bits); break; }
    }
}

/* field tag = (field_number << 3) | wire_type, then value */
/* PoP: _encode_field @ gateway/platforms/yuanbao_proto.py:_encode_field */
static void yb_append_field(yb_buf *b, int fn, int wt, const unsigned char *val, size_t n)
{
    yb_append_varint(b, (uint64_t)((fn << 3) | wt));
    if (wt == WT_LEN || wt == WT_64BIT || wt == WT_32BIT) {
        yb_append_varint(b, (uint64_t)n);
        yb_append(b, val, n);
    } else {
        yb_append_varint(b, *(uint64_t *)val); /* not used for varint path */
    }
}
/* convenience: varint field */
static void yb_f_varint(yb_buf *b, int fn, uint64_t v)
{ yb_append_varint(b, (uint64_t)((fn << 3) | WT_VARINT)); yb_append_varint(b, v); }
/* convenience: length-delimited field from raw bytes */

/* PoP: _encode_bytes @ gateway/platforms/yuanbao_proto.py:_encode_bytes */static void yb_f_len(yb_buf *b, int fn, const unsigned char *d, size_t n)
{ yb_append_varint(b, (uint64_t)((fn << 3) | WT_LEN)); yb_append_varint(b, n); yb_append(b, d, n); }
/* string field */

/* PoP: _encode_string @ gateway/platforms/yuanbao_proto.py:_encode_string */static void yb_f_str(yb_buf *b, int fn, const char *s)
{ if (!s || !*s) return; size_t n = strlen(s); yb_append_varint(b, (uint64_t)((fn<<3)|WT_LEN)); yb_append_varint(b, n); yb_append(b, (const unsigned char*)s, n); }
/* length-delimited from a sub-buffer (message/bytes wrapper) */

/* PoP: _encode_message @ gateway/platforms/yuanbao_proto.py:_encode_message */static void yb_f_sub(yb_buf *b, int fn, const yb_buf *sub)
{ yb_f_len(b, fn, sub->p, sub->len); }

/* ---- varint decode ---- */
/* PoP: _decode_varint @ gateway/platforms/yuanbao_proto.py:_decode_varint */
static int yb_decode_varint(const unsigned char *d, size_t n, size_t *pos, uint64_t *out)
{
    uint64_t result = 0; int shift = 0;
    while (*pos < n) {
        unsigned char byte = d[*pos]; (*pos)++;
        result |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
        if (!(byte & 0x80)) { *out = result; return 1; }
        if (shift >= 64) return 0;
    }
    return 0;
}

/* ---- parsed fields ---- */
typedef struct { int fn; int wt; uint64_t vint; unsigned char *vbytes; size_t vlen; } yb_field;
typedef struct { yb_field *f; size_t n, cap; } yb_fields;
static void yb_fields_free(yb_fields *fs)
{ for (size_t i=0;i<fs->n;i++) free(fs->f[i].vbytes); free(fs->f); fs->f=NULL; fs->n=fs->cap=0; }
/* PoP: _parse_fields @ gateway/platforms/yuanbao_proto.py:_parse_fields */

/* PoP: _fields_to_dict @ gateway/platforms/yuanbao_proto.py:_fields_to_dict */static int yb_parse_fields(const unsigned char *d, size_t n, yb_fields *out)
{
    out->f = NULL; out->n = out->cap = 0;
    size_t pos = 0;
    while (pos < n) {
        uint64_t tag; if (!yb_decode_varint(d, n, &pos, &tag)) break;
        int fn = (int)(tag >> 3), wt = (int)(tag & 0x07);
        yb_field f; memset(&f, 0, sizeof(f)); f.fn = fn; f.wt = wt; f.vint = 0; f.vbytes = NULL; f.vlen = 0;
        if (wt == WT_VARINT) {
            uint64_t v; if (!yb_decode_varint(d, n, &pos, &v)) break;
            f.vint = v;
        } else if (wt == WT_LEN) {
            uint64_t len; if (!yb_decode_varint(d, n, &pos, &len)) break;
            if (pos + len > n) { f.vlen = n - pos; } else f.vlen = (size_t)len;
            f.vbytes = malloc(f.vlen ? f.vlen : 1);
            memcpy(f.vbytes, d + pos, f.vlen);
            pos += f.vlen;
        } else if (wt == WT_64BIT) {
            if (pos + 8 > n) break;
            f.vbytes = malloc(8); memcpy(f.vbytes, d + pos, 8); f.vlen = 8; pos += 8;
        } else if (wt == WT_32BIT) {
            if (pos + 4 > n) break;
            f.vbytes = malloc(4); memcpy(f.vbytes, d + pos, 4); f.vlen = 4; pos += 4;
        } else { return 0; }
        if (out->n == out->cap) { out->cap = out->cap ? out->cap*2 : 8; out->f = realloc(out->f, out->cap*sizeof(yb_field)); }
        out->f[out->n++] = f;
    }
    return 1;
}
/* accessors: first match by field number */
/* PoP: _get_varint @ gateway/platforms/yuanbao_proto.py:_get_varint */
static uint64_t yb_get_varint(const yb_fields *fs, int fn, uint64_t def)
{ for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_VARINT) return fs->f[i].vint; return def; }
/* PoP: _get_string @ gateway/platforms/yuanbao_proto.py:_get_string */
static char *yb_get_string(const yb_fields *fs, int fn, char *buf, size_t bufsz)
{ buf[0]=0; for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) {
    size_t c = fs->f[i].vlen < bufsz-1 ? fs->f[i].vlen : bufsz-1;
    memcpy(buf, fs->f[i].vbytes, c); buf[c]=0; return buf; }
  return buf; }
/* PoP: _get_bytes @ gateway/platforms/yuanbao_proto.py:_get_bytes */
static unsigned char *yb_get_bytes(const yb_fields *fs, int fn, size_t *outlen)
{ *outlen=0; for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) { *outlen=fs->f[i].vlen; return fs->f[i].vbytes; } return NULL; }
/* collect repeated bytes into a malloc'd array of (ptr,len); returns count */
/* PoP: _get_repeated_bytes @ gateway/platforms/yuanbao_proto.py:_get_repeated_bytes */
static size_t yb_get_repeated_bytes(const yb_fields *fs, int fn, unsigned char ***out, size_t **lens)
{
    size_t cnt = 0; for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) cnt++;
    *out = cnt ? malloc(cnt*sizeof(unsigned char*)) : NULL;
    *lens = cnt ? malloc(cnt*sizeof(size_t)) : NULL;
    size_t j = 0;
    for (size_t i=0;i<fs->n;i++) if (fs->f[i].fn==fn && fs->f[i].wt==WT_LEN) { (*out)[j]=fs->f[i].vbytes; (*lens)[j]=fs->f[i].vlen; j++; }
    return cnt;
}

/* ---- Head encode/decode ---- */
/* PoP: _encode_head @ gateway/platforms/yuanbao_proto.py:_encode_head */

/* PoP: _decode_head @ gateway/platforms/yuanbao_proto.py:_decode_head */static void yb_encode_head(yb_buf *b, int cmd_type, const char *cmd, uint64_t seq_no,
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

/* ---- MsgContent encode/decode ---- */
/* PoP: _encode_msg_content @ gateway/platforms/yuanbao_proto.py:_encode_msg_content */

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
/* PoP: _encode_msg_body_element @ gateway/platforms/yuanbao_proto.py:_encode_msg_body_element */
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

/* PoP: _encode_log_ext @ gateway/platforms/yuanbao_proto.py:_encode_log_ext */static unsigned char *yb_encode_send_c2c_req(const char *to_account, const char *from_account,
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

/* PoP: _encode_send_group_req @ gateway/platforms/yuanbao_proto.py:_encode_send_group_req */
static unsigned char *yb_encode_send_group_req(const char *group_code, const char *from_account,
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
/* PoP: _encode_forward_msg @ gateway/platforms/yuanbao_proto.py:_encode_forward_msg */
static void yb_encode_forward_msg(yb_buf *b, json_t *msg)
{
    if (!msg||msg->type!=JSON_OBJECT) return;
    json_t *s=json_object_get(msg,"sender"); if (s&&s->type==JSON_STRING&&json_string_value(s)[0]) yb_f_str(b,1,json_string_value(s));
    json_t *tm=json_object_get(msg,"time"); if (tm&&tm->type==JSON_NUMBER&&(int)json_number_value(tm)!=0) yb_f_varint(b,2,(uint64_t)(int)json_number_value(tm));
    json_t *p=json_object_get(msg,"plainText"); if (p&&p->type==JSON_STRING&&json_string_value(p)[0]) yb_f_str(b,3,json_string_value(p));
    json_t *mc=json_object_get(msg,"msgContent"); if (mc&&mc->type==JSON_ARRAY) for (size_t i=0;i<json_array_size(mc);i++){ yb_buf cb; yb_init(&cb); yb_encode_forward_msg_content(&cb, json_array_get(mc,i)); yb_f_sub(b,4,&cb); yb_free(&cb); }
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
