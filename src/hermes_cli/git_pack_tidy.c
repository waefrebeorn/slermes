/*
 * git_pack_tidy.c — C port of upstream Hermes hermes_cli/git_pack_tidy.py.
 *
 * PoP: w10_gitpackti_d631c8_is_redundant @ hermes_cli/git_pack_tidy.py:_is_redundant
 * PoP: w10_gitpackti_d631c8_index_contains @ hermes_cli/git_pack_tidy.py:_Index.__contains__
 *
 * Filesystem, mmap and git subprocess seams are replaced by explicit fixture
 * inputs (oid lists and a v2 index byte blob); the membership algorithms are
 * ported verbatim, including the "last hit first" search order.
 */
#include "noc_w10_gitpackti_d631c8.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IDX_V2_0 0xff
#define IDX_V2_1 0x74
#define IDX_V2_2 0x4f
#define IDX_V2_3 0x63
#define IDX_V2_4 0x00
#define IDX_V2_5 0x00
#define IDX_V2_6 0x00
#define IDX_V2_7 0x02
#define FANOUT 8
#define NAMES (FANOUT + 256 * 4)

static int in_set(const json_t *arr, const char *oid) {
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val, oid)) return 1;
    }
    return 0;
}

int w10_gitpackti_d631c8_is_redundant(const struct json_t *oids, const struct json_t *others) {
    if (!json_is_array(others) || json_len(others) == 0) return 0;
    size_t hit = 0;
    for (size_t n = 0; n < (json_is_array(oids) ? json_len(oids) : 0); n++) {
        const json_t *ov = json_get(oids, n);
        const char *oid = (ov && ov->type == JSON_STRING) ? ov->str_val : "";
        if (in_set(json_get(others, hit), oid)) continue;   /* others[hit] first */
        int found = 0;
        for (size_t j = 0; j < json_len(others); j++) {
            if (j != hit && in_set(json_get(others, j), oid)) { hit = j; found = 1; break; }
        }
        if (!found) return 0;
    }
    return 1;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
/* Decode a hex string into a malloc'd byte buffer; sets *n. Returns NULL on odd/invalid hex. */
static unsigned char *hexdecode(const char *hex, size_t *n) {
    if (!hex) return NULL;
    size_t l = strlen(hex);
    if (l % 2) return NULL;
    *n = l / 2;
    unsigned char *b = (unsigned char *)malloc(*n ? *n : 1);
    for (size_t i = 0; i < *n; i++) {
        int hi = hexval(hex[2 * i]), lo = hexval(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) { free(b); return NULL; }
        b[i] = (unsigned char)((hi << 4) | lo);
    }
    return b;
}
static unsigned int be32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
           ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

int w10_gitpackti_d631c8_index_contains(const char *idx_hex, const char *oid_hex) {
    size_t n = 0, on = 0;
    unsigned char *map = hexdecode(idx_hex, &n);
    unsigned char *oid = hexdecode(oid_hex, &on);
    int result = 0;
    if (!map || !oid) goto done;
    if (n < NAMES || on != 20) goto done;                 /* header + fanout + at least one slot */
    const unsigned char *magic = map;
    if (magic[0] != IDX_V2_0 || magic[1] != IDX_V2_1 || magic[2] != IDX_V2_2 ||
        magic[3] != IDX_V2_3 || magic[4] != IDX_V2_4 || magic[5] != IDX_V2_5 ||
        magic[6] != IDX_V2_6 || magic[7] != IDX_V2_7) goto done;
    unsigned int count = be32(map + FANOUT + 255 * 4);
    if ((size_t)(NAMES + (size_t)count * 20) > n) goto done;
    unsigned int lo = oid[0] ? be32(map + FANOUT + ((size_t)oid[0] - 1) * 4) : 0;
    unsigned int hi = be32(map + FANOUT + (size_t)oid[0] * 4);
    while (lo < hi) {
        unsigned int mid = lo + (hi - lo) / 2;
        const unsigned char *name = map + NAMES + (size_t)mid * 20;
        int cmp = memcmp(name, oid, 20);
        if (cmp == 0) { result = 1; goto done; }
        if (cmp < 0) lo = mid + 1;
        else hi = mid;
    }
done:
    free(map); free(oid);
    return result;
}
