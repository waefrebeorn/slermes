/*
 * noc_w27_adapter_04abb5_p3.h - C surface for the Buzz adapter's inbound
 * shaping leaves (gate w27_adapter_04abb5_p3).
 *
 * Ported: the relay-frame disposition (EVENT / CLOSED / NOTICE), NIP-94
 * ``imeta`` acceptance (bounded count, HTTPS origin, SHA-256, size budget),
 * the attachment-rejection note, the pubkey -> display-name fallback and the
 * text/media/MessageType merge that _dispatch_message performs.
 * The WebSocket, the relay CLI and httpx downloads are seams.
 */
#ifndef NOC_W27_ADAPTER_04ABB5_P3_H
#define NOC_W27_ADAPTER_04ABB5_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {message, subscriptions, channel_state, membership_since} ->
 * {action, detail?, channel?, membership_since?, restricted?, state_left?}. */
json_t *w27_adapter_04abb5_ws_message(const json_t *in);

/* in: {event} -> {attachments: [...], rejected: n}. */
json_t *w27_adapter_04abb5_imeta_parse(const json_t *in);

/* in: {event} -> [attachment, ...]. */
json_t *w27_adapter_04abb5_imeta_attachments(const json_t *in);

/* in: {rejected} -> the fixed-width diagnostic note. */
json_t *w27_adapter_04abb5_rejection_note(const json_t *in);

/* in: {pubkey, cached, code, stdout} -> display name (never empty). */
json_t *w27_adapter_04abb5_user_name(const json_t *in);

/* in: {text, message_type, media_urls, media_types, localized, chat_id,
 * channel_names} -> {text, message_type, media_urls, media_types,
 * media_text_inlined, chat_name}. */
json_t *w27_adapter_04abb5_dispatch_shape(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
