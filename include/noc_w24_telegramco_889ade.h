/*
 * noc_w24_telegramco_889ade.h - C surface for upstream
 * plugins/platforms/telegram/telegram_context.py (gate w24_telegramco_889ade).
 *
 * Group addressing policy kept out of the Telegram adapter facade: whether a
 * group message names another participant, whether it opens with one, whether an
 * admitted message needs a reply (True/False/None = visible-silence fallback),
 * the trigger text after our own handle is stripped, and the session-stable
 * identity line.  The forum-topic reply anchor rule (``reply_to_message`` is the
 * topic root, not a reply to a person) lives here too.
 *
 * Seams: the adapter is a value bag.  ``_bot``/``_current_bot_username`` are
 * ``adapter.bot``/``adapter.username``; the three per-message adapter questions
 * (``_is_reply_to_bot``, ``_message_mentions_bot``,
 * ``_message_matches_mention_patterns``) are precomputed booleans on the
 * message; entity offsets are code-point indices of the message text (the
 * fixtures stay BMP, where Telegram's UTF-16 offsets and code points agree).
 */
#ifndef W24_TELEGRAMCO_889ADE_H
#define W24_TELEGRAMCO_889ADE_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* True when any mention/text_mention entity names someone other than us. */
json_t *w24_telegramco_889ade_mentions_other(const json_t *adapter, const json_t *msg);

/* True when the message OPENS with a mention of another participant. */
json_t *w24_telegramco_889ade_starts_with_other_mention(const json_t *adapter,
                                                        const json_t *msg);

/* The message this one replies to (its message_id), or null -- a forum topic
 * root anchor is not a reply. */
json_t *w24_telegramco_889ade_quoted_reply(const json_t *msg);

/* chat_type + message + adapter -> true / false / null (silence fallback). */
json_t *w24_telegramco_889ade_group_reply_expected(const json_t *chat_type,
                                                   const json_t *adapter,
                                                   const json_t *msg);

/* Our own @handle is stripped only when no other participant is named. */
json_t *w24_telegramco_889ade_group_trigger_text(const json_t *adapter,
                                                 const json_t *msg,
                                                 const json_t *text);

/* channel_prompt plus the session-stable identity line (group with a bot only). */
json_t *w24_telegramco_889ade_group_identity_prompt(const json_t *adapter,
                                                    const json_t *msg,
                                                    const json_t *channel_prompt);

/* The literal identity sentence the prompt appends. */
json_t *w24_telegramco_889ade_constants(void);

#ifdef __cplusplus
}
#endif
#endif
