/*
 * _jsonfile.c — C port of upstream Hermes plugins/google_meet/_jsonfile.py.
 * PoP: gmeetjson_read_json @ plugins/google_meet/_jsonfile.py:read_json
 */
#include "noc_gmeetjson.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* read_json(path): parsed JSON from path, or NULL (None) when missing,
 * unreadable, empty or malformed. Python reads with encoding="utf-8-sig",
 * so a leading UTF-8 BOM (EF BB BF) is stripped before parsing. */
json_t *gmeetjson_read_json(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { fclose(f); return NULL; }
    for (;;) {
        if (len + 1 >= cap) {
            size_t ncap = cap * 2;
            char *nb = (char *)realloc(buf, ncap);
            if (!nb) { free(buf); fclose(f); return NULL; }
            buf = nb; cap = ncap;
        }
        size_t got = fread(buf + len, 1, cap - len - 1, f);
        len += got;
        if (got == 0) break;
    }
    fclose(f);
    buf[len] = '\0';

    const char *start = buf;
    if (len >= 3 && (unsigned char)start[0] == 0xEF &&
        (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
        start += 3;
    }
    json_t *out = json_parse(start, NULL);
    free(buf);
    return out;
}
