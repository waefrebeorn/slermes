/*
 * noc_w29_adapter_d36d4e_p3.h - C surface for the outbound message shaping of
 * upstream plugins/platforms/signal/adapter.py (gate w29_adapter_d36d4e_p3).
 *
 * Routing keys, send-result validation, the UTF-16 offset/style machinery used
 * by the formatted splitter, the formatted split itself, the sent-timestamp
 * LRU, the typing backoff, reaction targets/params, image error text and chat
 * info.  The HTTP RPC and the clock are seams: callers pass the response shape
 * and "now".
 */
#ifndef NOC_W29_ADAPTER_D36D4E_P3_H
#define NOC_W29_ADAPTER_D36D4E_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* {"account": ..., "groupId" | "recipient": [...]} */
json_t *w29_adapter_d36d4e_p3_with_target(const json_t *chat_id, const json_t *account);

/* {"success": bool, "error": str|null} */
json_t *w29_adapter_d36d4e_p3_validate_send_result(const json_t *result);

/* cumulative UTF-16 offsets of every codepoint boundary */
json_t *w29_adapter_d36d4e_p3_utf16_offsets(const json_t *text);

/* full-message start:len:TYPE styles -> chunk-local range list */
json_t *w29_adapter_d36d4e_p3_styles_for_chunk(const json_t *styles, const json_t *start, const json_t *end);

/* [[text, [styles]], ...] with "(i/n)" indicators when split */
json_t *w29_adapter_d36d4e_p3_split_signal_message(const json_t *text, const json_t *styles,
                                                   const json_t *max_length);

/* {"recent": [[key, ts], ...], "sent_cache": [keys]} */
json_t *w29_adapter_d36d4e_p3_track_sent_timestamp(const json_t *result, const json_t *recent,
                                                   const json_t *sent_cache, const json_t *now,
                                                   const json_t *ttl, const json_t *max_recent,
                                                   const json_t *max_sent);

/* seconds to skip typing after N consecutive failures, null under the threshold */
json_t *w29_adapter_d36d4e_p3_typing_skip_seconds(const json_t *fails);

/* [target_author, target_timestamp] or null */
json_t *w29_adapter_d36d4e_p3_extract_reaction_target(const json_t *raw_message);

/* sendReaction params routed to the chat (no UUID upgrade) */
json_t *w29_adapter_d36d4e_p3_reaction_params(const json_t *account, const json_t *chat_id,
                                              const json_t *emoji, const json_t *author,
                                              const json_t *timestamp, const json_t *remove);

/* send_image failure text: download (exc text) / missing / oversize (bytes) */
json_t *w29_adapter_d36d4e_p3_image_error_text(const json_t *reason, const json_t *detail);

/* {"name": ..., "type": "group"|"dm", "chat_id": ...} */
json_t *w29_adapter_d36d4e_p3_chat_info(const json_t *chat_id, const json_t *contact);

#ifdef __cplusplus
}
#endif
#endif
