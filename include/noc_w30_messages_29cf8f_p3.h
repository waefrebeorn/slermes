/*
 * noc_w30_messages_29cf8f_p3.h - C surface for upstream
 * persistence/sqlite/conversation/messages.py, gate p3 (SQL text and read
 * envelopes).
 *
 * The statement builders the module hands its connections byte-for-byte
 * (latest_dialogue_message, message_storage_state, the two payload scans,
 * has_any_message, the spillover reference sweep) plus the read envelopes
 * that shape what a store read returns: the session timeline page, the prompt
 * page, the anchored window/view, the PR-url hits, the handoff row map and
 * the active watermark coercion.
 *
 * The connections and the store's readers stay in the caller.
 */
#ifndef NOC_W30_MESSAGES_29CF8F_P3_H
#define NOC_W30_MESSAGES_29CF8F_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"session_id": s, "row": [content]|null} -> {"sql", "params", "out"} for
 * latest_dialogue_message. */
json_t *w30_messages_29cf8f_p3_sql_latest_dialogue(const json_t *in);

/* {"message_id": n, "row": {...}|null} -> {"sql", "params", "out"} for
 * message_storage_state's row read. */
json_t *w30_messages_29cf8f_p3_sql_message_storage_state(const json_t *in);

/* {"since_ts": n, "rows": [[raw]...]} -> {"sql", "params", "out"} for
 * terminal_tool_call_payloads. */
json_t *w30_messages_29cf8f_p3_sql_terminal_payloads(const json_t *in);

/* {"since_ts": n, "rows": [[tool_call_id, content]...]} -> {"sql", "params",
 * "out"} for blocked_tool_result_payloads. */
json_t *w30_messages_29cf8f_p3_sql_blocked_payloads(const json_t *in);

/* {"table": b, "any": b} -> {"statements", "out"} for has_any_message. */
json_t *w30_messages_29cf8f_p3_sql_has_any_message(const json_t *in);

/* {"rows": [[content, api_content]...]} -> {"sql", "out"} for
 * spillover_reference_rows. */
json_t *w30_messages_29cf8f_p3_sql_spillover_reference(const json_t *in);

/* {} -> {"spillover_filter": str, "spillover_limit": 20001}. */
json_t *w30_messages_29cf8f_p3_sql_filter(const json_t *in);

/* {"result": {entries, pagination}} -> the SessionTimeline envelope. */
json_t *w30_messages_29cf8f_p3_timeline_page(const json_t *in);

/* {"result": {messages, pagination}|null} -> the PromptPage envelope or null. */
json_t *w30_messages_29cf8f_p3_prompt_page(const json_t *in);

/* {"around": {window, messages_before, messages_after}} -> AnchoredWindow. */
json_t *w30_messages_29cf8f_p3_anchored_window(const json_t *in);

/* {"view": {window, messages_before, messages_after, bookend_start,
 * bookend_end}} -> AnchoredView. */
json_t *w30_messages_29cf8f_p3_anchored_view(const json_t *in);

/* {"hits": [{session_id, content}...]} -> [PrUrlHit...]. */
json_t *w30_messages_29cf8f_p3_pr_url_hits(const json_t *in);

/* {"found": {shown_id: [row...]}} -> {shown_id: [TranscriptRow...]}. */
json_t *w30_messages_29cf8f_p3_handoff_map(const json_t *in);

/* {"last": n|null} -> {"last_active": n|null} (active_watermark). */
json_t *w30_messages_29cf8f_p3_watermark(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
