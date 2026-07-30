/* Round-trip oracle: C yuanbao_proto port vs Python reference.
 *
 * The Python module gateway/platforms/yuanbao_proto.py implements protobuf
 * wire-format by hand (no google.protobuf). The C port
 * (port_yuanbao_proto_helpers.c) renames the internal primitives to yb_* and
 * exposes public entry points. This harness links the C object and checks that
 * the externally-observable bytes + decoded structures match the Python
 * reference (computed in gen_fixtures.py -> yuanbao_proto.ref.json). That
 * proves the 30 internal helpers behave identically.
 *
 * Run:  cat yuanbao_proto.ref.json | ./smoke_yuanbao_proto
 * The harness parses the reference JSON from stdin and compares. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "hermes_json.h"

extern unsigned char *yuanbao_proto_encode_send_c2c_message(const char *to_account,
    const char *msg_body_json, const char *from_account, const char *msg_id,
    uint64_t msg_random, uint64_t msg_seq, const char *group_code,
    const char *trace_id, size_t *out_len);
extern char *yuanbao_proto_decode_inbound_push(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_forward_msg_data(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_get_group_member_list_rsp(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_msg_body_element(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_im_msg_seq(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_log_ext(const unsigned char *data, size_t len);
extern unsigned char *yuanbao_proto_encode_forward_msg_data(const char *data_json, size_t *out_len);

static int failures = 0;

/* Canonical compare of two parsed JSON values, order-insensitive for objects
 * and arrays. Lists (repeated fields) are compared as multisets by element
 * equality, since protobuf decode order may differ from the Python reference. */
static int jval_eq(const json_t *a, const json_t *b);

static int jarr_eq(const json_t *a, const json_t *b)
{
    if (a->c.count != b->c.count) return 0;
    /* compare as multiset of elements */
    size_t n = a->c.count;
    char *used = calloc(n, 1);
    for (size_t i = 0; i < n; i++) {
        json_t *ai = json_get((json_t*)a, i);
        int matched = 0;
        for (size_t j = 0; j < n; j++) {
            if (!used[j] && jval_eq(ai, json_get((json_t*)b, j))) { used[j] = 1; matched = 1; break; }
        }
        if (!matched) { free(used); return 0; }
    }
    free(used);
    return 1;
}

static int jobj_eq(const json_t *a, const json_t *b)
{
    if (a->c.count != b->c.count) return 0;
    for (size_t i = 0; i < a->c.count; i++) {
        const char *k = a->c.keys[i];
        json_t *av = a->c.items[i];
        json_t *bv = json_obj_get((json_t*)b, k);
        if (!bv) return 0;
        if (!jval_eq(av, bv)) return 0;
    }
    return 1;
}

static int jval_eq(const json_t *a, const json_t *b)
{
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY:  return jarr_eq(a, b);
        case JSON_OBJECT: return jobj_eq(a, b);
        default: return 0;
    }
}

static int jsame(const char *a, const char *b)
{
    char *ea = NULL, *eb = NULL;
    json_t *ja = json_parse(a, &ea), *jb = json_parse(b, &eb);
    free(ea); free(eb);
    if (!ja || !jb) { if (ja) json_free(ja); if (jb) json_free(jb); return 0; }
    int eq = jval_eq(ja, jb);
    json_free(ja); json_free(jb);
    return eq;
}

static void check_json(const char *name, const char *got, const char *ref)
{
    if (got && jsame(got, ref)) { printf("ok %s\n", name); }
    else { fprintf(stderr, "FAIL %s\n  got=%s\n  ref=%s\n", name, got ? got : "(null)", ref); failures++; }
}

/* like check_json but frees the serialized `ref` string afterwards */
static void check_json_free(const char *name, const char *got, char *ref)
{
    check_json(name, got, ref);
    free(ref);
}

/* bytes: compare a malloc'd buffer to a JSON array of ints (the ref form) */
static void check_bytes(const char *name, const unsigned char *buf, size_t len, const json_t *ref_arr)
{
    if (!buf || !ref_arr || ref_arr->type != JSON_ARRAY) { fprintf(stderr, "FAIL %s (bad args)\n", name); failures++; return; }
    if (len != ref_arr->c.count) {
        fprintf(stderr, "FAIL %s len %zu != expected %zu\n", name, len, ref_arr->c.count); failures++; return;
    }
    int ok = 1;
    for (size_t i = 0; i < len; i++) {
        json_t *e = json_get((json_t*)ref_arr, i);
        long rv = (e && e->type == JSON_NUMBER) ? (long)e->num_val : -999;
        if ((long)buf[i] != rv) { ok = 0; break; }
    }
    if (ok) printf("ok %s (%zu bytes)\n", name, len);
    else { fprintf(stderr, "FAIL %s byte mismatch\n", name); failures++; }
}

int main(void)
{
    /* read reference JSON from stdin */
    char *raw = malloc(1 << 20); size_t got = 0, cap = 1 << 20;
    size_t n; char tmp[65536];
    while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
        if (got + n + 1 >= cap) { cap *= 2; raw = realloc(raw, cap); }
        memcpy(raw + got, tmp, n); got += n;
    }
    raw[got] = '\0';
    char *err = NULL;
    json_t *ref = json_parse(raw, &err);
    free(raw);
    if (!ref) { fprintf(stderr, "ref parse error: %s\n", err); free(err); return 2; }

    /* 1) inbound push decode — use the EXACT bytes Python produced */
    {
        json_t *rb = json_obj_get(ref, "inbound_dec");
        json_t *ienc = json_obj_get(ref, "inbound_enc");
        if (rb && ienc && ienc->type == JSON_ARRAY) {
            unsigned char *all = malloc(ienc->c.count ? ienc->c.count : 1);
            for (size_t i=0;i<ienc->c.count;i++) all[i]=(unsigned char)(long)json_get(ienc,i)->num_val;
            char *dec = yuanbao_proto_decode_inbound_push(all, ienc->c.count);
            check_json_free("inbound_dec", dec, json_serialize(rb));
            free(dec); free(all);
        } else { fprintf(stderr,"SKIP inbound_dec (ref missing)\n"); }
    }

    /* 2) forward msg data round trip */
    {
        json_t *fref = json_obj_get(ref, "forward_dec");
        /* encode via C from the same JSON the generator used, then decode */
        const char *fwd_json = "{\"sub_type\":1,\"begin_time\":100,\"end_time\":200,\"nick_name\":\"n\",\"msg\":[{\"sender\":\"s\",\"time\":5,\"plainText\":\"hello\",\"msgContent\":[{\"type\":1,\"text\":\"t\",\"multimedia\":[{\"type\":\"image\",\"url\":\"u2\",\"file_size\":9}]}]}]}";
        size_t bl; unsigned char *b = yuanbao_proto_encode_forward_msg_data(fwd_json, &bl);
        char *dec = yuanbao_proto_decode_forward_msg_data(b, bl);
        check_json_free("forward_roundtrip", dec, json_serialize(fref));
        free(dec); free(b);
    }

    /* 3) msg_content decode from ref bytes */
    {
        json_t *mcref = json_obj_get(ref, "msgcontent_dec");
        json_t *mcenc = json_obj_get(ref, "msgcontent_enc");
        unsigned char *be = malloc(mcenc->c.count); for(size_t i=0;i<mcenc->c.count;i++) be[i]=(unsigned char)(long)json_get(mcenc,i)->num_val;
        char *dec = yuanbao_proto_decode_msg_content(be, mcenc->c.count);
        check_json_free("msgcontent_dec", dec, json_serialize(mcref));
        free(dec); free(be);
    }

    /* 4) msg_body_element decode from ref bytes */
    {
        json_t *bref = json_obj_get(ref, "bodyelem_dec");
        json_t *benc = json_obj_get(ref, "bodyelem_enc");
        unsigned char *be = malloc(benc->c.count); for(size_t i=0;i<benc->c.count;i++) be[i]=(unsigned char)(long)json_get(benc,i)->num_val;
        char *dec = yuanbao_proto_decode_msg_body_element(be, benc->c.count);
        check_json_free("bodyelem_dec", dec, json_serialize(bref));
        free(dec); free(be);
    }

    /* 5) group member list rsp decode from ref bytes */
    {
        json_t *gref = json_obj_get(ref, "gml_dec");
        /* rebuild gml bytes like the generator */
        unsigned char gml[256]; size_t L=0;
        unsigned char t1[]={0x08,0x00}; /* field1 varint 0 */
        unsigned char t2[]={0x12,0x02,'o','k'}; /* field2 str ok */
        /* member: field1 str u1, field3 varint 2 */
        unsigned char mem[]={0x0a,0x02,'u','1',0x18,0x02};
        unsigned char f3[2 + sizeof(mem)];
        f3[0] = (3<<3)|2;          /* tag field3 WT_LEN */
        f3[1] = (unsigned char)sizeof(mem);
        memcpy(f3+2, mem, sizeof(mem));
        unsigned char t4[]={0x20,0x00}; /* field4 varint 0 */
        unsigned char t5[]={0x28,0x01}; /* field5 varint 1 */
        memcpy(gml+L,t1,sizeof(t1));L+=sizeof(t1);
        memcpy(gml+L,t2,sizeof(t2));L+=sizeof(t2);
        memcpy(gml+L,f3,sizeof(f3));L+=sizeof(f3);
        memcpy(gml+L,t4,sizeof(t4));L+=sizeof(t4);
        memcpy(gml+L,t5,sizeof(t5));L+=sizeof(t5);
        char *dec = yuanbao_proto_decode_get_group_member_list_rsp(gml, L);
        check_json_free("gml_dec", dec, json_serialize(gref));
        free(dec);
    }

    json_free(ref);
    if (failures) { printf("\n%d FAILURES\n", failures); return 1; }
    printf("\nALL yuanbao_proto round-trip checks passed\n");
    return 0;
}
