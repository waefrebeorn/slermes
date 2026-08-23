/* gate: cua_backend_image_dimensions_from_bytes — traced copy of
 * src/hermes_cli/cua_backend_helpers.c::cua_backend_image_dimensions_from_bytes.
 * LIVE twin: tools/computer_use/cua_backend.py::_image_dimensions_from_bytes.
 *
 * Inputs arrive as hex-encoded byte strings (argv-safe); both sides decode.
 * Rung order (must match twin.py EXACTLY):
 *   B1  raw present and n >= 2
 *   B2  PNG magic && n >= 24
 *   B3    width > 0 && height > 0
 *   B4  JPEG FFD8
 *   L     per marker walk step
 *   B5      byte != 0xFF (skip)
 *   B6      standalone marker (D8/D9/D0-D7)
 *   B7      i+2 > n (truncated)
 *   B8      seg_len < 2 || segment overruns buffer
 *   B9      SOF marker
 *   B10       seg_len >= 7
 *   B11       width > 0 && height > 0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

static void dims(const unsigned char *raw, size_t n, char *out, size_t outsz) {
    WCUP_BEGIN("image_dimensions_from_bytes");
    int w = 0, h = 0;

    if (raw && n >= 2) {                                        /* B1 */
        WCUP_BRANCH(1);
        static const unsigned char png_magic[8] = {0x89,'P','N','G','\r','\n',0x1a,'\n'};
        if (n >= 24 && memcmp(raw, png_magic, 8) == 0) {        /* B2 */
            WCUP_BRANCH(1);
            w = ((int)raw[16] << 24) | ((int)raw[17] << 16) | ((int)raw[18] << 8) | (int)raw[19];
            h = ((int)raw[20] << 24) | ((int)raw[21] << 16) | ((int)raw[22] << 8) | (int)raw[23];
            if (w > 0 && h > 0) { WCUP_BRANCH(1); }             /* B3 */
            else WCUP_BRANCH(0);
            goto done;
        }
        WCUP_BRANCH(0);

        if (raw[0] == 0xFF && raw[1] == 0xD8) {                 /* B4 */
            WCUP_BRANCH(1);
            size_t i = 2;
            while (i + 9 < n) {
                WCUP_LOOP_TICK();
                if (raw[i] != 0xFF) { WCUP_BRANCH(1); i++; continue; }   /* B5 */
                WCUP_BRANCH(0);
                unsigned char marker = raw[i + 1];
                i += 2;
                if (marker == 0xD8 || marker == 0xD9 ||
                    (marker >= 0xD0 && marker <= 0xD7)) {       /* B6 */
                    WCUP_BRANCH(1);
                    continue;
                }
                WCUP_BRANCH(0);
                if (i + 2 > n) { WCUP_BRANCH(1); break; }       /* B7 */
                WCUP_BRANCH(0);
                int seg_len = ((int)raw[i] << 8) | (int)raw[i + 1];
                if (seg_len < 2 || i + (size_t)seg_len > n) { WCUP_BRANCH(1); break; }  /* B8 */
                WCUP_BRANCH(0);
                switch (marker) {
                    case 0xC0: case 0xC1: case 0xC2: case 0xC3:
                    case 0xC5: case 0xC6: case 0xC7:
                    case 0xC9: case 0xCA: case 0xCB:
                    case 0xCD: case 0xCE: case 0xCF:
                        WCUP_BRANCH(1);                          /* B9 SOF */
                        if (seg_len >= 7) {                      /* B10 */
                            WCUP_BRANCH(1);
                            int hh = ((int)raw[i + 3] << 8) | (int)raw[i + 4];
                            int ww = ((int)raw[i + 5] << 8) | (int)raw[i + 6];
                            if (ww > 0 && hh > 0) { w = ww; h = hh; WCUP_BRANCH(1); }  /* B11 */
                            else WCUP_BRANCH(0);
                        } else WCUP_BRANCH(0);
                        goto done;                               /* SOF reached */
                    default:
                        WCUP_BRANCH(0);
                        break;
                }
                i += (size_t)seg_len;
            }
        } else {
            WCUP_BRANCH(0);
        }
    } else {
        WCUP_BRANCH(0);
    }

done:
    snprintf(out, outsz, "%dx%d", w, h);
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("image_dimensions_from_bytes");
}

static size_t hexdec(const char *hx, unsigned char *buf, size_t cap) {
    size_t n = strlen(hx) / 2;
    if (n > cap) n = cap;
    for (size_t i = 0; i < n; i++) {
        unsigned v = 0;
        for (int j = 0; j < 2; j++) {
            char c = hx[2 * i + j];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        }
        buf[i] = (unsigned char)v;
    }
    return n;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s image_dimensions_from_bytes <hexbytes>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        unsigned char buf[4096];
        size_t n = hexdec(argv[i], buf, sizeof buf);
        char out[64];
        dims(buf, n, out, sizeof out);
    }
    return 0;
}
