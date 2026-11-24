/*
 * noc_w27_adapter_74f618_p6.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p6).
 *
 * Gate 6: native task-card / stream frame shaping, the elapsed status label,
 * the bot-sender policy and the small Block Kit payload constructors.
 */
#ifndef NOC_W27_ADAPTER_74F618_P6_H
#define NOC_W27_ADAPTER_74F618_P6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"task": obj} -> {type,id,title,status} */
json_t *w27_p6_task_update_chunk(const json_t *req);

/* req: {"text": str, "glyphs": [str]?} -> str */
json_t *w27_p6_strip_stream_cursor(const json_t *req);

/* req: {"chat_id": str, "metadata": obj|null, "reply_in_thread": bool?,
 *       "scope_id": str?} -> [team, chat, thread]|null */
json_t *w27_p6_stream_key(const json_t *req);

/* req: {"text": str} -> str */
json_t *w27_p6_mrkdwn_insensitive(const json_t *req);

/* req: {"sent": str, "text": str} -> [relation, delta] */
json_t *w27_p6_stream_relation(const json_t *req);

/* req: {"started": number|null, "now": number} -> str */
json_t *w27_p6_default_status_text(const json_t *req);

/* req: {"raw": any} -> "thread"|"in_channel" */
json_t *w27_p6_cron_continuable_surface(const json_t *req);

/* req: {"raw": any} -> "none"|"mentions"|"all" */
json_t *w27_p6_slack_allow_bots(const json_t *req);

/* req: {"raw": any} -> [str] (sorted) */
json_t *w27_p6_api_human_users(const json_t *req);

/* req: {"event": obj, "human_users": [str]} -> bool */
json_t *w27_p6_event_declares_bot_sender(const json_t *req);

/* req: {"preview_text": str} -> str */
json_t *w27_p6_format_tool_preview(const json_t *req);

/* req: {"content": str, "max_chars": int?} -> [block]|null */
json_t *w27_p6_markdown_block_payload(const json_t *req);

/* req: {"blocks": [..]|null, "enabled": bool} -> [..]|null */
json_t *w27_p6_append_feedback_block(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P6_H */
