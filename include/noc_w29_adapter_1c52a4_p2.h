/*
 * noc_w29_adapter_1c52a4_p2.h - C surface for upstream
 * plugins/platforms/teams/adapter.py (gate w29_adapter_1c52a4_p2).
 *
 * The message/card shaping half of the Teams adapter: @mention detection, the
 * Adaptive Card body blocks of an approval prompt, the media-kind precedence
 * that decides the dispatched MessageType, the inbound attachment skip
 * predicate and file-attachment filename, and the threaded-reply predicate.
 *
 * The seams are the SDK types and the i18n catalog: TextBlock/AdaptiveCard stay
 * in the caller and the resolved catalog strings arrive as arguments.
 */
#ifndef NOC_W29_ADAPTER_1C52A4_P2_H
#define NOC_W29_ADAPTER_1C52A4_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* TeamsAdapter._activity_mentions_bot: a mention entity pointing at one of
 * bot_ids, else (no mention entities at all) the rendered "<at>" tag. */
int w29_adapter_1c52a4_p2_mentions_bot(const json_t *entities,
                                       const char *text,
                                       const json_t *bot_ids);

/* _approval_body(cmd, desc, always) -> the TextBlock kwargs list:
 * [{"text","wrap","weight"}, {"text","wrap"}, {"text","wrap","isSubtle"}]. */
json_t *w29_adapter_1c52a4_p2_approval_body(const char *cmd, const char *desc,
                                            int always, const char *header,
                                            const char *reason_label);

/* Media-kind precedence from _on_message: document > image > video > audio,
 * else "text" (MessageType values). */
json_t *w29_adapter_1c52a4_p2_media_type(const json_t *kinds);

/* _cache_attachment skip predicate: body-mirror text/plain and text/html without
 * a URL, and application/vnd.microsoft.card.* payloads, are never downloaded. */
int w29_adapter_1c52a4_p2_attachment_skip(const char *content_type,
                                          const char *content_url);

/* _cache_attachment file-download filename: the attachment name, else
 * document.<fileType>, else "document". */
json_t *w29_adapter_1c52a4_p2_document_filename(const char *name,
                                                const json_t *content);

/* send() reply predicate: reply_to truthy, all digits, and not "0". */
int w29_adapter_1c52a4_p2_threaded_reply(const char *reply_to);

#ifdef __cplusplus
}
#endif
#endif
