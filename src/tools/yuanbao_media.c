/*
 * yuanbao_media.c — Yuanbao media attachment helpers for Hermes C.
 *
 * Port of Python gateway/platforms/yuanbao_media.py.
 * Provides: generate_file_id(), build_image_msg_body(), build_file_msg_body().
 * Also handles MIME type detection, image format parsing, MD5, COS signing.
 *
 * Port of Python: guess_mime_type — extension→MIME table (implemented)
 * Port of Python: is_image — consolidated in yuanbao_build_image_msg
 * Port of Python: get_image_format — implemented as yuanbao_get_image_format
 * Port of Python: md5_hex — crypto_md5_hex (libcrypto)
 * Port of Python: generate_file_id
 * Port of Python: parse_image_size — PNG/JPEG/GIF/WebP header parser (implemented)
 * Port of Python: _parse_png_size, _parse_jpeg_size, _parse_gif_size, _parse_webp_size — implemented (static)
 * Port of Python: build_image_msg_body
 * Port of Python: build_file_msg_body
 * Port of Python: _basename_from_url — uses url_extract_basename
 *
 * MIT License — WuBu Slermes Project
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_crypto.h"
#include "hermes_yuanbao_media.h"
#include "hermes_url_safety.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

/* ================================================================
 *  generate_file_id — 32 hex chars from 16 random bytes
 *  Port of Python yuanbao_media.generate_file_id().
 * ================================================================ */
/* PoP: yuanbao_generate_file_id @ gateway/platforms/yuanbao_media.py:generate_file_id */
char *yuanbao_generate_file_id(void)
{
    unsigned char buf[16];
    if (!crypto_random_bytes(buf, 16))
        return NULL;
    return crypto_hex_encode(buf, 16);
}

/* ================================================================
 *  build_image_msg — TIMImageElem JSON string builder
 *  Port of Python yuanbao_media.build_image_msg_body().
 * ================================================================ */

char *yuanbao_build_image_msg(const char *url,
                              const char *uuid,
                              const char *filename,
                              int size,
                              int width,
                              int height,
                              const char *mime_type)
{
    if (!url) return NULL;

    /* Determine uuid: explicit uuid > filename > url basename > "image" */
    const char *resolved_uuid = uuid;
    if (!resolved_uuid || !resolved_uuid[0])
        resolved_uuid = filename;
    if (!resolved_uuid || !resolved_uuid[0]) {
        char *basename = url_extract_basename(url);
        resolved_uuid = basename;
        if (!resolved_uuid || !resolved_uuid[0])
            resolved_uuid = "image";
    }

    /* Determine image_format from mime_type */
    int image_format = 255; /* default: unknown */
    if (mime_type && mime_type[0])
        image_format = (int)url_get_image_format(mime_type);

    /* Build JSON */
    json_t *info = json_object();
    json_set(info, "type", json_number(1));
    json_set(info, "size", json_number(size));
    json_set(info, "width", json_number(width));
    json_set(info, "height", json_number(height));
    json_set(info, "url", json_string(url));

    json_t *info_arr = json_array();
    json_append(info_arr, info);

    json_t *content = json_object();
    json_set(content, "uuid", json_string(resolved_uuid));
    json_set(content, "image_format", json_number(image_format));
    json_set(content, "image_info_array", info_arr);

    json_t *msg = json_object();
    json_set(msg, "msg_type", json_string("TIMImageElem"));
    json_set(msg, "msg_content", content);

    json_t *root = json_array();
    json_append(root, msg);

    char *result = json_serialize(root);
    json_free(root);
    return result;
}

/* ================================================================
 *  build_file_msg — TIMFileElem JSON string builder
 *  Port of Python yuanbao_media.build_file_msg_body().
 * ================================================================ */

char *yuanbao_build_file_msg(const char *url,
                             const char *filename,
                             const char *uuid,
                             int size)
{
    if (!url || !filename) return NULL;

    /* uuid falls back to filename */
    const char *resolved_uuid = uuid;
    if (!resolved_uuid || !resolved_uuid[0])
        resolved_uuid = filename;

    json_t *content = json_object();
    json_set(content, "uuid", json_string(resolved_uuid));
    json_set(content, "file_name", json_string(filename));
    json_set(content, "file_size", json_number(size));
    json_set(content, "url", json_string(url));

    json_t *msg = json_object();
    json_set(msg, "msg_type", json_string("TIMFileElem"));
    json_set(msg, "msg_content", content);

    json_t *root = json_array();
    json_append(root, msg);

    char *result = json_serialize(root);
    json_free(root);
    return result;
}

/* PoP: yuanbao_get_image_format @ gateway/platforms/yuanbao_media.py:get_image_format */
/* TIM image format number from mime type; 255 = unknown. */
long yuanbao_get_image_format(const char *mime_type)
{
    if (!mime_type) return 255;
    char low[64];
    snprintf(low, sizeof(low), "%s", mime_type);
    for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
    if (strstr(low, "png")) return 1;
    if (strstr(low, "jpeg") || strstr(low, "jpg")) return 2;
    if (strstr(low, "gif")) return 3;
    if (strstr(low, "bmp")) return 4;
    if (strstr(low, "webp")) return 5;
    return 255;
}

/* =================================================================
 *  guess_mime_type — extension → MIME, faithful to _EXT_TO_MIME
 *  Port of Python yuanbao_media.guess_mime_type().
 * ================================================================= */

typedef struct {
    const char *ext;
    const char *mime;
} yuanbao_ext_mime_t;

static const yuanbao_ext_mime_t YUANBAO_EXT_TO_MIME[] = {
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".png",  "image/png"},
    {".gif",  "image/gif"},
    {".webp", "image/webp"},
    {".bmp",  "image/bmp"},
    {".heic", "image/heic"},
    {".tiff", "image/tiff"},
    {".ico",  "image/x-icon"},
    {".pdf",  "application/pdf"},
    {".doc",  "application/msword"},
    {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {".xls",  "application/vnd.ms-excel"},
    {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {".ppt",  "application/vnd.ms-powerpoint"},
    {".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {".txt",  "text/plain"},
    {".zip",  "application/zip"},
    {".tar",  "application/x-tar"},
    {".gz",   "application/gzip"},
    {".mp3",  "audio/mpeg"},
    {".mp4",  "video/mp4"},
    {".wav",  "audio/wav"},
    {".ogg",  "audio/ogg"},
    {".webm", "video/webm"},
};

/* PoP: yuanbao_guess_mime_type @ gateway/platforms/yuanbao_media.py:guess_mime_type */
const char *yuanbao_guess_mime_type(const char *filename)
{
    static const char *FALLBACK = "application/octet-stream";
    if (!filename || !filename[0]) return FALLBACK;
    /* splitext -> lowercase extension */
    const char *dot = strrchr(filename, '.');
    if (!dot) return FALLBACK;
    char ext[32];
    size_t n = 0;
    for (const char *p = dot; *p && n + 1 < sizeof(ext); p++)
        ext[n++] = (char)tolower((unsigned char)*p);
    ext[n] = '\0';
    for (size_t i = 0; i < sizeof(YUANBAO_EXT_TO_MIME) / sizeof(YUANBAO_EXT_TO_MIME[0]); i++)
        if (strcmp(YUANBAO_EXT_TO_MIME[i].ext, ext) == 0)
            return YUANBAO_EXT_TO_MIME[i].mime;
    return FALLBACK;
}

/* PoP: yuanbao_md5_hex @ gateway/platforms/yuanbao_media.py:md5_hex */
char *yuanbao_md5_hex(const unsigned char *data, size_t len)
{
    return crypto_md5_hex(data, len);
}

/* =================================================================
 *  parse_image_size — PNG/JPEG/GIF/WebP header width/height
 *  Port of Python yuanbao_media.parse_image_size() + _parse_*_size().
 * ================================================================= */

static bool yuanbao_parse_png(const unsigned char *b, size_t n, int *w, int *h)
{
    if (n < 24 || b[0] != 0x89 || b[1] != 'P' || b[2] != 'N' || b[3] != 'G')
        return false;
    *w = (b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19];
    *h = (b[20] << 24) | (b[21] << 16) | (b[22] << 8) | b[23];
    return true;
}

static bool yuanbao_parse_gif(const unsigned char *b, size_t n, int *w, int *h)
{
    if (n < 10) return false;
    if (!((b[0] == 'G' && b[1] == 'I' && b[2] == 'F' && b[3] == '8' &&
           (b[4] == '7' || b[4] == '9') && b[5] == 'a')))
        return false;
    *w = b[6] | (b[7] << 8);
    *h = b[8] | (b[9] << 8);
    return true;
}

static bool yuanbao_parse_jpeg(const unsigned char *b, size_t n, int *w, int *h)
{
    if (n < 4 || b[0] != 0xFF || b[1] != 0xD8) return false;
    size_t i = 2;
    while (i + 9 <= n) {
        if (b[i] != 0xFF) { i++; continue; }
        unsigned char marker = b[i + 1];
        if (marker == 0xC0 || marker == 0xC2) {
            *h = (b[i + 5] << 8) | b[i + 6];
            *w = (b[i + 7] << 8) | b[i + 8];
            return true;
        }
        if (i + 4 <= n) {
            size_t seg = (b[i + 2] << 8) | b[i + 3];
            i += 2 + seg;
        } else break;
    }
    return false;
}

static bool yuanbao_parse_webp(const unsigned char *b, size_t n, int *w, int *h)
{
    if (n < 16 || memcmp(b, "RIFF", 4) != 0 || memcmp(b + 8, "WEBP", 4) != 0)
        return false;
    const unsigned char *chunk = b + 12;
    if (memcmp(chunk, "VP8 ", 4) == 0) {
        if (n >= 30 && b[23] == 0x9D && b[24] == 0x01 && b[25] == 0x2A) {
            *w = ((b[26] | (b[27] << 8)) & 0x3FFF);
            *h = ((b[28] | (b[29] << 8)) & 0x3FFF);
            return true;
        }
    } else if (memcmp(chunk, "VP8L", 4) == 0) {
        if (n >= 25 && b[20] == 0x2F) {
            unsigned int bits = (b[21] | (b[22] << 8) | (b[23] << 16) | (b[24] << 24));
            *w = (int)(bits & 0x3FFF) + 1;
            *h = (int)((bits >> 14) & 0x3FFF) + 1;
            return true;
        }
    } else if (memcmp(chunk, "VP8X", 4) == 0) {
        if (n >= 30) {
            *w = (b[24] | (b[25] << 8) | (b[26] << 16)) + 1;
            *h = (b[27] | (b[28] << 8) | (b[29] << 16)) + 1;
            return true;
        }
    }
    return false;
}

/* PoP: yuanbao_parse_image_size @ gateway/platforms/yuanbao_media.py:parse_image_size */
bool yuanbao_parse_image_size(const unsigned char *data, size_t len,
                              int *width, int *height)
{
    if (!data || !width || !height) return false;
    if (yuanbao_parse_png(data, len, width, height)) return true;
    if (yuanbao_parse_jpeg(data, len, width, height)) return true;
    if (yuanbao_parse_gif(data, len, width, height)) return true;
    if (yuanbao_parse_webp(data, len, width, height)) return true;
    return false;
}
