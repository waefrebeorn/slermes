/*
 * noc_w27_adapter_878564_p2.h - C surface for upstream
 * plugins/platforms/photon/adapter.py (gate w27_adapter_878564_p2).
 *
 * Gate 2 of 5: inbound content PREDICATES and LABELS — voice detection, media
 * typing, group-item flattening, rich-link preview detection/labels, reply
 * unwrapping and the mention-gate text.  None of these decode bytes or touch a
 * cache; they decide from the payload shape alone.
 */
#ifndef NOC_W27_ADAPTER_878564_P2_H
#define NOC_W27_ADAPTER_878564_P2_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _is_voice_payload: type=="voice", or an attachment named *.caf / MIME
 * audio/x-caf. */
int w27_adapter_878564_p2_is_voice_payload(const json_t *payload);

/* _cached_media_type: the MIME a cached voice payload must claim.
 * Caller frees. */
char *w27_adapter_878564_p2_cached_media_type(const json_t *payload, const char *mime,
                                              int is_voice);

/* _attachment_message_type: image/→photo, video/→video, audio/→audio, else
 * document (the MessageType value string). */
const char *w27_adapter_878564_p2_attachment_message_type(const char *mime);

/* _group_item_contents: the dict `content` of every well-formed group item. */
json_t *w27_adapter_878564_p2_group_item_contents(const json_t *content);

/* _richlink_url_from_content: first URL-only candidate in a text/richlink item,
 * or in a group's items.  Caller frees. */
char *w27_adapter_878564_p2_richlink_url_from_content(const json_t *content);

/* _is_richlink_preview_attachment: attachment whose name/id carries the
 * .pluginpayloadattachment marker. */
int w27_adapter_878564_p2_is_richlink_preview_attachment(const json_t *payload);

/* _richlink_preview_label: name or id or "(unnamed)"; group joins with ", ",
 * empty group is "(group)"; anything else "(unknown)".  Caller frees. */
char *w27_adapter_878564_p2_richlink_preview_label(const json_t *content);

/* _is_richlink_preview_content: a preview attachment, or a non-empty group made
 * ONLY of preview attachments. */
int w27_adapter_878564_p2_is_richlink_preview_content(const json_t *content);

/* _reply_inner: the inner content of a reply envelope, else the content. */
json_t *w27_adapter_878564_p2_reply_inner(const json_t *content);

/* _carries_voice: a voice payload, alone or anywhere inside a group envelope. */
int w27_adapter_878564_p2_carries_voice(const json_t *content);

/* _mention_gate_text: user-typed text only — no attachment bytes decoded, no
 * media cached.  Caller frees. */
char *w27_adapter_878564_p2_mention_gate_text(const json_t *content);

#ifdef __cplusplus
}
#endif
#endif
