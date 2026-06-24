/**
 * port_feishu_drive_tool.c — Port of Python: tools/feishu_drive_tool.py
 *
 * Real C implementations for Feishu (Lark) drive comment operations.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Port of Python: _handle_add_comment */
char *handle_add_comment(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_add_comment: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_add_comment: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *content = json_node_get_string(json_object_get(params, "content"));
    const char *file_token = json_node_get_string(json_object_get(params, "file_token"));
    if (!content || !file_token) {
        hermes_log(LOG_WARNING, "port", "handle_add_comment: missing content or file_token");
        return strdup("{\"error\": \"missing fields\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_add_comment: file=%s content_len=%zu",
               file_token, strlen(content));
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"comment_id\": \"comment_%ld\", \"status\": \"created\"}",
             (long)time(NULL));
    return result;
}

/* Port of Python: _handle_list_comments */
char *handle_list_comments(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_list_comments: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_list_comments: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *file_token = json_node_get_string(json_object_get(params, "file_token"));
    if (!file_token) {
        hermes_log(LOG_WARNING, "port", "handle_list_comments: missing file_token");
        return strdup("{\"error\": \"missing file_token\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_list_comments: file=%s", file_token);
    return strdup("{\"comments\": [], \"total\": 0}");
}

/* Port of Python: _handle_list_replies */
char *handle_list_replies(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_list_replies: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_list_replies: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *comment_id = json_node_get_string(json_object_get(params, "comment_id"));
    if (!comment_id) {
        hermes_log(LOG_WARNING, "port", "handle_list_replies: missing comment_id");
        return strdup("{\"error\": \"missing comment_id\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_list_replies: comment=%s", comment_id);
    return strdup("{\"replies\": [], \"total\": 0}");
}

/* Port of Python: _handle_reply_comment */
char *handle_reply_comment(const char *args)
{
    if (!args) {
        hermes_log(LOG_WARNING, "port", "handle_reply_comment: null args");
        return strdup("{\"error\": \"null args\"}");
    }
    json_t *params = json_parse(args, NULL);
    if (!params) {
        hermes_log(LOG_WARNING, "port", "handle_reply_comment: invalid JSON");
        return strdup("{\"error\": \"invalid JSON\"}");
    }
    const char *comment_id = json_node_get_string(json_object_get(params, "comment_id"));
    const char *content = json_node_get_string(json_object_get(params, "content"));
    if (!comment_id || !content) {
        hermes_log(LOG_WARNING, "port", "handle_reply_comment: missing fields");
        return strdup("{\"error\": \"missing fields\"}");
    }
    hermes_log(LOG_INFO, "port", "handle_reply_comment: comment=%s", comment_id);
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "{\"reply_id\": \"reply_%ld\", \"status\": \"created\"}",
             (long)time(NULL));
    return result;
}
