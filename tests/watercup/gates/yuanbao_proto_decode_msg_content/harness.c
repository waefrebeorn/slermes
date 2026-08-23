/* gate: yuanbao_proto_decode_msg_content — drives the REAL
 * src/hermes_cli/yuanbao_proto_helpers.c::yuanbao_proto_decode_msg_content
 * (PoP: gateway/platforms/yuanbao_proto.py::_decode_msg_content).
 *
 * Input: hex-encoded protobuf MsgContent bytes.
 *
 * Rung ladder (must match twin.py EXACTLY):
 *   B1 data nonempty
 *   V(canonical decoded content: sorted-key "k=v" pairs joined by ";",
 *     arrays joined by "|", from the REAL C decoder)
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include "json.h"

char *yuanbao_proto_decode_msg_content(const unsigned char *data, size_t len);

static size_t hex_decode(const char *hex, unsigned char **out) {
    size_t n = strlen(hex) / 2;
    unsigned char *buf = malloc(n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        unsigned b; sscanf(hex + 2 * i, "%2x", &b);
        buf[i] = (unsigned char)b;
    }
    *out = buf;
    return n;
}

/* canonical form shared with twin.py */
static void canon(const json_t *v, char *dst, size_t cap, size_t *off) {
    if (!v) return;
    if (v->type == JSON_OBJECT) {
        /* insertion order == both impls' key order; sort keys to be stable */
        int idx[32]; size_t n = v->c.count < 32 ? v->c.count : 32;
        for (size_t i = 0; i < n; i++) idx[i] = (int)i;
        for (size_t a = 1; a < n; a++)
            for (size_t b = a; b > 0 && strcmp(v->c.keys[idx[b-1]], v->c.keys[idx[b]]) > 0; b--) {
                int t = idx[b-1]; idx[b-1] = idx[b]; idx[b] = t;
            }
        int first = 1;
        for (size_t i = 0; i < n; i++) {
            const char *k = v->c.keys[idx[i]];
            if (!first && *off < cap) dst[(*off)++] = ';';
            first = 0;
            for (const char *q = k; *q && *off < cap; q++) dst[(*off)++] = *q;
            if (*off < cap) dst[(*off)++] = '=';
            canon(v->c.items[idx[i]], dst, cap, off);
        }
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) {
            if (i && *off < cap) dst[(*off)++] = '|';
            canon(v->c.items[i], dst, cap, off);
        }
    } else if (v->type == JSON_STRING) {
        for (const char *q = v->str_val ? v->str_val : ""; *q && *off < cap; q++) dst[(*off)++] = *q;
    } else if (v->type == JSON_NUMBER) {
        *off += (size_t)snprintf(dst + *off, cap - *off, "%lld", (long long)v->num_val);
    } else if (v->type == JSON_BOOL) {
        const char *b = v->bool_val ? "True" : "False";
        while (*b && *off < cap) dst[(*off)++] = *b++;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_yuanbao_msg_content <hexbytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        unsigned char *data = NULL; size_t len = hex_decode(argv[i], &data);

        WCUP_BEGIN("decode_msg_content");
        WCUP_BRANCH(len > 0);                                        /* B1 */

        char *js = yuanbao_proto_decode_msg_content(data, len);
        json_t *root = js ? json_parse(js, NULL) : NULL;
        char vs[2048]; size_t off = 0;
        if (root) canon(root, vs, sizeof vs - 1, &off);
        vs[off] = '\0';
        WCUP_VALSTR(vs);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("decode_msg_content");
        free(data);
        if (root) json_free(root);
        free(js);
    }
    return 0;
}
