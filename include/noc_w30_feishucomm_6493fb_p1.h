/*
 * noc_w30_feishucomm_6493fb_p1.h - C surface for upstream
 * plugins/platforms/feishu/feishu_comment.py, event/request half
 * (gate w30_feishucomm_6493fb_p1).
 *
 * Every export here is a pure value->value function of what the Feishu drive
 * comment flow receives and what it would send: the flattened event, the
 * request body a call builds, the retry/pagination verdict over canned
 * responses, and the delivery plan over canned reply codes.  The lark_oapi
 * transport, the tenant token, the wiki lookup and the agent run stay in the
 * caller.
 */
#ifndef NOC_W30_FEISHUCOMM_6493FB_P1_H
#define NOC_W30_FEISHUCOMM_6493FB_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"event": {...}} -> {"event_id","comment_id","reply_id","timestamp",
 *                       "file_token","file_type","notice_type",
 *                       "from_open_id","to_open_id","is_mentioned"}
 * or null when the payload has no event object. */
json_t *w30_feishucomm_6493fb_p1_parse_event(const json_t *payload);

/* {"file_token","file_type","results":[{"code","data"}...]} ->
 * {"result":{title,url,doc_type}, "method","uri","paths","queries","body"}.
 * result is {} on a failing code or missing metas. */
json_t *w30_feishucomm_6493fb_p1_meta_result(const json_t *req);

/* {"file_token","file_type","comment_id","results":[...]} ->
 * {"result":item|{} , "attempts":n, "method","uri","paths","queries","body"}.
 * At most 6 attempts (the 5 s retry loop); the first code==0 wins. */
json_t *w30_feishucomm_6493fb_p1_comment_result(const json_t *req);

/* {"pages":[{"code","data"}...]} ->
 * {"items":[...], "fetch_ok":bool, "calls":n}; five pages max, a page_token
 * only follows a has_more page. */
json_t *w30_feishucomm_6493fb_p1_pages_result(const json_t *req);

/* {"action","file_token","file_type","reply_id","reaction_type"?,"code"} ->
 * {"ok":bool,"method","uri","paths","queries","body"} for the Drive v2
 * reaction endpoint (reaction_type defaults to "OK"). */
json_t *w30_feishucomm_6493fb_p1_reaction_result(const json_t *req);

/* {"file_token","file_type","comment_id","text","code"} ->
 * {"ok","code","method","uri","paths","queries","body"} for reply_to_comment
 * (text is sanitized into a text_run element). */
json_t *w30_feishucomm_6493fb_p1_reply_result(const json_t *req);

/* {"file_token","file_type","text","code"} ->
 * {"ok","method","uri","paths","queries","body"} for add_whole_comment
 * (text is sanitized into a reply_elements text entry). */
json_t *w30_feishucomm_6493fb_p1_whole_comment_result(const json_t *req);

/* {"text","is_whole","reply_codes":[{"code"}...]?,"whole_ok":[bool...]?} ->
 * {"ok":bool, "steps":[{"api":"reply"|"whole","text","ok","code"?}]}.
 * Chunks the text at 4000 chars and flips to whole-comment on code 1069302. */
json_t *w30_feishucomm_6493fb_p1_deliver_result(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
