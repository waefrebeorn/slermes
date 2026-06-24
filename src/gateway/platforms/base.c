/*
 * gateway/platforms/base.c — Base platform functionality.
 *
 * Port of Python gateway/platforms/base.py.
 *
 * Provides common platform utilities: UTF-16 length, proxy handling,
 * media caching, message formatting, and platform vtable helpers.
 */

#include "hermes_gateway.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

/* ================================================================
 *  UTF-16 helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: utf16_len */
/* PoP: cli_gateway_platforms_base_utf16_len @ gateway/platforms/base.py:utf16_len */
/* PoP: cli_gateway_relay_adapter__utf16_len @ gateway/relay/adapter.py:_utf16_len */
size_t utf16_len(const char *s) {
    if (!s) return 0;

    size_t len = 0;
    const unsigned char *p = (const unsigned char *)s;

    while (*p) {
        if (*p < 0x80) {
            /* ASCII: 1 UTF-16 code unit */
            len++;
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            /* 2-byte UTF-8: 1 UTF-16 code unit (BMP) */
            len++;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            /* 3-byte UTF-8: 1 UTF-16 code unit (BMP) or 2 (supplementary) */
            /* Check if it's a supplementary character (U+10000+) */
            uint32_t cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            len += (cp >= 0x10000) ? 2 : 1;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            /* 4-byte UTF-8: always 2 UTF-16 code units (surrogate pair) */
            len += 2;
            p += 4;
        } else {
            /* Invalid - count as 1 */
            len++;
            p++;
        }
    }

    return len;
}

/* Port of Python: _prefix_within_utf16_limit */
char *gw_prefix_within_utf16_limit(const char *s, size_t limit) {
    if (!s || limit == 0) return strdup("");

    size_t len = 0;
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *start = p;

    while (*p) {
        size_t units = 0;
        uint32_t cp = 0;

        if (*p < 0x80) {
            cp = *p;
            units = 1;
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            units = 1;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            units = (cp >= 0x10000) ? 2 : 1;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                 ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            units = 2;
            p += 4;
        } else {
            units = 1;
            p++;
        }

        if (len + units > limit) {
            break;
        }
        len += units;
    }

    size_t out_len = p - start;
    char *result = malloc(out_len + 1);
    if (!result) return NULL;

    memcpy(result, start, out_len);
    result[out_len] = '\0';
    return result;
}

/* Port of Python: _custom_unit_to_cp */
int custom_unit_to_cp(const char *s, int len, int budget,
                       int (*len_fn)(const char *, int)) {
    if (!s || len <= 0 || budget <= 0) return 0;

    /* Binary search for the largest prefix within budget */
    int low = 0, high = len, result = 0;

    while (low <= high) {
        int mid = (low + high) / 2;
        int units = len_fn(s, mid);

        if (units <= budget) {
            result = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return result;
}

/* ================================================================
 *  Float/env helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: _float_env */
double float_env(const char *name, double default_value) {
    if (!name) return default_value;

    const char *val = getenv(name);
    if (!val || !*val) return default_value;

    char *endptr;
    double result = strtod(val, &endptr);
    if (endptr == val || *endptr != '\0') {
        return default_value;
    }
    return result;
}

/* ================================================================
 *  Media cache helpers (Port of Python gateway/platforms/base.py)
 * - Many functions delegate to media_cache.c
 * ================================================================ */

/* Port of Python: _looks_like_image */
bool looks_like_image(const char *url) {
    if (!url) return false;

    /* Check file extension */
    const char *ext = strrchr(url, '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
            strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
            strcasecmp(ext, "webp") == 0 || strcasecmp(ext, "bmp") == 0 ||
            strcasecmp(ext, "tiff") == 0 || strcasecmp(ext, "svg") == 0 ||
            strcasecmp(ext, "avif") == 0) {
            return true;
        }
    }

    /* Check MIME type hints in URL */
    if (strstr(url, "image/") ||
        strstr(url, "img") ||
        strstr(url, "photo")) {
        return true;
    }

    return false;
}

/* ================================================================
 *  Session/source helpers
 * ================================================================ */

/* PoP: _build_source @ src/gateway/platforms/base.c:gw_build_source
 * Port of Python yuanbao.py:_build_source(). */
json_node_t *gw_build_source(const char *platform, const char *chat_id,
                              const char *chat_name, const char *chat_type,
                              const char *user_id, const char *user_name,
                              const char *thread_id) {
    json_node_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set(obj, "platform", json_string(platform));
    json_object_set(obj, "chat_id", json_string(chat_id));
    if (chat_name) json_object_set(obj, "chat_name", json_string(chat_name));
    if (chat_type) json_object_set(obj, "chat_type", json_string(chat_type));
    if (user_id) json_object_set(obj, "user_id", json_string(user_id));
    if (user_name) json_object_set(obj, "user_name", json_string(user_name));
    if (thread_id) json_object_set(obj, "thread_id", json_string(thread_id));

    return obj;
}

/* ================================================================
 *  Message formatting helpers
 * ================================================================ */

/* Port of Python: format_message */
char *gw_format_message(const char *text, bool markdown) {
    if (!text) return strdup("");

    /* For now, just return a copy. Full markdown processing
     * is done in platform-specific code. */
    return strdup(text);
}

/* ================================================================ */
