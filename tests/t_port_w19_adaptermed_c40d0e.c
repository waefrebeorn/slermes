#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_adaptermed_c40d0e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a hex string fixture field into bytes. Returns byte count. */
static unsigned long hex_decode(const char *hex, unsigned char *out, unsigned long cap) {
    unsigned long n = 0;
    const char *p = hex ? hex : "";
    while (p[0] && p[1] && n < cap) {
        int hi = hexval((unsigned char)p[0]);
        int lo = hexval((unsigned char)p[1]);
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
        p += 2;
    }
    return n;
}

int main(void) {
    char line[65536];
    static unsigned char buf[16384];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "ogg_duration")) {
            unsigned long n = hex_decode(json_get_str(in, "hex", ""), buf, sizeof buf);
            out = w19_adaptermed_c40d0e_ogg_opus_duration(buf, n);
        } else if (!strcmp(func, "upload_limit")) {
            out = w19_adaptermed_c40d0e_upload_limit(json_obj_get(in, "guild"));
        } else if (!strcmp(func, "oversized")) {
            out = w19_adaptermed_c40d0e_oversized(json_get_str(in, "filename", ""),
                                                 (long long)json_get_num(in, "size", 0),
                                                 (long long)json_get_num(in, "limit", 1));
        } else if (!strcmp(func, "reply_count")) {
            out = w19_adaptermed_c40d0e_reply_count(json_get_str(in, "content", ""),
                                                   (long long)json_get_num(in, "max_len", 2000));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
