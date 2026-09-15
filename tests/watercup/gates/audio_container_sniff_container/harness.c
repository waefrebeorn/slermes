/* gate: audio_container_sniff_container — traced C copy of
 * src/tools/audio_container.c::pca_sniff_container (PoP:
 * tools/audio_container.py:sniff_container).
 * Input per line of argv: hex-encoded magic bytes.
 * WCUP_* records the SAME control structure LIVE Python takes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static const char *sniff(const unsigned char *data, size_t len, char *buf, size_t bufsz) {
    WCUP_BEGIN("sniff_container");
    const char *result = NULL;
    /* ftyp box: bytes 4-7 == "ftyp", brand at 8-11. */
    if (len >= 12 && memcmp(data + 4, "ftyp", 4) == 0) {
        WCUP_BRANCH(1);
        char brand[5];
        memcpy(brand, data + 8, 4);
        for (int i = 0; i < 4; i++) brand[i] = (char)tolower(brand[i]);
        brand[4] = '\0';
        if (strcmp(brand, "m4a ") == 0 || strcmp(brand, "m4b ") == 0) {
            WCUP_BRANCH(1);
            result = "m4a";
        } else {
            WCUP_BRANCH(0);
            result = "mp4";
        }
    } else {
        WCUP_BRANCH(0);
    }
    if (!result && len >= 4 && memcmp(data, "OggS", 4) == 0) {
        WCUP_BRANCH(1); result = "ogg";
    } else if (!result) {
        WCUP_BRANCH(0);
    }
    if (!result && len >= 4 && memcmp(data, "fLaC", 4) == 0) {
        WCUP_BRANCH(1); result = "flac";
    } else if (!result) {
        WCUP_BRANCH(0);
    }
    if (!result && len >= 12 && memcmp(data, "RIFF", 4) == 0 &&
        memcmp(data + 8, "WAVE", 4) == 0) {
        WCUP_BRANCH(1); result = "wav";
    } else if (!result) {
        WCUP_BRANCH(0);
    }
    if (!result && len >= 3 && memcmp(data, "ID3", 3) == 0) {
        WCUP_BRANCH(1); result = "mp3";
    } else if (!result) {
        WCUP_BRANCH(0);
    }
    if (!result && len >= 2 && data[0] == 0xFF && (data[1] & 0xE0) == 0xE0) {
        WCUP_BRANCH(1);
        if ((data[1] & 0xF6) == 0xF0) {
            WCUP_BRANCH(1);
            result = "aac";
        } else {
            WCUP_BRANCH(0);
            result = "mp3";
        }
    } else if (!result) {
        WCUP_BRANCH(0);
    }
    if (!result && len >= 4 && memcmp(data, "\x1a\x45\xdf\xa3", 4) == 0) {
        WCUP_BRANCH(1); result = "webm";
    } else if (!result) {
        WCUP_BRANCH(0);
    }
    snprintf(buf, bufsz, "%s", result ? result : "");
    WCUP_VALSTR(buf);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sniff_container");
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sniff_container <hexbytes>...\n", argv[0]); return 2; }
    unsigned char data[256];
    for (int i = 2; i < argc; i++) {
        const char *hx = argv[i];
        size_t n = 0;
        for (const char *p = hx; p[0] && p[1] && n < sizeof data; p += 2)
            data[n++] = (unsigned char)((hexval(p[0]) << 4) | hexval(p[1]));
        char buf[16];
        sniff(data, n, buf, sizeof buf);
    }
    return 0;
}
