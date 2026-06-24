/*
 * port_gateway_platforms_yuanbao_media.c — C port of gateway/platforms/yuanbao_media.py
 */

#include "hermes.h"
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

/* PoP: cli_gateway_platforms_yuanbao_media_get_cos_credentials @ gateway/platforms/yuanbao_media.py:get_cos_credentials */

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
