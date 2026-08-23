/* gate: base_looks_like_image — traced C copy of
 * src/gateway/platforms/base.c::pb_looks_like_image.
 * PoP of LIVE gateway/platforms/base.py::_looks_like_image (magic-byte ladder).
 * Inputs are hex-encoded byte strings; both sides decode hex first so the
 * real byte-level logic is exercised via argv-safe input tokens.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static size_t unhex(const char *hex, unsigned char *out, size_t max) {
    size_t n = 0;
    for (size_t i = 0; hex[i] && n < max; i += 2) {
        int hi = hexval(hex[i]);
        int lo = hex[i + 1] ? hexval(hex[i + 1]) : -1;
        if (hi < 0 || lo < 0) break;
        out[n++] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

static void looks_like_image(const char *arg) {
    WCUP_BEGIN("_looks_like_image");
    unsigned char buf[64];
    size_t len = arg ? unhex(arg, buf, sizeof buf) : 0;
    const unsigned char *data = buf;
    WCUP_VALSTR(arg ? arg : "");

    int result = 0;
    if (!data || len < 4) {
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        if (len >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) {
            WCUP_BRANCH(1);
            result = 1;
        } else {
            WCUP_BRANCH(0);
            if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
                WCUP_BRANCH(1);
                result = 1;
            } else {
                WCUP_BRANCH(0);
                if (len >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) {
                    WCUP_BRANCH(1);
                    result = 1;
                } else {
                    WCUP_BRANCH(0);
                    if (len >= 2 && data[0] == 'B' && data[1] == 'M') {
                        WCUP_BRANCH(1);
                        result = 1;
                    } else {
                        WCUP_BRANCH(0);
                        if (len >= 12 && memcmp(data, "RIFF", 4) == 0 &&
                            memcmp(data + 8, "WEBP", 4) == 0) {
                            WCUP_BRANCH(1);
                            result = 1;
                        } else {
                            WCUP_BRANCH(0);
                        }
                    }
                }
            }
        }
    }
    char rbuf[8];
    snprintf(rbuf, sizeof rbuf, "%s", result ? "True" : "False");
    WCUP_VALSTR(rbuf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_looks_like_image");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _looks_like_image <hexbytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) looks_like_image(argv[i]);
    return 0;
}
