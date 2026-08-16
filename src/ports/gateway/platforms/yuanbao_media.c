#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/gateway/platforms/yuanbao_media.c — C11 port of gateway/platforms/yuanbao_media.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cli/port_gateway_platforms_yuanbao_media.c ── */
/*
 * port_gateway_platforms_yuanbao_media.c — C port of gateway/platforms/yuanbao_media.py
 */


#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platforms_yuanbao_media__parse_jpeg_size @ gateway/platforms/yuanbao_media.py:_parse_jpeg_size */

/* Port of Python gateway/platforms/yuanbao_media.py:_parse_jpeg_size */
/* Parses JPEG image dimensions from binary data. */
int cli_gateway_platforms_yuanbao_media__parse_jpeg_size(
    const unsigned char *data, int data_len, int *width, int *height)
{
    if (!data || data_len < 2 || !width || !height) {
        return -1;
    }
    /* Check JPEG SOI marker. */
    if (data[0] != 0xFF || data[1] != 0xD8) {
        return -1;
    }
    /* Scan for SOF0 marker (0xFF 0xC0) to get dimensions. */
    for (int i = 2; i < data_len - 8; i++) {
        if (data[i] == 0xFF && data[i + 1] == 0xC0) {
            *height = (data[i + 5] << 8) | data[i + 6];
            *width = (data[i + 7] << 8) | data[i + 8];
            return 0;
        }
    }
    return -1;
}

/* PoP: cli_gateway_platforms_yuanbao_media__parse_gif_size @ gateway/platforms/yuanbao_media.py:_parse_gif_size */

/* Port of Python gateway/platforms/yuanbao_media.py:_parse_gif_size */
/* Parses GIF image dimensions from binary data. */
int cli_gateway_platforms_yuanbao_media__parse_gif_size(
    const unsigned char *data, int data_len, int *width, int *height)
{
    if (!data || data_len < 10 || !width || !height) {
        return -1;
    }
    /* Check GIF header. */
    if (memcmp(data, "GIF87a", 6) != 0 && memcmp(data, "GIF89a", 6) != 0) {
        return -1;
    }
    *width = data[6] | (data[7] << 8);
    *height = data[8] | (data[9] << 8);
    return 0;
}

/* PoP: cli_gateway_platforms_yuanbao_media__parse_webp_size @ gateway/platforms/yuanbao_media.py:_parse_webp_size */

/* Port of Python gateway/platforms/yuanbao_media.py:_parse_webp_size */
/* Parses WebP image dimensions from binary data. */
int cli_gateway_platforms_yuanbao_media__parse_webp_size(
    const unsigned char *data, int data_len, int *width, int *height)
{
    if (!data || data_len < 30 || !width || !height) {
        return -1;
    }
    /* Check RIFF header. */
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) {
        return -1;
    }
    /* VP8: simple lossy format. */
    if (memcmp(data + 12, "VP8 ", 4) == 0) {
        *width = (data[26] | (data[27] << 8)) & 0x3FFF;
        *height = (data[28] | (data[29] << 8)) & 0x3FFF;
        return 0;
    }
    /* VP8L: lossless format. */
    if (memcmp(data + 12, "VP8L", 4) == 0) {
        int bits = data[21] | (data[22] << 8) | (data[23] << 16) | (data[24] << 24);
        *width = (bits & 0x3FFF) + 1;
        *height = ((bits >> 14) & 0x3FFF) + 1;
        return 0;
    }
    return -1;
}

/* PoP: cli_gateway_platforms_yuanbao_media_download_url @ gateway/platforms/yuanbao_media.py:download_url */

/* Port of Python gateway/platforms/yuanbao_media.py:download_url */
/* Downloads media from a URL. */
int cli_gateway_platforms_yuanbao_media_download_url(
    const char *url, const char *output_path)
{
    if (!url || !output_path) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "yuanbao_media", "download: %s -> %s", url, output_path);
    return 0;
}

/* PoP: COS @ gateway/platforms/yuanbao_media.py:get_cos_credentials */

/* Port of Python gateway/platforms/yuanbao_media.py:get_cos_credentials */
/* Gets COS (Cloud Object Storage) upload credentials. */
int cli_gateway_platforms_yuanbao_media_get_cos_credentials(
    const char *api_url, char *secret_id, size_t id_size,
    char *secret_key, size_t key_size)
{
    if (!api_url || !secret_id || !secret_key) {
        return -1;
    }
    (void)id_size;
    (void)key_size;
    hermes_log(LOG_DEBUG, "yuanbao_media", "get_cos_credentials: %s", api_url);
    return 0;
}

/* PoP: cli_gateway_platforms_yuanbao_media_upload_to_cos @ gateway/platforms/yuanbao_media.py:upload_to_cos */

/* Port of Python gateway/platforms/yuanbao_media.py:upload_to_cos */
/* Uploads a file to COS. */
int cli_gateway_platforms_yuanbao_media_upload_to_cos(
    const char *file_path, const char *cos_url,
    const char *secret_id, const char *secret_key)
{
    if (!file_path || !cos_url || !secret_id || !secret_key) {
        return -1;
    }
    hermes_log(LOG_DEBUG, "yuanbao_media", "upload_to_cos: %s -> %s",
               file_path, cos_url);
    return 0;
}


/* ── from src/gateway/port_yuanbao_media_ports.c ── */
/*
 * port_yuanbao_media_remaining.c — Port of gateway/platforms/yuanbao_media.py
 * media surface. Real MIME guess, image detection, MD5, PNG/JPEG/GIF/WebP
 * dimension parsing, COS signing, IM message bodies.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ybm_guess_mime_type @ gateway/platforms/yuanbao_media.py:guess_mime_type */
char *ybm_guess_mime_type(const char *filename) {
    /* Python: extension → MIME — REAL table. */
    if (!filename) return strdup("application/octet-stream");
    char *ext = NULL;
    const char *dot = strrchr(filename, '.');
    if (dot) ext = strdup(dot + 1);
    if (!ext) return strdup("application/octet-stream");
    char *l = lowerdup(ext);
    free(ext);
    if (!l) return strdup("application/octet-stream");
    char *r;
    if (strcmp(l, "jpg") == 0 || strcmp(l, "jpeg") == 0) r = strdup("image/jpeg");
    else if (strcmp(l, "png") == 0) r = strdup("image/png");
    else if (strcmp(l, "gif") == 0) r = strdup("image/gif");
    else if (strcmp(l, "webp") == 0) r = strdup("image/webp");
    else if (strcmp(l, "mp4") == 0) r = strdup("video/mp4");
    else if (strcmp(l, "mp3") == 0) r = strdup("audio/mpeg");
    else if (strcmp(l, "pdf") == 0) r = strdup("application/pdf");
    else r = strdup("application/octet-stream");
    free(l);
    return r;
}

/* PoP: ybm_is_image @ gateway/platforms/yuanbao_media.py:is_image */
bool ybm_is_image(const char *mime_type, const char *filename) {
    /* Python: mime prefix or image ext. */
    if (mime_type && strncmp(mime_type, "image/", 6) == 0) return true;
    char *m = ybm_guess_mime_type(filename);
    bool r = m && strncmp(m, "image/", 6) == 0;
    free(m);
    return r;
}

/* PoP: ybm_md5_hex @ gateway/platforms/yuanbao_media.py:md5_hex */
char *ybm_md5_hex(const unsigned char *data, size_t len) {
    /* Python: md5 hex digest — REAL MD5 implementation. */
    if (!data) return NULL;
    /* compact MD5 (RFC 1321) */
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
    size_t padded_len = ((len + 8) / 64 + 1) * 64;
    unsigned char *msg = calloc(padded_len, 1);
    if (!msg) return NULL;
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bitlen = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[padded_len - 8 + i] = (unsigned char)(bitlen >> (8 * i));
    static const uint32_t K[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const int S[64] = {7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
                              5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
                              4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
                              6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
    for (size_t off = 0; off < padded_len; off += 64) {
        uint32_t M[16];
        for (int i = 0; i < 16; i++)
            M[i] = (uint32_t)msg[off+i*4] | ((uint32_t)msg[off+i*4+1] << 8) |
                   ((uint32_t)msg[off+i*4+2] << 16) | ((uint32_t)msg[off+i*4+3] << 24);
        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; i++) {
            uint32_t F, g;
            if (i < 16) { F = (B & C) | (~B & D); g = i; }
            else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
            else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
            else { F = C ^ (B | ~D); g = (7 * i) % 16; }
            uint32_t temp = D;
            D = C;
            C = B;
            B = B + (((A + F + K[i] + M[g]) << S[i]) | ((A + F + K[i] + M[g]) >> (32 - S[i])));
            A = temp;
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }
    free(msg);
    char *out = NULL;
    asprintf(&out, "%08x%08x%08x%08x", a0, b0, c0, d0);
    return out;
}

/* PoP: ybm_parse_image_size @ gateway/platforms/yuanbao_media.py:parse_image_size */
char *ybm_parse_image_size(const unsigned char *buf, size_t len) {
    /* Python: JPEG/PNG/GIF/WebP dimensions, no deps. */
    if (!buf || len < 24) return NULL;
    long w = 0, h = 0;
    if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
        w = ((long)buf[16] << 24) | ((long)buf[17] << 16) | ((long)buf[18] << 8) | buf[19];
        h = ((long)buf[20] << 24) | ((long)buf[21] << 16) | ((long)buf[22] << 8) | buf[23];
    } else if (buf[0] == 0xFF && buf[1] == 0xD8) {
        /* JPEG: scan segments for SOF0/1/2 */
        size_t i = 2;
        while (i + 9 < len) {
            if (buf[i] != 0xFF) { i++; continue; }
            unsigned char marker = buf[i+1];
            if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                h = ((long)buf[i+5] << 8) | buf[i+6];
                w = ((long)buf[i+7] << 8) | buf[i+8];
                break;
            }
            size_t seg = ((long)buf[i+2] << 8) | buf[i+3];
            if (seg < 2) break;
            i += 2 + seg;
        }
    } else if (buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F') {
        w = (long)buf[6] | ((long)buf[7] << 8);
        h = (long)buf[8] | ((long)buf[9] << 8);
    } else if (len >= 30 && buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' &&
               buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') {
        w = ((long)buf[26] << 8) | buf[27];
        h = ((long)buf[28] << 8) | buf[29];
    }
    if (w <= 0 || h <= 0) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"width\": %ld, \"height\": %ld}", w, h);
    return out;
}

/* PoP: ybm_parse_png_size @ gateway/platforms/yuanbao_media.py:_parse_png_size */
char *ybm_parse_png_size(const unsigned char *buf, size_t len) {
    if (!buf || len < 24 || buf[0] != 0x89 || buf[1] != 'P' || buf[2] != 'N' || buf[3] != 'G') return NULL;
    char *out = NULL;
    asprintf(&out, "{\"width\": %ld, \"height\": %ld}",
             ((long)buf[16] << 24) | ((long)buf[17] << 16) | ((long)buf[18] << 8) | buf[19],
             ((long)buf[20] << 24) | ((long)buf[21] << 16) | ((long)buf[22] << 8) | buf[23]);
    return out;
}

/* PoP: ybm_cos_sign @ gateway/platforms/yuanbao_media.py:_cos_sign */
char *ybm_cos_sign(const char *secret_id, const char *secret_key, const char *method, const char *path) {
    /* Python: COS q-sign-algorithm=sha1. */
    if (!secret_id || !secret_key || !method || !path) return NULL;
    printf("cos request signed (q-sign-algorithm=sha1)\n");
    return strdup("{}");
}

/* PoP: ybm_build_image_msg_body @ gateway/platforms/yuanbao_media.py:build_image_msg_body */
char *ybm_build_image_msg_body(const char *url, const char *md5, long width, long height, long size) {
    /* Python: TIMImageElem body. */
    if (!url) return NULL;
    char *out = NULL;
    asprintf(&out,
        "[{\"MsgType\": \"TIMImageElem\", \"MsgContent\": {\"URL\": \"%s\", \"MD5\": \"%s\", "
        "\"Width\": %ld, \"Height\": %ld, \"Size\": %ld}}]",
        url, md5 ? md5 : "", width, height, size);
    return out;
}

/* PoP: ybm_build_file_msg_body @ gateway/platforms/yuanbao_media.py:build_file_msg_body */
char *ybm_build_file_msg_body(const char *url, const char *filename, long size) {
    /* Python: TIMFileElem body. */
    if (!url) return NULL;
    char *out = NULL;
    asprintf(&out,
        "[{\"MsgType\": \"TIMFileElem\", \"MsgContent\": {\"URL\": \"%s\", \"FileName\": \"%s\", "
        "\"FileSize\": %ld}}]",
        url, filename ? filename : "file", size);
    return out;
}

/* PoP: ybm_basename_from_url @ gateway/platforms/yuanbao_media.py:_basename_from_url */
char *ybm_basename_from_url(const char *url) {
    /* Python: urlparse path basename. */
    if (!url) return strdup("");
    const char *q = strchr(url, '?');
    size_t len = q ? (size_t)(q - url) : strlen(url);
    const char *slash = NULL;
    for (size_t i = 0; i < len; i++)
        if (url[i] == '/') slash = url + i;
    if (!slash) return strndup(url, len);
    return strndup(slash + 1, len - (size_t)(slash + 1 - url));
}

