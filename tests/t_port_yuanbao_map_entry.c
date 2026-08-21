/*
 * t_port_yuanbao_map_entry.c — parity harness for the map-entry codec helpers
 * in src/hermes_cli/yuanbao_proto_helpers.c
 * (gateway/platforms/yuanbao_proto.py: _encode_map_entry, _decode_map_entry).
 *
 * Reads a JSON fixture (argv[1]) with "ops": each op is either
 *   {"op":"encode","key":..,"value":..} -> call encode, hexdump bytes
 *   {"op":"decode","hex":..}            -> call decode, emit [key,value]
 * Emits one JSON line per op, matching sta_oracle_yuanbao_map_entry.py.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Implemented in src/hermes_cli/yuanbao_proto_helpers.c (wired build). */
unsigned char *yuanbao_proto_encode_map_entry(const char *key, const char *value, size_t *out_len);
char *yuanbao_proto_decode_map_entry(const unsigned char *data, size_t len);

static char *read_all(const char *p) {
    FILE *f = fopen(p, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(n + 1); size_t r = fread(b, 1, n, f); b[r] = '\0';
    fclose(f); return b;
}

/* hex-encode bytes */
static void hexout(const unsigned char *d, size_t n, char *out, size_t outsz) {
    size_t j = 0;
    for (size_t i = 0; i < n && j + 2 < outsz; i++)
        j += (size_t)snprintf(out + j, outsz - j, "%02x", d[i]);
    out[j] = '\0';
}

/* hex-decode (even-length ascii hex) */
static size_t hexdec(const char *h, unsigned char *out, size_t outsz) {
    size_t n = strlen(h) / 2;
    if (n > outsz) n = outsz;
    for (size_t i = 0; i < n; i++) {
        unsigned int b = 0;
        sscanf(h + 2 * i, "%2x", &b);
        out[i] = (unsigned char)b;
    }
    return n;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_all(argv[1]);
    if (!raw) { fprintf(stderr, "cannot read fixture\n"); return 1; }
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *op = json_get_str(c, "op", "");
            if (strcmp(op, "encode") == 0) {
                const char *k = json_get_str(c, "key", "");
                const char *v = json_get_str(c, "value", "");
                size_t blen = 0;
                unsigned char *b = yuanbao_proto_encode_map_entry(k, v, &blen);
                char hex[1024]; hexout(b ? b : (const unsigned char *)"", b ? blen : 0, hex, sizeof(hex));
                json_t *o = json_new_object();
                json_object_set(o, "op", json_string("encode"));
                json_object_set(o, "key", json_string(k));
                json_object_set(o, "value", json_string(v));
                json_object_set(o, "hex", json_string(hex));
                char *line = json_dumps(o, 0);
                printf("%s\n", line);
                free(line); json_free(o); free(b);
            } else if (strcmp(op, "decode") == 0) {
                const char *h = json_get_str(c, "hex", "");
                unsigned char buf[2048];
                size_t n = hexdec(h, buf, sizeof(buf));
                char *dec = yuanbao_proto_decode_map_entry(buf, n);
                json_t *o = json_new_object();
                json_object_set(o, "op", json_string("decode"));
                json_object_set(o, "hex", json_string(h));
                /* dec is a JSON array string; embed it raw via json_parse */
                json_t *arr = dec ? json_parse(dec, NULL) : NULL;
                if (arr) json_object_set(o, "out", arr);
                else json_object_set(o, "out", json_new_null());
                char *line = json_dumps(o, 0);
                printf("%s\n", line);
                free(line); json_free(o); free(dec);
            }
        }
    }
    json_free(root); free(raw);
    return 0;
}
