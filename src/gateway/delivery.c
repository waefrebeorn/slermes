/*
 * delivery.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include "hermes_system_prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

/* ================================================================
 *  Delivery helpers — error/silence detection
 *  Port of Python gateway/delivery.py.
 * ================================================================ */

/* Check if a string looks like a Telegram private chat ID (positive integer).
 * Port of Python gateway/delivery.py _looks_like_telegram_private_chat_id().
 * AG26: Port of Python gateway/delivery.py:_looks_like_telegram_private_chat_id().
 */
/* PoP: looks_like_telegram_private_chat_id @ gateway/delivery.py:looks_like_telegram_private_chat_id */
bool looks_like_telegram_private_chat_id(const char *chat_id) {
    if (!chat_id || !*chat_id) return false;
    char *end = NULL;
    long val = strtol(chat_id, &end, 10);
    (void)val;
    return (end && *end == '\0' && val > 0);
}


/* Check if a string looks like an integer.
 * Port of Python gateway/delivery.py _looks_like_int().
 * AG26: Port of Python gateway/delivery.py:_looks_like_int().
 */
bool looks_like_int(const char *value) {
    if (!value || !*value) return false;
    char *end = NULL;
    strtol(value, &end, 10);
    return (end && *end == '\0');
}


/* Check if a delivery result indicates failure.
 * Port of Python gateway/delivery.py _send_result_failed().
 * AG26: Port of Python gateway/delivery.py:_send_result_failed().
 */
bool send_result_failed(const char *result_json) {
    if (!result_json) return true;
    char *jerr = NULL;
    json_node_t *root = json_parse(result_json, &jerr);
    free(jerr);
    if (!root) return true;
    bool failed = false;
    json_node_t *success = json_object_get(root, "success");
    if (success && success->type == JSON_BOOL && !success->bool_val) failed = true;
    json_free(root);
    return failed;
}


/* Check if content is a silence-narration token (no actual reply).
 * Port of Python gateway/delivery.py _is_silence_narration().
 * AG26: Port of Python gateway/delivery.py:_is_silence_narration().
 */
bool is_silence_narration(const char *content) {
    if (!content || !*content) return false;
    size_t len = strlen(content);
    if (len > 64) return false; /* length guard */
    /* Strip whitespace/punctuation wrappers */
    const char *s = content;
    while (*s && (isspace((unsigned char)*s) || *s == '*' || *s == '_' || *s == '`' || *s == '~')) s++;
    if (!*s) return true; /* only wrappers */
    /* Check for silence keywords */
    const char *keywords[] = {"silent", "silence", "no response", "no reply", NULL};
    for (int i = 0; keywords[i]; i++) {
        size_t klen = strlen(keywords[i]);
        if (strncasecmp(s, keywords[i], klen) == 0) {
            const char *after = s + klen;
            while (*after && (isspace((unsigned char)*after) || *after == '.' || *after == ')' || *after == '*' || *after == '_' || *after == '`' || *after == '~')) after++;
            if (!*after) return true;
        }
    }
    return false;
}

/* ================================================================
 *  Delivery helpers — send result error handling
 *  Port of Python gateway/delivery.py.
 * ================================================================ */

/* Extract error message from a delivery result.
 * Port of Python gateway/delivery.py _send_result_error().
 * AG26: Port of Python gateway/delivery.py:_send_result_error().
 * result_json is a JSON string from a send operation.
 * Returns malloc'd error string or NULL (caller must free). */
char *send_result_error(const char *result_json) {
    if (!result_json || !*result_json) return NULL;
    char *jerr = NULL;
    json_node_t *root = json_parse(result_json, &jerr);
    free(jerr);
    if (!root) return NULL;
    char *error = NULL;
    json_node_t *err_node = json_object_get(root, "error");
    if (err_node && err_node->type == JSON_STRING) {
        error = strdup(err_node->str_val);
    } else {
        json_node_t *desc = json_object_get(root, "description");
        if (desc && desc->type == JSON_STRING)
            error = strdup(desc->str_val);
    }
    json_free(root);
    return error;
}


/* Check if a delivery error is a Telegram thread-not-found failure.
 * Port of Python gateway/delivery.py _is_thread_not_found_delivery_error().
 * AG26: Port of Python gateway/delivery.py:_is_thread_not_found_delivery_error().
 */
bool is_thread_not_found_delivery_error(const char *result_json) {
    char *error = send_result_error(result_json);
    if (!error) return false;
    bool found = (strstr(error, "thread not found") != NULL) ||
                 (strstr(error, "message thread not found") != NULL) ||
                 (strstr(error, "TOPIC_ID_INVALID") != NULL) ||
                 (strstr(error, "chat not found") != NULL);
    free(error);
    return found;
}

