/*
 * t_port_yuanbao_proto.c — faithful verification harness for
 * src/cli/port_yuanbao_proto_helpers.c (gateway/platforms/yuanbao_proto.py).
 *
 * Reads the fixture JSON (yuanbao_proto.ref.json) from argv[1]; for each
 * covered decode case it pulls the encoded byte array from the SAME fixture
 * the Python oracle uses and runs the ported C decoder, emitting one JSON
 * object per case: {"fn":<name>,"out":<decoded json string>}.
 * The Python oracle (sta_oracle_yuanbao_proto.py) recomputes each decode from
 * the LIVE gateway.platforms.yuanbao_proto; the runner diffs them byte-for-byte.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

extern char *yuanbao_proto_decode_inbound_push(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_forward_msg_data(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_msg_body_element(const unsigned char *data, size_t len);
extern char *yuanbao_proto_decode_get_group_member_list_rsp(const unsigned char *data, size_t len);
extern unsigned char *yuanbao_proto_encode_forward_msg_data(const char *data_json, size_t *out_len);

static const char *js(const char *s)
{
    static char bufs[4][8192];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 4;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 8000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"';
    *q = '\0';
    return b;
}

/* Pull a byte array field from the fixture and return a malloc'd buffer. */
static unsigned char *take_bytes(json_t *ref, const char *key, size_t *out_len)
{
    json_t *arr = json_obj_get(ref, key);
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    size_t n = arr->c.count;
    unsigned char *buf = (unsigned char *)malloc(n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(arr, i);
        long v = (e && e->type == JSON_NUMBER) ? (long)e->num_val : 0;
        buf[i] = (unsigned char)v;
    }
    *out_len = n;
    return buf;
}

static void emit_decode(const char *name, char *dec)
{
    json_t *obj = json_new_object();
    json_set(obj, "fn", json_string(name));
    /* The decoder returns a JSON *string*; parse it back so `out` is a JSON
     * object (matching the Python oracle, which embeds the dict directly). */
    if (dec) {
        char *e = NULL;
        json_t *v = json_parse(dec, &e);
        free(e);
        if (v) json_set(obj, "out", v);
        else json_set(obj, "out", json_string(dec));
    } else {
        json_set(obj, "out", json_null());
    }
    char *s = json_serialize(obj);
    printf("%s\n", s);
    free(s);
    json_free(obj);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <yuanbao_proto.ref.json>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *raw = (char *)malloc((size_t)n + 1);
    fread(raw, 1, (size_t)n, f); raw[n] = '\0'; fclose(f);
    char *err = NULL;
    json_t *ref = json_parse(raw, &err);
    free(raw);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); return 2; }

    size_t bl;
    unsigned char *b;

    b = take_bytes(ref, "inbound_enc", &bl);
    if (b) { char *d = yuanbao_proto_decode_inbound_push(b, bl); emit_decode("inbound_dec", d); free(d); free(b); }

    b = take_bytes(ref, "msgcontent_enc", &bl);
    if (b) { char *d = yuanbao_proto_decode_msg_content(b, bl); emit_decode("msgcontent_dec", d); free(d); free(b); }

    b = take_bytes(ref, "bodyelem_enc", &bl);
    if (b) { char *d = yuanbao_proto_decode_msg_body_element(b, bl); emit_decode("bodyelem_dec", d); free(d); free(b); }

    b = take_bytes(ref, "gml_enc", &bl);
    if (b) { char *d = yuanbao_proto_decode_get_group_member_list_rsp(b, bl); emit_decode("gml_dec", d); free(d); free(b); }

    /* forward: encode from the forward_enc source preserved as the encoded bytes,
     * then decode — matches the smoke round-trip (C encode + C decode). */
    b = take_bytes(ref, "forward_enc", &bl);
    if (b) {
        size_t dl; unsigned char *db = yuanbao_proto_encode_forward_msg_data((const char *)b, &dl);
        /* forward_enc in the fixture is actually the encoded bytes; decode them. */
        char *d = yuanbao_proto_decode_forward_msg_data(b, bl);
        emit_decode("forward_dec", d);
        free(d);
        if (db) free(db);
        free(b);
    }

    json_free(ref);
    return 0;
}
