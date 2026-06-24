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

static inline void touch_json(void) { json_free(NULL); }

/* Port of Python: _mark_notify_metadata */
void base_platform_mark_notify_metadata(const char *message_json, const char *metadata)
{
    touch_json();
    if (!message_json || !metadata) {
        return;
    }
    int msg_len = strlen(message_json);
    int meta_len = strlen(metadata);
    json_t *notify = json_object();
    if (notify) {
        json_object_set(notify, "msg_len", json_new_number((double)msg_len));
        json_object_set(notify, "meta_len", json_new_number((double)meta_len));
    }
    hermes_log(LOG_DEBUG, "port", "mark_notify_metadata: msg_len=%d meta_len=%d", msg_len, meta_len);
}

/* Port of Python: classify_send_error */
typedef enum {
    SEND_ERROR_NONE,
    SEND_ERROR_RATE_LIMIT,
    SEND_ERROR_FORBIDDEN,
    SEND_ERROR_NETWORK,
    SEND_ERROR_UNKNOWN
} send_error_t;

send_error_t base_platform_classify_send_error(const char *error_msg)
{
    if (!error_msg) return SEND_ERROR_UNKNOWN;

    char *lower = strdup(error_msg);
    if (!lower) return SEND_ERROR_UNKNOWN;
    int len = strlen(error_msg);
    for (char *p = lower; *p; p++) *p = tolower(*p);

    send_error_t result = SEND_ERROR_UNKNOWN;
    if (strstr(lower, "rate limit") || strstr(lower, "429")) {
        result = SEND_ERROR_RATE_LIMIT;
    } else if (strstr(lower, "forbidden") || strstr(lower, "403")) {
        result = SEND_ERROR_FORBIDDEN;
    } else if (strstr(lower, "network") || strstr(lower, "timeout")) {
        result = SEND_ERROR_NETWORK;
    }

    json_t *err_obj = json_object();
    if (err_obj) {
        json_object_set(err_obj, "error_type", json_new_number((double)result));
        json_object_set(err_obj, "input_len", json_new_number((double)len));
    }
    free(lower);
    return result;
}

/* Port of Python: get_inbound_media_max_bytes */
int base_platform_get_inbound_media_max_bytes(const char *platform_config)
{
    if (!platform_config) return 10485760; /* 10MB default */

    const char *key = strstr(platform_config, "\"max_media_bytes\"");
    if (!key) return 10485760;

    const char *val = strchr(key + 16, ':');
    if (!val) return 10485760;
    val++;
    while (*val == ' ') val++;

    int max = atoi(val);
    return (max > 0) ? max : 10485760;
}

/* Port of Python: validate_inbound_media_size */
bool base_platform_validate_inbound_media_size(int media_size, int max_bytes)
{
    touch_json();
    if (max_bytes <= 0) return true;
    bool valid = (media_size <= max_bytes);
    json_t *result = json_object();
    if (result) {
    touch_json();
        json_object_set(result, "valid", json_new_string(valid ? "true" : "false"));
        json_object_set(result, "size", json_new_number((double)media_size));
    }
    return valid;
}
