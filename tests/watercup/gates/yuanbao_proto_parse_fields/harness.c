/* gate: yuanbao_proto_parse_fields — traced copy of
 * src/hermes_cli/yuanbao_proto_helpers.c::yb_parse_fields (+ yb_decode_varint).
 * LIVE twin: gateway/platforms/yuanbao_proto.py::_parse_fields.
 * Input: hex-encoded protobuf message.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   per record: LOOP tick
 *     B1: tag varint ok
 *     B2: wire_type == VARINT -> read varint value
 *       else B3: == LEN -> length + bytes
 *       else B4: == 64BIT / B5: == 32BIT / else B6: unknown -> stop(err)
 *   V per field: "<fn>:<wt>:<hex-or-int>"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "watercup.h"

static int yb_decode_varint(const unsigned char *d, size_t n, size_t *pos, uint64_t *out) {
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

static void wc_parse_fields(const unsigned char *d, size_t n) {
    WCUP_BEGIN("parse_fields");
    size_t pos = 0;
    while (pos < n) {
        WCUP_LOOP_TICK();
        uint64_t tag;
        if (!yb_decode_varint(d, n, &pos, &tag)) { WCUP_BRANCH(0); break; } /* B1 fail */
        WCUP_BRANCH(1);
        int fn = (int)(tag >> 3), wt = (int)(tag & 0x07);
        char vbuf[512];
        if (wt == 0) {                                          /* B2 VARINT */
            WCUP_BRANCH(1);
            uint64_t v;
            if (!yb_decode_varint(d, n, &pos, &v)) break;
            snprintf(vbuf, sizeof vbuf, "%d:0:%llu", fn, (unsigned long long)v);
        } else if (wt == 2) {                                   /* B3 LEN */
            WCUP_BRANCH(0); WCUP_BRANCH(1);
            uint64_t len;
            if (!yb_decode_varint(d, n, &pos, &len)) break;
            size_t avail = (pos + len <= n) ? (size_t)len : (n - pos);
            char hex[256];
            size_t hl = 0;
            for (size_t i = 0; i < avail && hl + 2 < sizeof hex; i++)
                hl += (size_t)snprintf(hex + hl, sizeof hex - hl, "%02x", d[pos + i]);
            hex[hl] = '\0';
            pos += avail;
            snprintf(vbuf, sizeof vbuf, "%d:2:%s", fn, hex);
        } else if (wt == 1) {                                   /* B4 64BIT */
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
            if (pos + 8 > n) break;
            char hex[32]; size_t hl = 0;
            for (int i = 0; i < 8; i++) hl += (size_t)snprintf(hex+hl, sizeof hex-hl, "%02x", d[pos+i]);
            pos += 8;
            snprintf(vbuf, sizeof vbuf, "%d:1:%s", fn, hex);
        } else if (wt == 5) {                                   /* B5 32BIT */
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(1);
            if (pos + 4 > n) break;
            char hex[16]; size_t hl = 0;
            for (int i = 0; i < 4; i++) hl += (size_t)snprintf(hex+hl, sizeof hex-hl, "%02x", d[pos+i]);
            pos += 4;
            snprintf(vbuf, sizeof vbuf, "%d:5:%s", fn, hex);
        } else {                                                /* B6 unknown */
            WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0); WCUP_BRANCH(0);
            snprintf(vbuf, sizeof vbuf, "ERR:%d", wt);
            WCUP_VALSTR(vbuf);
            goto emit;
        }
        WCUP_VALSTR(vbuf);                                      /* V per field */
    }
emit:
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("parse_fields");
    }
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_fields <hexbytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *h = argv[i];
        size_t hl = strlen(h), n = hl / 2;
        unsigned char *d = malloc(n ? n : 1);
        for (size_t j = 0; j < n; j++)
            d[j] = (unsigned char)((hexval(h[2*j]) << 4) | hexval(h[2*j+1]));
        wc_parse_fields(d, n);
        free(d);
    }
    return 0;
}
