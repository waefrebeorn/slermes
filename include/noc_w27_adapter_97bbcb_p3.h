/*
 * noc_w27_adapter_97bbcb_p3.h - C surface for upstream
 * plugins/platforms/google_chat/adapter.py (gate w27_adapter_97bbcb_p3).
 *
 * Gate 3 of 4: outbound shaping.  Text chunking against the 4000-character
 * wall, thread-key resolution (metadata aliases -> reply_to -> last inbound
 * thread -> none), messages.patch masks, card bodies, attachment bodies and
 * their filename/mime fallbacks, the rate-limit counter and the typing-card
 * state machine.  Every HTTP call those bodies feed is a seam.
 */
#ifndef NOC_W27_ADAPTER_97BBCB_P3_H
#define NOC_W27_ADAPTER_97BBCB_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _chunk_text: req {"text": str} -> [str] */
json_t *w27_gchat_p3_chunk_text(const json_t *req);

/* _resolve_thread_id: req {"reply_to": str|null, "metadata": obj|null,
 *   "chat_id": str|null, "last_inbound_thread": {chat: thread}?} -> str|null */
json_t *w27_gchat_p3_resolve_thread_id(const json_t *req);

/* _patch_message shaping: req {"body": obj}
 * -> {"update_mask": str, "patch_body": obj} */
json_t *w27_gchat_p3_patch_shape(const json_t *req);

/* send_card body: req {"card": obj, "chat_id": str, "metadata": obj|null,
 *   "last_inbound_thread": obj?} -> {"body": obj, "thread_id": str|null} */
json_t *w27_gchat_p3_send_card_shape(const json_t *req);

/* _send_file body: req {"attachment_ref": any, "caption": str|null,
 *   "thread_id": str|null, "chat_id": any} -> {"body": obj, "create_kwargs": obj} */
json_t *w27_gchat_p3_attachment_body(const json_t *req);

/* _download_attachment filename/ext: req {"name": str} -> {"filename": str, "ext": str} */
json_t *w27_gchat_p3_media_filename(const json_t *req);

/* _send_file basename: req {"path": str, "override": str|null} -> str */
json_t *w27_gchat_p3_upload_basename(const json_t *req);

/* _note_rate_limit: req {"hits": int} -> {"count": int, "warn": bool} */
json_t *w27_gchat_p3_note_rate_limit(const json_t *req);

/* send_typing guard: req {"state": "absent"|"card"|"sentinel"|"inflight"}
 * -> "create"|"already"|"wait" */
json_t *w27_gchat_p3_typing_send_action(const json_t *req);

/* stop_typing guard: req {"state": "absent"|"card"|"sentinel"|"inflight"}
 * -> "pop"|"noop" */
json_t *w27_gchat_p3_typing_stop_action(const json_t *req);

/* _consume_typing_card_with_text: req {"state": ..., "error_status": int|null}
 * -> "none"|"patched"|"raise" */
json_t *w27_gchat_p3_typing_consume_action(const json_t *req);

/* get_chat_info shaping: req {"chat_id": str, "info": obj|null}
 * -> {"name": str, "type": "dm"|"group", "chat_id": str} */
json_t *w27_gchat_p3_chat_info_shape(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_97BBCB_P3_H */
