/*
 * noc_w30_feishucomm_6493fb_p2.h - C surface for upstream
 * plugins/platforms/feishu/feishu_comment.py, text/timeline half
 * (gate w30_feishucomm_6493fb_p2).
 *
 * Every export here is a pure value->value function over comment replies and
 * the strings derived from them: rich-text element lists to plain text,
 * document-link extraction/dedupe, the timeline window a prompt embeds, and
 * the prompt text itself.  Network fetches (comments, replies, wiki nodes) are
 * fed in as already-fetched values.
 */
#ifndef NOC_W30_FEISHUCOMM_6493FB_P2_H
#define NOC_W30_FEISHUCOMM_6493FB_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"text"} -> escaped text_run content (& < > only). */
json_t *w30_feishucomm_6493fb_p2_sanitize(const json_t *req);

/* {"text","limit"?} -> ["chunk", ...]; prefers the last newline before the
 * limit (4000 by default) and strips the newlines it split on. */
json_t *w30_feishucomm_6493fb_p2_chunk(const json_t *req);

/* {"reply": {...}, "semantic"?: bool, "self_open_id"?: str} -> plain text of
 * the reply's content (text_run / docs_link / person elements). */
json_t *w30_feishucomm_6493fb_p2_extract_text(const json_t *req);

/* {"reply": {...}} -> the reply's user id (open_id, else user_id, else ""). */
json_t *w30_feishucomm_6493fb_p2_reply_user(const json_t *req);

/* {"whole": {...}} -> its reply_list.replies (reply_list may be a JSON
 * string). */
json_t *w30_feishucomm_6493fb_p2_reply_list(const json_t *req);

/* {"replies":[...]} -> [{"url","doc_type","token"}...], first occurrence of
 * each token only. */
json_t *w30_feishucomm_6493fb_p2_extract_links(const json_t *req);

/* {"links":[...], "current"?: str} -> the "Referenced documents in comments:"
 * block, or "" with no links. */
json_t *w30_feishucomm_6493fb_p2_format_docs(const json_t *req);

/* {"text","limit"?} -> text, or text[:limit] + "..." (220 by default). */
json_t *w30_feishucomm_6493fb_p2_truncate(const json_t *req);

/* {"timeline":[...], "limit","center","pinned"?:[int...]} -> the selected
 * entries (pinned + center, then outward; last `limit` when none). */
json_t *w30_feishucomm_6493fb_p2_select(const json_t *req);

/* {"replies":[...], "self_open_id"?: str} -> [{"user_id","text","is_self"}]. */
json_t *w30_feishucomm_6493fb_p2_timeline(const json_t *req);

/* build_local_comment_prompt's kwargs -> the prompt text. */
json_t *w30_feishucomm_6493fb_p2_build_local(const json_t *req);

/* build_whole_comment_prompt's kwargs -> the prompt text. */
json_t *w30_feishucomm_6493fb_p2_build_whole(const json_t *req);

/* {"doc":{doc_title,doc_url,file_token,file_type,self_open_id},
 *  "from_open_id", "whole_comments":[{reply_list}...]} ->
 * _whole_comment_prompt's result with the wiki resolve seam pinned to
 * "unresolved links". */
json_t *w30_feishucomm_6493fb_p2_whole_prompt(const json_t *req);

/* {"doc":{...}, "comment_id","reply_id","from_open_id","quote_text",
 *  "replies":[...]} -> _local_comment_prompt's result (same seam). */
json_t *w30_feishucomm_6493fb_p2_local_prompt(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
