/* gate: yuanbao_media_parse_webp_size — traced faithful copy of
 * src/hermes_cli/platforms_yuanbao_media.c::
 * cli_gateway_platforms_yuanbao_media__parse_webp_size over hex-encoded
 * buffers. LIVE twin: gateway/platforms/yuanbao_media.py::_parse_webp_size.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static int parse_webp(const unsigned char *data, int data_len, int *width, int *height) {
    if (!data || data_len < 30 || !width || !height) return -1;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) return -1;
    if (memcmp(data + 12, "VP8 ", 4) == 0) {
        *width = (data[26] | (data[27] << 8)) & 0x3FFF;
        *height = (data[28] | (data[29] << 8)) & 0x3FFF;
        return 0;
    }
    if (memcmp(data + 12, "VP8L", 4) == 0) {
        int bits = data[21] | (data[22] << 8) | (data[23] << 16) | (data[24] << 24);
        *width = (bits & 0x3FFF) + 1;
        *height = ((bits >> 14) & 0x3FFF) + 1;
        return 0;
    }
    return -1;
}

static void trace_one(const char *hex) {
    WCUP_BEGIN("parse_webp_size");
    unsigned char buf[512] = {0};
    int len = 0;
    if (hex) {
        for (const char *p = hex; p[0] && p[1] && len < (int)sizeof buf; p += 2) {
            char hb[3] = {p[0], p[1], 0};
            buf[len++] = (unsigned char)strtoul(hb, NULL, 16);
        }
    }
    int w = -1, h = -1;
    bool too_short = len < 30 || len == 0;
    WCUP_BRANCH(too_short);
    bool bad_header = !too_short &&
        (memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WEBP", 4) != 0);
    WCUP_BRANCH(bad_header);
    bool is_vp8 = !too_short && !bad_header && memcmp(buf + 12, "VP8 ", 4) == 0;
    WCUP_BRANCH(is_vp8);
    bool is_vp8l = !is_vp8 && !too_short && !bad_header &&
        memcmp(buf + 12, "VP8L", 4) == 0;
    WCUP_BRANCH(is_vp8l);
    char vbuf[64];
    if (!too_short && !bad_header) parse_webp(buf, len, &w, &h);
    if (w >= 0) snprintf(vbuf, sizeof vbuf, "width=%d,height=%d", w, h);
    else snprintf(vbuf, sizeof vbuf, "none");
    WCUP_VALSTR(vbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("parse_webp_size");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_webp_size <hexbytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
