#ifndef NOC_W22_TELEGRAMSP_8C7E95_H
#define NOC_W22_TELEGRAMSP_8C7E95_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/telegram_split.py (gate w22_telegramsp_8c7e95).
 *
 * The long-reply handoff decision and the MarkdownV2 strip a split chunk's
 * plain-text fallback uses are pure string work and are ported here:
 *
 *   - _strip_mdv2: unescape MarkdownV2 punctuation, then unwrap **bold**,
 *     *bold*, _italic_ (word-bounded so snake_case survives), ~strike~ and
 *     ||spoiler|| -- six sequential left-to-right passes, exactly as the six
 *     re.sub calls run.
 *   - quotable_text: the sent-text index entry for a chunk. Code regions
 *     (``` fenced, or `inline`) stay verbatim; everything between them is
 *     _strip_mdv2'd. With mdv2=True the code region unescapes only \` and \\.
 *   - split_reply_anchor: `reply_to` for a reply sent as `parts` messages,
 *     dropped for a split reply when `extra.anchor_multipart_replies` is false.
 *     The adapter is the seam: its attribute and its DM-topic predicate are
 *     injected, and the short-circuit order is visible in `dm_checked`.
 *
 * `send_reply_document` and `reply_message_count` need a live adapter (a
 * TDLib handle, the rich-part pipeline) and stay upstream.
 */

/* PoP: w22_telegramsp_8c7e95_strip_mdv2 @ plugins/platforms/telegram/telegram_split.py:_strip_mdv2
 * Plain-text fallback for a MarkdownV2 chunk. Caller frees the result. */
char *w22_telegramsp_8c7e95_strip_mdv2(const char *text);

/* PoP: w22_telegramsp_8c7e95_quotable_text @ plugins/platforms/telegram/telegram_split.py:quotable_text
 * The sent-text index entry for a chunk: _strip_mdv2 outside code regions,
 * code kept verbatim (unescaped only with mdv2=1). Caller frees the result. */
char *w22_telegramsp_8c7e95_quotable_text(const char *text, int mdv2);

/* PoP: w22_telegramsp_8c7e95_anchor @ plugins/platforms/telegram/telegram_split.py:split_reply_anchor
 * {"reply_to": <reply_to, or null>} plus sampling evidence:
 *   "dm_checked": 1 when `_is_private_dm_topic_send` was actually called.
 * `anchor_multipart` is the adapter attribute (-1 = absent, which reads as
 * True); the DM predicate is injected as `dm_topic`. */
json_t *w22_telegramsp_8c7e95_anchor(int anchor_multipart, int parts, int dm_topic,
                                     const json_t *reply_to);

/* PoP: w22_telegramsp_8c7e95_reply_file_max @ plugins/platforms/telegram/telegram_split.py:REPLY_FILE_MAX_BYTES
 * Telegram's sendDocument cap is 50 MB; each reply file stays well inside it. */
#define W22_TELEGRAMSP_8C7E95_REPLY_FILE_MAX_BYTES 47185920LL

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_TELEGRAMSP_8C7E95_H */
