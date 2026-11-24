/*
 * vision_tools_image_prep.c — faithful C11 port of the pure MIME helpers in
 * tools/vision_tools_image_prep.py.
 *
 * PoP: w18_visiontool_49e404_unsupported_inline_image_media_type @ tools/vision_tools_image_prep.py:unsupported_inline_image_media_type
 * PoP: w18_visiontool_49e404_detect_image_mime_type_from_bytes @ tools/vision_tools_image_prep.py:_detect_image_mime_type_from_bytes
 *
 * PNG verification (a soft Pillow dependency) is injected as `png_verify_ok`.
 */
#include "noc_w18_visiontool_49e404.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

static char *vis_dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static int vis_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* strip leading/trailing ASCII whitespace; NUL at new length. */
static char *vis_strip(const char *s) {
    if (!s) return NULL;
    const unsigned char *p = (const unsigned char *)s;
    size_t a = 0, b = strlen(s);
    while (a < b && vis_is_ws(p[a])) ++a;
    while (b > a && vis_is_ws(p[b-1])) --b;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

/* unsupported_inline_image_media_type(url) */
const char *w18_visiontool_49e404_unsupported_inline_image_media_type(const char *url) {
    if (!url) return NULL;
    const char *comma = strchr(url, ',');
    size_t hlen = comma ? (size_t)(comma - url) : strlen(url);
    char *header = (char *)malloc(hlen + 1);
    if (!header) return NULL;
    memcpy(header, url, hlen);
    header[hlen] = '\0';
    for (size_t i = 0; i < hlen; ++i) header[i] = (char)tolower((unsigned char)header[i]);

    const char *prefix = "data:image/";
    const char *result = NULL;
    if (strncmp(header, prefix, strlen(prefix)) == 0) {
        const char *rest = header + strlen(prefix);
        const char *semi = strchr(rest, ';');
        size_t slen = semi ? (size_t)(semi - rest) : strlen(rest);
        char *raw = (char *)malloc(slen + 1);
        if (raw) {
            memcpy(raw, rest, slen); raw[slen] = '\0';
            char *subtype = vis_strip(raw);
            free(raw);
            if (subtype) {
                if (*subtype == '\0') { free(subtype); subtype = vis_dupstr("unknown"); }
                const char *mime = NULL;
                char *built = NULL;
                if (subtype && !strcmp(subtype, "jpg")) {
                    mime = "image/jpeg";
                } else if (subtype) {
                    size_t cap = strlen(subtype) + 16;
                    built = (char *)malloc(cap);
                    if (built) { snprintf(built, cap, "image/%s", subtype); mime = built; }
                }
                if (mime && strcmp(mime, "image/jpeg") && strcmp(mime, "image/png") &&
                    strcmp(mime, "image/gif") && strcmp(mime, "image/webp"))
                    result = vis_dupstr(mime);
                free(built);
                free(subtype);
            }
        }
    }
    free(header);
    return result;
}

static int vis_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static const char *vis_brand_has(const unsigned char *b, size_t n, const char *const *set, size_t nset) {
    for (size_t i = 0; i + 4 <= n; i += 4)
        for (size_t k = 0; k < nset; ++k)
            if (!memcmp(b + i, set[k], 4)) return "YES";
    return NULL;
}

/* _detect_image_mime_type_from_bytes(data) */
const char *w18_visiontool_49e404_detect_image_mime_type_from_bytes(const char *data_hex,
                                                                    int png_verify_ok) {
    if (!data_hex) return NULL;
    size_t hlen = strlen(data_hex);
    size_t nb = hlen / 2;
    unsigned char *data = (unsigned char *)malloc(nb ? nb : 1);
    if (!data) return NULL;
    size_t n = 0;
    for (size_t i = 0; i < nb; ++i) {
        int hi = vis_hexval(data_hex[i*2]), lo = vis_hexval(data_hex[i*2+1]);
        if (hi < 0 || lo < 0) break;
        data[i] = (unsigned char)((hi << 4) | lo);
        ++n;
    }
    size_t hn = n < 64 ? n : 64;          /* header = data[:64] */
    const unsigned char *h = data;
    const char *result = NULL;

    static const unsigned char PNG_SIG[8] = {0x89,'P','N','G','\r','\n',0x1a,'\n'};
    if (hn >= 8 && !memcmp(h, PNG_SIG, 8)) {
        result = png_verify_ok ? "image/png" : NULL;
        goto done;
    }
    if (hn >= 3 && h[0]==0xff && h[1]==0xd8 && h[2]==0xff) { result = "image/jpeg"; goto done; }
    if (hn >= 6 && (!memcmp(h,"GIF87a",6) || !memcmp(h,"GIF89a",6))) { result = "image/gif"; goto done; }
    if (hn >= 2 && h[0]=='B' && h[1]=='M') { result = "image/bmp"; goto done; }
    if (hn >= 12 && !memcmp(h,"RIFF",4) && !memcmp(h+8,"WEBP",4)) { result = "image/webp"; goto done; }
    if (hn >= 12 && !memcmp(h+4,"ftyp",4)) {
        uint32_t box_size = ((uint32_t)h[0]<<24)|((uint32_t)h[1]<<16)|((uint32_t)h[2]<<8)|h[3];
        size_t limit = box_size < 16 ? 16 : (box_size < hn ? (size_t)box_size : hn);
        /* major brand at [8:12]; compatible brands from 16 in 4-byte steps,
         * bounded by `limit` (range(16, limit-3, 4)). */
        unsigned char brands[16][4];
        size_t nbrands = 0;
        memcpy(brands[nbrands++], h + 8, 4);
        for (size_t i = 16; i + 4 <= limit && nbrands < 16; i += 4)
            memcpy(brands[nbrands++], h + i, 4);
        static const char *const av1[] = {"avif","avis","av01"};
        static const char *const hei[] = {"heic","heix","heim","heis","hevc","hevx","mif1","msf1"};
        if (vis_brand_has((const unsigned char *)brands, nbrands*4, av1, 3)) { result = "image/avif"; goto done; }
        if (vis_brand_has((const unsigned char *)brands, nbrands*4, hei, 8)) { result = "image/heic"; goto done; }
    }
done:
    free(data);
    return result ? vis_dupstr(result) : NULL;
}
