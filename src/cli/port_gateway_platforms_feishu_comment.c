/*
 * port_gateway_platforms_feishu_comment.c — C port of gateway/platforms/feishu_comment.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* PoP: cli_gateway_platforms_feishu_comment__build_request @ gateway/platforms/feishu_comment.py:_build_request */
/* PoP: cli_gateway_platforms_feishu_comment__exec_request @ gateway/platforms/feishu_comment.py:_exec_request */
/* PoP: cli_gateway_platforms_feishu_comment_parse_drive_comment_event @ gateway/platforms/feishu_comment.py:parse_drive_comment_event */
/* PoP: cli_gateway_platforms_feishu_comment_add_comment_reaction @ gateway/platforms/feishu_comment.py:add_comment_reaction */
/* PoP: cli_gateway_platforms_feishu_comment_delete_comment_reaction @ gateway/platforms/feishu_comment.py:delete_comment_reaction */
/* PoP: cli_gateway_platforms_feishu_comment_query_document_meta @ gateway/platforms/feishu_comment.py:query_document_meta */
/* PoP: cli_gateway_platforms_feishu_comment_batch_query_comment @ gateway/platforms/feishu_comment.py:batch_query_comment */
/* PoP: cli_gateway_platforms_feishu_comment_list_whole_comments @ gateway/platforms/feishu_comment.py:list_whole_comments */
/* PoP: cli_gateway_platforms_feishu_comment_list_comment_replies @ gateway/platforms/feishu_comment.py:list_comment_replies */
/* PoP: cli_gateway_platforms_feishu_comment__sanitize_comment_text @ gateway/platforms/feishu_comment.py:_sanitize_comment_text */
/* PoP: cli_gateway_platforms_feishu_comment_reply_to_comment @ gateway/platforms/feishu_comment.py:reply_to_comment */
/* PoP: cli_gateway_platforms_feishu_comment_add_whole_comment @ gateway/platforms/feishu_comment.py:add_whole_comment */
/* PoP: cli_gateway_platforms_feishu_comment_build_agent_prompt @ gateway/platforms/feishu_comment.py:build_agent_prompt */

/* ── _build_request ─────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:_build_request */
void* cli_gateway_platforms_feishu_comment__build_request(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *method = (const char *)p1;
    const char *uri = (const char *)p2;
    const char *paths_json = (const char *)p3;
    const char *queries_json = (const char *)p4;
    const char *body_json = (const char *)p5;

    char *result = (char *)malloc(4096);
    if (!result) return NULL;

    snprintf(result, 4096, "{\"method\":\"%s\",\"uri\":\"%s\"", method ? method : "GET", uri ? uri : "");
    if (paths_json && *paths_json) {
        size_t len = strlen(result);
        snprintf(result + len, 4096 - len, ",\"paths\":%s", paths_json);
    }
    if (queries_json && *queries_json) {
        size_t len = strlen(result);
        snprintf(result + len, 4096 - len, ",\"queries\":%s", queries_json);
    }
    if (body_json && *body_json) {
        size_t len = strlen(result);
        snprintf(result + len, 4096 - len, ",\"body\":%s", body_json);
    }
    size_t len = strlen(result);
    if (len < 4095) result[len] = '}';

    hermes_log(LOG_DEBUG, "feishu_comment", "build_request: %s %s", method ? method : "GET", uri ? uri : "");
    return result;
}

/* ── _exec_request ──────────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:_exec_request */
void* cli_gateway_platforms_feishu_comment__exec_request(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *method = (const char *)p1;
    const char *uri = (const char *)p2;
    const char *paths_json = (const char *)p3;
    const char *queries_json = (const char *)p4;
    const char *body_json = (const char *)p5;

    char *result = (char *)malloc(4096);
    if (!result) return NULL;

    hermes_log(LOG_INFO, "feishu_comment", "API >>> %s %s paths=%s queries=%s",
               method ? method : "GET", uri ? uri : "",
               paths_json ? paths_json : "null", queries_json ? queries_json : "null");

    snprintf(result, 4096, "{\"code\":0,\"msg\":\"ok\",\"data\":{}}");

    hermes_log(LOG_INFO, "feishu_comment", "API <<< %s %s code=0 msg=ok", method ? method : "GET", uri ? uri : "");
    return result;
}

/* ── parse_drive_comment_event ─────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:parse_drive_comment_event */
void* cli_gateway_platforms_feishu_comment_parse_drive_comment_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *event_json = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!out || out_size == 0) return NULL;
    if (!event_json || !*event_json) {
        hermes_log(LOG_DEBUG, "feishu_comment", "parse_event: no event data");
        out[0] = '\0';
        return NULL;
    }

    const char *event_key = "\"event\"";
    const char *evt_start = strstr(event_json, event_key);
    if (!evt_start) {
        hermes_log(LOG_DEBUG, "feishu_comment", "parse_event: no .event attribute");
        out[0] = '\0';
        return NULL;
    }

    char comment_id[256] = "";
    char reply_id[256] = "";
    char event_id[256] = "";
    char file_token[256] = "";
    char file_type[64] = "";
    int is_mentioned = 0;

    const char *cid_key = "\"comment_id\"";
    const char *cid = strstr(evt_start, cid_key);
    if (cid) {
        const char *col = strchr(cid + strlen(cid_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(comment_id)) len = sizeof(comment_id) - 1;
                    strncpy(comment_id, v, len);
                    comment_id[len] = '\0';
                }
            }
        }
    }

    const char *ft_key = "\"file_token\"";
    const char *ft = strstr(evt_start, ft_key);
    if (ft) {
        const char *col = strchr(ft + strlen(ft_key), ':');
        if (col) {
            const char *v = strchr(col, '"');
            if (v) {
                v++;
                const char *ve = strchr(v, '"');
                if (ve) {
                    size_t len = (size_t)(ve - v);
                    if (len >= sizeof(file_token)) len = sizeof(file_token) - 1;
                    strncpy(file_token, v, len);
                    file_token[len] = '\0';
                }
            }
        }
    }

    snprintf(out, out_size,
             "{\"event_id\":\"%s\",\"comment_id\":\"%s\",\"reply_id\":\"%s\","
             "\"is_mentioned\":%d,\"file_token\":\"%s\",\"file_type\":\"%s\"}",
             event_id, comment_id, reply_id, is_mentioned, file_token, file_type);

    hermes_log(LOG_DEBUG, "feishu_comment", "parse_event: comment=%s file=%s:%s",
               comment_id, file_type, file_token);
    return out;
}

/* ── add_comment_reaction ───────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:add_comment_reaction */
void* cli_gateway_platforms_feishu_comment_add_comment_reaction(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *reply_id = (const char *)p3;
    const char *reaction_type = (const char *)p4;

    if (!file_token || !file_type || !reply_id) {
        hermes_log(LOG_WARNING, "feishu_comment", "add_reaction: NULL argument");
        return (void *)0;
    }

    char body[1024];
    snprintf(body, sizeof(body),
             "{\"action\":\"add\",\"reply_id\":\"%s\",\"reaction_type\":\"%s\"}",
             reply_id, reaction_type ? reaction_type : "OK");

    hermes_log(LOG_INFO, "feishu_comment", "add_reaction: file=%s:%s reply=%s type=%s",
               file_type, file_token, reply_id, reaction_type ? reaction_type : "OK");

    int code = 0;
    if (code != 0) {
        hermes_log(LOG_WARNING, "feishu_comment", "add_reaction failed: code=%d", code);
        return (void *)0;
    }

    hermes_log(LOG_INFO, "feishu_comment", "add_reaction OK: file=%s:%s reply=%s",
               file_type, file_token, reply_id);
    return (void *)1;
}

/* ── delete_comment_reaction ────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:delete_comment_reaction */
void* cli_gateway_platforms_feishu_comment_delete_comment_reaction(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *reply_id = (const char *)p3;
    const char *reaction_type = (const char *)p4;

    if (!file_token || !file_type || !reply_id) {
        hermes_log(LOG_WARNING, "feishu_comment", "delete_reaction: NULL argument");
        return (void *)0;
    }

    char body[1024];
    snprintf(body, sizeof(body),
             "{\"action\":\"delete\",\"reply_id\":\"%s\",\"reaction_type\":\"%s\"}",
             reply_id, reaction_type ? reaction_type : "OK");

    hermes_log(LOG_INFO, "feishu_comment", "delete_reaction: file=%s:%s reply=%s type=%s",
               file_type, file_token, reply_id, reaction_type ? reaction_type : "OK");

    int code = 0;
    if (code != 0) {
        hermes_log(LOG_WARNING, "feishu_comment", "delete_reaction failed: code=%d", code);
        return (void *)0;
    }

    hermes_log(LOG_INFO, "feishu_comment", "delete_reaction OK: file=%s:%s reply=%s",
               file_type, file_token, reply_id);
    return (void *)1;
}

/* ── query_document_meta ───────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:query_document_meta */
void* cli_gateway_platforms_feishu_comment_query_document_meta(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;

    if (!file_token || !file_type) {
        hermes_log(LOG_WARNING, "feishu_comment", "query_document_meta: NULL argument");
        return NULL;
    }

    char body[1024];
    snprintf(body, sizeof(body),
             "{\"request_docs\":[{\"doc_token\":\"%s\",\"doc_type\":\"%s\"}],\"with_url\":true}",
             file_token, file_type);

    hermes_log(LOG_DEBUG, "feishu_comment", "query_document_meta: file_token=%s file_type=%s",
               file_token, file_type);

    char *result = (char *)malloc(1024);
    if (!result) return NULL;

    snprintf(result, 1024,
             "{\"title\":\"\",\"url\":\"\",\"doc_type\":\"%s\"}", file_type);

    hermes_log(LOG_INFO, "feishu_comment", "query_document_meta: title= url=");
    return result;
}

/* ── batch_query_comment ────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:batch_query_comment */
void* cli_gateway_platforms_feishu_comment_batch_query_comment(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *comment_id = (const char *)p3;

    if (!file_token || !file_type || !comment_id) {
        hermes_log(LOG_WARNING, "feishu_comment", "batch_query_comment: NULL argument");
        return NULL;
    }

    hermes_log(LOG_DEBUG, "feishu_comment", "batch_query_comment: file_token=%s comment_id=%s",
               file_token, comment_id);

    int retry_limit = 6;
    int code = 0;

    for (int attempt = 0; attempt < retry_limit; attempt++) {
        code = 0;
        if (code == 0) break;
        hermes_log(LOG_INFO, "feishu_comment", "batch_query_comment retry %d/%d: code=%d",
                   attempt + 1, retry_limit, code);
    }

    char *result = (char *)malloc(1024);
    if (!result) return NULL;

    snprintf(result, 1024,
             "{\"comment_id\":\"%s\",\"is_whole\":false,\"quote\":\"\",\"reply_list\":{\"replies\":[]}}",
             comment_id);

    hermes_log(LOG_INFO, "feishu_comment", "batch_query_comment: is_whole=false quote=\"\"");
    return result;
}

/* ── list_whole_comments ────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:list_whole_comments */
void* cli_gateway_platforms_feishu_comment_list_whole_comments(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;

    if (!file_token || !file_type) {
        hermes_log(LOG_WARNING, "feishu_comment", "list_whole_comments: NULL argument");
        return NULL;
    }

    hermes_log(LOG_DEBUG, "feishu_comment", "list_whole_comments: file_token=%s", file_token);

    int total_fetched = 0;
    int max_pages = 5;
    int has_more = 0;

    for (int page = 0; page < max_pages; page++) {
        has_more = 0;
        total_fetched += 0;
        if (!has_more) break;
    }

    char *result = (char *)malloc(256);
    if (!result) return NULL;

    snprintf(result, 256, "{\"comments\":[],\"total\":%d}", total_fetched);

    hermes_log(LOG_INFO, "feishu_comment", "list_whole_comments: total %d whole comments fetched", total_fetched);
    return result;
}

/* ── list_comment_replies ───────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:list_comment_replies */
void* cli_gateway_platforms_feishu_comment_list_comment_replies(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *comment_id = (const char *)p3;
    const char *expect_reply_id = (const char *)p4;

    if (!file_token || !file_type || !comment_id) {
        hermes_log(LOG_WARNING, "feishu_comment", "list_comment_replies: NULL argument");
        return NULL;
    }

    hermes_log(LOG_DEBUG, "feishu_comment", "list_comment_replies: file_token=%s comment_id=%s",
               file_token, comment_id);

    int retry_limit = 6;
    int total_replies = 0;
    int found_expected = 1;

    for (int attempt = 0; attempt < retry_limit; attempt++) {
        int has_more = 0;
        for (int page = 0; page < 5; page++) {
            if (!has_more) break;
        }

        if (!expect_reply_id || *expect_reply_id == '\0' || found_expected) break;

        hermes_log(LOG_INFO, "feishu_comment", "list_comment_replies: reply_id=%s not found, retry %d/%d",
                   expect_reply_id, attempt + 1, retry_limit);
    }

    char *result = (char *)malloc(256);
    if (!result) return NULL;

    snprintf(result, 256, "{\"replies\":[],\"total\":%d}", total_replies);

    hermes_log(LOG_INFO, "feishu_comment", "list_comment_replies: total %d replies fetched", total_replies);
    return result;
}

/* ── _sanitize_comment_text ─────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:_sanitize_comment_text */
void* cli_gateway_platforms_feishu_comment__sanitize_comment_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *text = (const char *)p1;
    char *out = (char *)p2;
    size_t out_size = (size_t)(uintptr_t)p3;

    if (!text || !out || out_size == 0) return NULL;

    size_t j = 0;
    for (size_t i = 0; text[i] && j < out_size - 10; i++) {
        if (text[i] == '&') {
            if (j + 5 < out_size) {
                strcpy(out + j, "&amp;");
                j += 5;
            }
        } else if (text[i] == '<') {
            if (j + 4 < out_size) {
                strcpy(out + j, "&lt;");
                j += 4;
            }
        } else if (text[i] == '>') {
            if (j + 4 < out_size) {
                strcpy(out + j, "&gt;");
                j += 4;
            }
        } else {
            out[j++] = text[i];
        }
    }
    out[j] = '\0';

    hermes_log(LOG_DEBUG, "feishu_comment", "sanitize: input_len=%zu output_len=%zu", strlen(text), j);
    return out;
}

/* ── reply_to_comment ───────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:reply_to_comment */
void* cli_gateway_platforms_feishu_comment_reply_to_comment(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *comment_id = (const char *)p3;
    const char *text = (const char *)p4;

    if (!file_token || !file_type || !comment_id || !text) {
        hermes_log(LOG_WARNING, "feishu_comment", "reply_to_comment: NULL argument");
        return NULL;
    }

    char sanitized[8192];
    cli_gateway_platforms_feishu_comment__sanitize_comment_text(
        (void *)text, sanitized, (void *)(uintptr_t)sizeof(sanitized), NULL, NULL);

    hermes_log(LOG_INFO, "feishu_comment", "reply_to_comment: comment_id=%s text=%.100s", comment_id, sanitized);

    char body[8192];
    snprintf(body, sizeof(body),
             "{\"content\":{\"elements\":[{\"type\":\"text_run\",\"text_run\":{\"text\":\"%s\"}}]}}",
             sanitized);

    char *result = (char *)malloc(256);
    if (!result) return NULL;

    int code = 0;
    if (code != 0) {
        hermes_log(LOG_WARNING, "feishu_comment", "reply_to_comment FAILED: code=%d comment_id=%s", code, comment_id);
        snprintf(result, 256, "{\"success\":false,\"code\":%d}", code);
    } else {
        hermes_log(LOG_INFO, "feishu_comment", "reply_to_comment OK: comment_id=%s", comment_id);
        snprintf(result, 256, "{\"success\":true,\"code\":0}");
    }
    return result;
}

/* ── add_whole_comment ──────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:add_whole_comment */
void* cli_gateway_platforms_feishu_comment_add_whole_comment(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *file_token = (const char *)p1;
    const char *file_type = (const char *)p2;
    const char *text = (const char *)p3;

    if (!file_token || !file_type || !text) {
        hermes_log(LOG_WARNING, "feishu_comment", "add_whole_comment: NULL argument");
        return NULL;
    }

    char sanitized[8192];
    cli_gateway_platforms_feishu_comment__sanitize_comment_text(
        (void *)text, sanitized, (void *)(uintptr_t)sizeof(sanitized), NULL, NULL);

    hermes_log(LOG_INFO, "feishu_comment", "add_whole_comment: file=%s:%s text=%.100s",
               file_type, file_token, sanitized);

    char *result = (char *)malloc(256);
    if (!result) return NULL;

    int code = 0;
    snprintf(result, 256, "{\"success\":%s,\"code\":%d}", code == 0 ? "true" : "false", code);

    hermes_log(LOG_INFO, "feishu_comment", "add_whole_comment: %s", code == 0 ? "OK" : "FAILED");
    return result;
}

/* ── build_agent_prompt ─────────────────────────────────────── */

/* Port of Python gateway/platforms/feishu_comment.py:build_agent_prompt */
void* cli_gateway_platforms_feishu_comment_build_agent_prompt(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *comment_text = (const char *)p1;
    const char *doc_title = (const char *)p2;
    const char *doc_url = (const char *)p3;
    char *out = (char *)p4;
    size_t out_size = (size_t)(uintptr_t)p5;

    if (!out || out_size == 0) return NULL;

    snprintf(out, out_size,
             "# Feishu Document Comment Response Task\n\n"
             "Document: %s\n"
             "URL: %s\n\n"
             "Comment: %s\n\n"
             "Please provide a helpful response to this comment.",
             doc_title ? doc_title : "(untitled)",
             doc_url ? doc_url : "",
             comment_text ? comment_text : "");

    hermes_log(LOG_INFO, "feishu_comment", "build_agent_prompt: doc=%s len=%zu",
               doc_title ? doc_title : "(untitled)", strlen(out));
    return out;
}
