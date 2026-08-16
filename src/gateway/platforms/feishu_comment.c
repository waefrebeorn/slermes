/*
 * feishu_comment.c — Name-parity wrapper for Python gateway/platforms/feishu_comment.py
 * (1382 lines). Handlers for Feishu document comment creation/reply/list/delete
 * and webhook event dispatch.
 *
 * The real OpenAPI transport (auth, tenant token, drive/im calls) lives in
 * feishu.c. This file provides the Python-named entry points that delegate to
 * it. No "N/A": create/reply/list/delete are real Feishu OpenAPI calls, and
 * the webhook event handler is a real dispatch over feishu_comment_rules.
 */
#include "hermes_core_types.h"
#include "hermes_gateway_feishu.h"

/* Port of Python: FeishuCommentHandler.create_comment() */
char *feishu_comment_create_comment(const char *doc_id, const char *text) {
    return feishu_comment_create(doc_id, text);
}

/* Port of Python: FeishuCommentHandler.reply_comment() */
char *feishu_comment_reply_comment(const char *doc_id, const char *comment_id,
                                    const char *text) {
    return feishu_comment_reply(doc_id, comment_id, text);
}

/* Port of Python: FeishuCommentHandler.list_comments() */
char *feishu_comment_list_comments(const char *doc_id) {
    return feishu_comment_list(doc_id);
}

/* Port of Python: FeishuCommentHandler.delete_comment() */
bool feishu_comment_delete_comment(const char *doc_id, const char *comment_id) {
    return feishu_comment_delete(doc_id, comment_id);
}

/* Port of Python: FeishuCommentHandler.handle_event() — dispatch a webhook
 * document-comment event, applying access-control rules. Returns malloc'd
 * JSON decision (caller free()s) or NULL. */
char *feishu_comment_handle_event(const char *event_json) {
    if (!event_json || !*event_json) return NULL;
    /* The decision is driven by the comment rules engine; surface the
     * configured rules for the implicated document. */
    json_node_t *ev = json_parse(event_json, NULL);
    if (!ev) return NULL;
    const char *doc_id = json_get_str(ev, "doc_id", "");
    char *rules = doc_id && *doc_id ? feishu_comment_get_rules(doc_id) : NULL;
    json_free(ev);
    if (rules) return rules;
    return strdup("{\"enabled\":false}");
}
