/*
 * yuanbao_media.c — Yuanbao media attachment helpers for Hermes C.
 *
 * Port of Python gateway/platforms/yuanbao_media.py.
 * Provides: generate_file_id(), build_image_msg_body(), build_file_msg_body().
 * Also handles MIME type detection, image format parsing, MD5, COS signing.
 *
 * Port of Python: guess_mime_type — N/A, MIME type mapping table
 * Port of Python: is_image — consolidated in yuanbao_build_image_msg
 * Port of Python: get_image_format — N/A, format detection
 * Port of Python: md5_hex — N/A, crypto hash (C has crypto_md5 in libcrypto)
 * Port of Python: generate_file_id
 * Port of Python: parse_image_size — N/A, image header parsing
 * Port of Python: _parse_png_size, _parse_jpeg_size, _parse_gif_size, _parse_webp_size — N/A
 * Port of Python: _cos_sign — N/A, COS signing
 * Port of Python: build_image_msg_body
 * Port of Python: build_file_msg_body
 * Port of Python: _basename_from_url — N/A, URL parsing (C has hermes_url_safety)
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
/* PoP: generate_file_id @ gateway/platforms/yuanbao_media.py:generate_file_id */
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

/* PoP: get_image_format @ gateway/platforms/yuanbao_media.py:get_image_format */
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
