/*
 * noc_w27_adapter_97bbcb_p1.h - C surface for upstream
 * plugins/platforms/google_chat/adapter.py (gate w27_adapter_97bbcb_p1).
 *
 * Gate 1 of 4: inbound envelope shaping.  The adapter accepts three inbound
 * envelope dialects (Workspace Add-ons, native Chat API, flat relay) and
 * normalizes them into one (message, space) pair, then filters bot senders and
 * duplicates and shapes the outbound message body / create kwargs.  Everything
 * here is a pure function of the envelope values: no HTTP, no socket, no file.
 *
 * Every exported symbol takes the harness request object and returns the JSON
 * "out" value.
 */
#ifndef NOC_W27_ADAPTER_97BBCB_P1_H
#define NOC_W27_ADAPTER_97BBCB_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _extract_message_payload: req {"envelope": obj, "ce_type": str?}
 * -> null | {"msg": obj, "space": obj, "format": "workspace_addons"
 *            | "native_chat_api" | "relay_flat"} */
json_t *w27_gchat_p1_extract_message_payload(const json_t *req);

/* _prepare_inbound: req {"envelope": obj, "dedup_seen": [str]?}
 * -> {"outcome": "ok"|"drop_unknown"|"drop_bot"|"drop_dup",
 *     "msg": obj?, "envelope": obj?} */
json_t *w27_gchat_p1_prepare_inbound(const json_t *req);

/* _thread_body: req {"text": any, "thread_id": any?} -> {"text": ..., "thread"?: {"name": ...}} */
json_t *w27_gchat_p1_thread_body(const json_t *req);

/* _create_kwargs: req {"chat_id": any, "body": obj} -> {"parent","body","messageReplyOption"?} */
json_t *w27_gchat_p1_create_kwargs(const json_t *req);

/* _mime_for_message_type: req {"mime": str?} -> "photo"|"audio"|"video"|"document" */
json_t *w27_gchat_p1_mime_for_message_type(const json_t *req);

/* _handle_pubsub_message routing: req {"ce_type": str, "envelope": obj,
 *   "bot_user_id": str|null?} ->
 * {"action": "membership_added"|"membership_removed"|"card_ack"|"message",
 *  "space_name": str|null, "bot_user_id": str|null} */
json_t *w27_gchat_p1_route_pubsub_event(const json_t *req);

/* _build_message_event shaping (attachments pre-downloaded):
 * req {"msg": obj, "envelope": obj, "counts": {chat: {thread: int}}?,
 *      "authorized": bool|null, "attachments": [{"path","mime"}]?} ->
 * {"chat_type","text","message_type","session_thread_id","prev_thread_count",
 *  "thread_cache": {"op":"set","thread":str}|{"op":"del"}|null,
 *  "last_sender": {"space":str,"email":str}|null,
 *  "media_urls": [str], "media_types": [str], "source": obj} */
json_t *w27_gchat_p1_build_message_event(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_97BBCB_P1_H */
