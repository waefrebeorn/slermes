/*
 * noc_w30_adapter_7683aa_p2.h - C surface for upstream
 * plugins/platforms/mattermost/adapter.py (gate w30_adapter_7683aa_p2).
 *
 * The inbound half of the Mattermost adapter: WebSocket post admission,
 * mention gating, thread derivation and the post/attachment payloads.  The
 * WebSocket itself, the REST session and the media cache are seams fed as
 * fixture inputs; only the value->value decisions are ported.
 *
 * PoP: w30_mm_p2_apply_channel_gating     @ adapter.py:MattermostAdapter._apply_channel_gating
 * PoP: w30_mm_p2_is_configured_bot_user   @ adapter.py:MattermostAdapter._is_configured_bot_user
 * PoP: w30_mm_p2_post_payload             @ adapter.py:MattermostAdapter._post_message
 * PoP: w30_mm_p2_broken_root_fallback     @ adapter.py:MattermostAdapter._post_preserving_thread
 * PoP: w30_mm_p2_ws_event                 @ adapter.py:MattermostAdapter._handle_ws_event
 * PoP: w30_mm_p2_resolved_root_id         @ adapter.py:MattermostAdapter._resolve_root_id
 * PoP: w30_mm_p2_typing_request           @ adapter.py:MattermostAdapter.send_typing
 * PoP: w30_mm_p2_multi_image_empty        @ adapter.py:MattermostAdapter.send_multiple_images
 * PoP: w30_mm_p2_post_with_file_result    @ adapter.py:MattermostAdapter._post_with_file
 */
#ifndef NOC_W30_ADAPTER_7683AA_P2_H
#define NOC_W30_ADAPTER_7683AA_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gate result: the cleaned text, or null when the post must be ignored.
 * req: {channel_id, message_text, bot_username, bot_user_id, extra, env} */
json_t *w30_mm_p2_apply_channel_gating(const json_t *req);

/* True when user_id is in the configured MATTERMOST_BOT_USERS set. */
json_t *w30_mm_p2_is_configured_bot_user(const json_t *req);

/* The posts payload: channel_id/message[/file_ids], mentions disabled,
 * props first, root_id only in thread reply mode with a resolvable candidate. */
json_t *w30_mm_p2_post_payload(const json_t *req);

/* {"flat_fallback","posts","flat_keys"}: does a failed threaded post retry
 * flat in the channel? */
json_t *w30_mm_p2_broken_root_fallback(const json_t *req);

/* One WebSocket event -> {"dispatched", text, message_type, chat_id,
 * chat_type, user_id, user_name, thread_id, message_id, is_bot,
 * media_urls, media_types}. */
json_t *w30_mm_p2_ws_event(const json_t *req);

/* A post id resolved through GET posts/{id} to its thread root. */
json_t *w30_mm_p2_resolved_root_id(const json_t *req);

/* POST users/{bot}/typing {"channel_id": chat_id} -> {"path","payload"}. */
json_t *w30_mm_p2_typing_request(const json_t *req);

/* The empty-batch guard of send_multiple_images -> {"success","error"}. */
json_t *w30_mm_p2_multi_image_empty(const json_t *req);

/* _post_with_file -> {"success","message_id","error","message","file_ids"}. */
json_t *w30_mm_p2_post_with_file_result(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W30_ADAPTER_7683AA_P2_H */
