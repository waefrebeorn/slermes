/*
 * noc_w27_adapter_74f618_p8.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p8).
 *
 * Gate 8: the markdown -> Slack mrkdwn converter (tables fenced first, code
 * and entities protected, broadcast mentions escaped, then emphasis), the
 * block-text merge, link-unfurl rendering, message_changed normalization and
 * the Block Kit constructors.
 */
#ifndef NOC_W27_ADAPTER_74F618_P8_H
#define NOC_W27_ADAPTER_74F618_P8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"content": str} -> str */
json_t *w27_p8_format_message(const json_t *req);

/* req: {"text": str, "blocks": [..]|null, "bot_uid": str?} -> str */
json_t *w27_p8_append_block_text(const json_t *req);

/* req: {"event": obj, "processed": [str]?} -> obj|null */
json_t *w27_p8_normalize_changed_message(const json_t *req);

/* req: {"text": str, "attachments": [..]|null} -> str */
json_t *w27_p8_append_link_unfurls(const json_t *req);

/* req: {"media_types": [str]|null} -> "TEXT"|"PHOTO"|"VIDEO"|"VOICE"|"DOCUMENT" */
json_t *w27_p8_media_message_type(const json_t *req);

/* req: {"event": obj, "ts": str, "is_dm": bool, "assistant_meta": obj|null,
 *       "dm_top_level": bool?, "reply_in_thread": bool?} -> str|null */
json_t *w27_p8_session_thread_ts(const json_t *req);

/* req: {"text": str, "action_id": str, "value": str, "style": str?,
 *       "emoji": bool?} -> obj */
json_t *w27_p8_button(const json_t *req);

/* req: {"body": obj, "action": obj} -> [action_id, value, message, msg_ts,
 *       channel_id, user_name, user_id] */
json_t *w27_p8_interaction_fields(const json_t *req);

/* req: {"message": obj, "limit": int|null} -> str */
json_t *w27_p8_section_text(const json_t *req);

/* req: {"channel_id": str, "thread_ts": str, "team_id": str} -> str */
json_t *w27_p8_thread_cache_key(const json_t *req);

/* req: {"messages": [obj]|null, "thread_ts": str} -> obj|null */
json_t *w27_p8_thread_root_message(const json_t *req);

/* req: {"msg": obj, "bot_uid": str?, "own_ids": [str]?} -> str */
json_t *w27_p8_render_message_text(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P8_H */
