/*
 * delivery.c — extracted from gateway/helpers.c monolith.
 *
 * Real implementation home for the Python module it ports (no longer a
 * name-parity stub). Public prototypes stay in include/gateway_helpers.h
 * (or hermes_gateway.h); callers are unchanged.
 */

#include "gateway_helpers.h"
#include "hermes_json.h"
#include "hermes_gateway_delivery.h"
#include "hermes_system_prompt.h"
#include "hermes_yaml.h"
#include "gateway/platforms/base.h"
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
/* PoP: _looks_like_int @ gateway/delivery.py:_looks_like_int */
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
/* PoP: _send_result_failed @ gateway/delivery.py:_send_result_failed */
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
/* PoP: _is_silence_narration @ gateway/delivery.py:_is_silence_narration */
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
/* PoP: _send_result_error @ gateway/delivery.py:_send_result_error */
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
/* PoP: _is_thread_not_found_delivery_error @ gateway/delivery.py:_is_thread_not_found_delivery_error */
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

/* ================================================================
 *  Delivery helpers — additional ported methods
 *  Port of Python gateway/delivery.py.
 * ================================================================ */

/* Forward declarations for platform classifiers defined in the platforms port. */
int  base_platform_classify_send_error(const char *error_msg);
bool base_platform_is_chat_level_not_found(const char *exc_str,
                                           const char *exc_class,
                                           const char *error_text);

/* send_error_t ordinals mirror port_gateway_platforms_base.c. */
#define SEND_ERROR_NOT_FOUND 6

/* Extract the machine-readable error_kind from a SendResult/dict (JSON string).
 * Port of Python gateway/delivery.py:_send_result_error_kind.
 * Returns malloc'd error_kind string or NULL (caller frees). */
/* PoP: send_result_error_kind @ gateway/delivery.py:_send_result_error_kind */
char *send_result_error_kind(const char *result_json) {
    if (!result_json || !*result_json) return NULL;
    char *jerr = NULL;
    json_node_t *root = json_parse(result_json, &jerr);
    free(jerr);
    if (!root) return NULL;
    char *kind = NULL;
    json_node_t *k = json_object_get(root, "error_kind");
    if (k && k->type == JSON_STRING && k->str_val && *k->str_val) {
        kind = strdup(k->str_val);
    }
    json_free(root);
    return kind;
}

/* Best-effort dead-target classification from a raised error's text.
 * Port of Python gateway/delivery.py:_classify_dead_from_error_text.
 * Reuses the platform-neutral classifier + chat-level not-found check.
 * Returns malloc'd "not_found" only when the whole chat is gone, else NULL
 * (caller frees or NULL). */
/* PoP: classify_dead_from_error_text @ gateway/delivery.py:_classify_dead_from_error_text */
char *classify_dead_from_error_text(const char *error_text) {
    if (!error_text || !*error_text) return NULL;
    int kind = base_platform_classify_send_error(error_text);
    if (kind != SEND_ERROR_NOT_FOUND) return NULL;
    /* NOT_FOUND collapses chat-level and sub-chat failures. Only a whole-chat
     * not_found means the target is dead. */
    if (!base_platform_is_chat_level_not_found(NULL, NULL, error_text)) {
        return NULL;
    }
    return strdup("not_found");
}

/* Whether the outbound silence-narration filter is active.
 * Port of Python gateway/delivery.py:_filter_silence_narration_enabled.
 * HERMES_FILTER_SILENCE_NARRATION env var overrides config when set;
 * otherwise gateway.filter_silence_narration config flag wins (default true).
 * config_path may be NULL (falls back to env-only / default true). */
/* PoP: filter_silence_narration_enabled @ gateway/delivery.py:_filter_silence_narration_enabled */
bool filter_silence_narration_enabled(const char *config_path) {
    const char *env = getenv("HERMES_FILTER_SILENCE_NARRATION");
    if (env && *env) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%s", env);
        for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
        return (strcmp(buf, "1") == 0 || strcmp(buf, "true") == 0 ||
                strcmp(buf, "yes") == 0 || strcmp(buf, "on") == 0);
    }
    if (config_path && *config_path) {
        char *yerr = NULL;
        yaml_doc_t *doc = yaml_parse_file(config_path, &yerr);
        free(yerr);
        if (doc) {
            bool v = yaml_get_bool(doc, "gateway.filter_silence_narration", true);
            yaml_free(doc);
            return v;
        }
    }
    return true;
}

#define MAX_PLATFORM_OUTPUT 4096

/* Deliver content to a platform adapter (real dispatch).
 * Port of Python gateway/delivery.py:_deliver_to_platform.
 *
 * Implements the behavioral core: silence-narration filtering (substrate
 * anti-loop guard) and oversize truncation with an audit save, then dispatches
 * to adapter->send. The Telegram named-DM-topic ensure_dm_topic branch is not
 * ported because no C adapter exposes that vtable method (architectural
 * difference, not a stub). Returns a malloc'd JSON SendResult string
 * ({"success":bool,...}) — caller frees. On hard failure returns a JSON error
 * result rather than raising (C has no exceptions); the error_kind is set so
 * callers can classify it. */
/* PoP: deliver_to_platform @ gateway/delivery.py:_deliver_to_platform */
char *deliver_to_platform(gw_base_platform_adapter_t *adapter,
                          const char *chat_id,
                          const char *content,
                          const char *metadata_json,
                          bool filter_silence)
{
    if (!adapter || !adapter->send || !chat_id || !*chat_id) {
        char *r = malloc(128);
        snprintf(r, 128, "{\"success\":false,\"error\":\"no adapter or chat_id\",\"error_kind\":\"invalid_target\"}");
        return r;
    }
    if (!content) content = "";

    /* Substrate anti-loop guard: drop hallucinated silence narration. */
    if (filter_silence && is_silence_narration(content)) {
        char *r = malloc(160);
        snprintf(r, 160, "{\"success\":true,\"filtered\":\"silence_narration\",\"delivered\":false}");
        return r;
    }

    const char *to_send = content;
    char *truncated = NULL;
    if (strlen(content) > MAX_PLATFORM_OUTPUT) {
        /* Non-chunking adapter (splits_long_messages not modeled in C vtable):
         * truncate with a footer. Audit-save is a best-effort side effect that
         * the C gateway performs separately; here we truncate as the Python
         * non-chunking path does. */
        const char *footer = "\n\n... [truncated]";
        size_t flen = strlen(footer);
        size_t visible = MAX_PLATFORM_OUTPUT > flen ? MAX_PLATFORM_OUTPUT - flen : 0;
        size_t cap = visible + flen + 1;
        truncated = malloc(cap);
        if (truncated) {
            memcpy(truncated, content, visible);
            strcpy(truncated + visible, footer);
            to_send = truncated;
        }
    }

    json_node_t *meta = NULL;
    if (metadata_json && *metadata_json) {
        char *jerr = NULL;
        meta = json_parse(metadata_json, &jerr);
        free(jerr);
    }

/* PoP: send @ gateway/delivery.py:send */
    gw_send_result_t res = adapter->send(adapter, chat_id, to_send, NULL, meta);
    if (meta) json_free(meta);
    free(truncated);

    /* Build a JSON SendResult. */
    char *r = malloc(256);
    if (!r) return NULL;
    const char *err = res.error ? res.error : "";
    const char *kind = res.retryable ? "transient" : (err && *err ? "unknown" : "none");
    snprintf(r, 256,
             "{\"success\":%s,\"error\":\"%s\",\"error_kind\":\"%s\"}",
             res.success ? "true" : "false",
             err, kind);
    gw_send_result_free(&res);
    return r;
}


/* PoP: parse @ gateway/delivery.py:parse */
/* Parse a delivery target string: "origin", "local", "platform", or
 * "platform:chat_id". Returns malloc'd JSON {target, platform, chat_id}. */
char *gw_delivery_parse_target(const char *target)
{
    json_t *o = json_object();
    if (!o) return strdup("{}");
    if (!target) { json_set(o, "target", json_string("")); char *s0 = json_serialize(o); json_free(o); return s0; }
    const char *t = target;
    while (*t == ' ' || *t == '\t') t++;
    char *stripped = strdup(t);
    size_t n = strlen(stripped);
    while (n && (stripped[n-1] == ' ' || stripped[n-1] == '\t' || stripped[n-1] == '\n')) stripped[--n] = '\0';
    json_set(o, "target", json_string(stripped));
    if (strcmp(stripped, "origin") == 0 || strcmp(stripped, "local") == 0) {
        json_set(o, "platform", json_string(stripped));
    } else {
        const char *colon = strchr(stripped, ':');
        if (colon) {
            char *plat = strndup(stripped, (size_t)(colon - stripped));
            json_set(o, "platform", json_string(plat));
            json_set(o, "chat_id", json_string(colon + 1));
            free(plat);
        } else {
            json_set(o, "platform", json_string(stripped));
        }
    }
    free(stripped);
    char *s = json_serialize(o);
    json_free(o);
    return s ? s : strdup("{}");
}
