/*
 * t_port_file_fs_ops.c — oracle harness for the byte-layer binary helpers of
 * tools/file_operations.py ported to C (file_fs_ops.c / file_fs_ops.h).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE tools/file_operations.py.
 *
 * Tested: identify_binary_bytes, describe_binary_file, _is_likely_binary_bytes,
 * _not_regular_error, _size_probe_cmd.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "file_fs_ops.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

static char *jesc(const char *s) {
    static char buf[4][8192];
    static int cur = 0;
    char *q = buf[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
            else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
            else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
            else if (c < 32) { *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
                *q++ = (c > 15 ? '1' : '0'); *q++ = (c % 16 < 10 ? c % 16 + '0' : c % 16 - 10 + 'a'); }
            else if ((q - base) < 8170) *q++ = c;
        }
    }
    *q++ = '"'; *q = '\0';
    return base;
}

/* decode a base64-encoded sample string into bytes */
static unsigned char *b64decode(const char *s, size_t *outlen) {
    size_t slen = strlen(s);
    unsigned char *out = malloc(slen);
    size_t o = 0, val = 0, bits = 0;
    for (size_t i = 0; i < slen; i++) {
        unsigned char c = (unsigned char)s[i];
        int d;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        else continue;
        val = (val << 6) | (unsigned)d;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out[o++] = (unsigned char)((val >> bits) & 0xFF);
        }
    }
    *outlen = o;
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_file_fs_ops <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");
        json_t *arg = json_obj_get(c, "arg");

        if (strcmp(op, "identify") == 0) {
            const char *b64 = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            size_t blen = 0;
            unsigned char *bytes = b64decode(b64, &blen);
            char *r = file_fs_ops_identify_binary_bytes(bytes, blen);
            printf("{\"op\":\"identify\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r); free(bytes);
        } else if (strcmp(op, "describe") == 0) {
            json_t *sample = json_obj_get(c, "sample");
            const char *b64 = (sample && sample->type == JSON_STRING) ? sample->str_val : "";
            size_t blen = 0;
            unsigned char *bytes = b64decode(b64, &blen);
            long long fsz = (arg && arg->type == JSON_NUMBER) ? (long long)arg->num_val : 0;
            char *r = file_fs_ops_describe_binary_file(bytes, blen, fsz);
            printf("{\"op\":\"describe\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r); free(bytes);
        } else if (strcmp(op, "is_binary_bytes") == 0) {
            const char *b64 = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            size_t blen = 0;
            unsigned char *bytes = b64decode(b64, &blen);
            printf("{\"op\":\"is_binary_bytes\",\"out\":%s}\n",
                   file_fs_ops_is_likely_binary_bytes(bytes, blen) ? "true" : "false");
            free(bytes);
        } else if (strcmp(op, "not_regular") == 0) {
            const char *p = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = file_fs_ops_not_regular_error(p);
            printf("{\"op\":\"not_regular\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else if (strcmp(op, "size_probe") == 0) {
            const char *p = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            char *r = file_fs_ops_size_probe_cmd(p);
            printf("{\"op\":\"size_probe\",\"out\":%s}\n", jesc(r ? r : ""));
            free(r);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
