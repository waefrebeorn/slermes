/**
 * port_gateway_platforms_base.c — Port of Python gateway/platforms/base.py
 *
 * Real C implementations for gateway platform base functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"

/* Port of Python: _mark_notify_metadata
 * Clones metadata and adds "notify": true marker.
 * Input metadata_json is a JSON string; output is a newly allocated JSON string
 * that must be freed by caller, or NULL on error. */
char *base_platform_mark_notify_metadata(const char *metadata_json)
{
    if (!metadata_json || !*metadata_json) {
        /* Empty or null input -> return {"notify": true} */
        json_t *notify = json_object();
        if (!notify) return NULL;
        json_object_set(notify, "notify", json_bool(true));
        char *result = json_serialize(notify);
        json_free(notify);
        return result;
    }

    /* Parse input metadata JSON */
    json_t *meta = json_parse(metadata_json, NULL);
    if (!meta) {
        /* Invalid JSON -> treat as empty object */
        json_t *notify = json_object();
        if (!notify) return NULL;
        json_object_set(notify, "notify", json_bool(true));
        char *result = json_serialize(notify);
        json_free(notify);
        return result;
    }

    /* Clone and add notify flag */
    json_t *notify = json_copy(meta);
    json_free(meta);
    if (!notify) return NULL;

    json_object_set(notify, "notify", json_bool(true));
    char *result = json_serialize(notify);
    json_free(notify);
    return result;
}

/* Port of Python: classify_send_error
 * Maps error message to error kind enum.
 * Returns error kind enum (matches Python SEND_ERROR_KINDS string values mapped to enum). */
typedef enum {
    SEND_ERROR_NONE,
    SEND_ERROR_RATE_LIMIT,
    SEND_ERROR_FORBIDDEN,
    SEND_ERROR_NETWORK,
    SEND_ERROR_TOO_LONG,
    SEND_ERROR_BAD_FORMAT,
    SEND_ERROR_NOT_FOUND,
    SEND_ERROR_TRANSIENT,
    SEND_ERROR_UNKNOWN
} send_error_t;

/* Helper: check if substring exists in haystack (case-insensitive) */
static bool str_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return false;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

send_error_t base_platform_classify_send_error(const char *error_msg)
{
    if (!error_msg) return SEND_ERROR_UNKNOWN;

    /* Check error patterns in order matching Python implementation */
    if (str_contains_ci(error_msg, "message_too_long") ||
        str_contains_ci(error_msg, "too long") ||
        str_contains_ci(error_msg, "message is too long")) {
        return SEND_ERROR_TOO_LONG;
    }
    if (str_contains_ci(error_msg, "can't parse entities") ||
        str_contains_ci(error_msg, "cant parse entities") ||
        str_contains_ci(error_msg, "can't find end") ||
        str_contains_ci(error_msg, "unsupported start tag") ||
        (str_contains_ci(error_msg, "entity") && str_contains_ci(error_msg, "parse")) ||
        (str_contains_ci(error_msg, "bad request") && str_contains_ci(error_msg, "entit"))) {
        return SEND_ERROR_BAD_FORMAT;
    }
    if (str_contains_ci(error_msg, "forbidden") ||
        str_contains_ci(error_msg, "bot was blocked") ||
        str_contains_ci(error_msg, "blocked by the user") ||
        str_contains_ci(error_msg, "user is deactivated") ||
        str_contains_ci(error_msg, "not enough rights") ||
        str_contains_ci(error_msg, "have no rights") ||
        str_contains_ci(error_msg, "not a member")) {
        return SEND_ERROR_FORBIDDEN;
    }
    if (str_contains_ci(error_msg, "flood") ||
        str_contains_ci(error_msg, "too many requests") ||
        str_contains_ci(error_msg, "retry after") ||
        str_contains_ci(error_msg, "rate limit") ||
        str_contains_ci(error_msg, "429")) {
        return SEND_ERROR_RATE_LIMIT;
    }
    if (str_contains_ci(error_msg, "chat not found") ||
        str_contains_ci(error_msg, "user not found") ||
        str_contains_ci(error_msg, "channel not found") ||
        str_contains_ci(error_msg, "group not found") ||
        str_contains_ci(error_msg, "peer not found") ||
        str_contains_ci(error_msg, "message not found") ||
        str_contains_ci(error_msg, "topic not found") ||
        str_contains_ci(error_msg, "thread not found")) {
        return SEND_ERROR_NOT_FOUND;
    }
    if (str_contains_ci(error_msg, "network") ||
        str_contains_ci(error_msg, "timeout") ||
        str_contains_ci(error_msg, "connecttimeout") ||
        str_contains_ci(error_msg, "connection") ||
        str_contains_ci(error_msg, "dns") ||
        str_contains_ci(error_msg, "temporary failure") ||
        str_contains_ci(error_msg, "unreachable")) {
        return SEND_ERROR_TRANSIENT;
    }
    return SEND_ERROR_UNKNOWN;
}

/* Port of Python: get_inbound_media_max_bytes
 * Returns max inbound media bytes from platform config JSON string.
 * Returns default 128MB if config is missing or invalid. */
int base_platform_get_inbound_media_max_bytes(const char *platform_config)
{
    const int DEFAULT_MAX_BYTES = 128 * 1024 * 1024; /* 128MB default */
    if (!platform_config) return DEFAULT_MAX_BYTES;

    const char *key = strstr(platform_config, "\"max_inbound_media_bytes\"");
    if (!key) return DEFAULT_MAX_BYTES;

    const char *val = strchr(key + 24, ':');
    if (!val) return DEFAULT_MAX_BYTES;
    val++;
    while (*val == ' ') val++;

    long max = strtol(val, NULL, 10);
    return (max > 0) ? (int)max : DEFAULT_MAX_BYTES;
}

/* Port of Python: validate_inbound_media_size
 * Returns true if media_size <= max_bytes (or if max_bytes <= 0 disables check).
 * Returns false if media_size exceeds max_bytes. */
bool base_platform_validate_inbound_media_size(int media_size, int max_bytes)
{
    if (max_bytes <= 0) return true; /* Check disabled */
    return (media_size <= max_bytes);
}
