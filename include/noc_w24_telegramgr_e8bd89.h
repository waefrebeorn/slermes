/*
 * noc_w24_telegramgr_e8bd89.h - C surface for upstream
 * plugins/platforms/telegram/telegram_group_observe.py (gate w24_telegramgr_e8bd89).
 *
 * Observe mode aligns a triggered group turn with the chat's shared session.
 * The adapter (raw Telegram message, allowed-chat cache, shared source builder)
 * is a seam: its answers arrive as plain values, and what is ported here is
 * what those values decide -- whether the turn is rewritten at all, which
 * channel prompt the join produces, and how the sender is named.
 */
#ifndef W24_TELEGRAMGR_E8BD89_H
#define W24_TELEGRAMGR_E8BD89_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Every key of the attribution decision, as one object. */
#define W24_TELEGRAMGR_E8BD89_PLAN_KEYS 5

/* req: {observe_enabled, has_raw, is_group, allowed, chat_id, channel_prompt,
 *       observe_prompt, message_type, text, user_id, source_user_id,
 *       source_shared_session, attributed_text, shared_source_user_id,
 *       shared_source_shared_session} ->
 * {applied, shared_session, text, channel_prompt, source_user_id}: the fields
 * apply_group_observe_attribution() would leave on the event it returns. */
json_t *w24_telegramgr_e8bd89_attribution_plan(const json_t *req);

/* req: {kind, context_note} -> "<context_note> (not attached inline; to look
 * at it use <tool>)" with the tool chosen by the cached media kind. */
json_t *w24_telegramgr_e8bd89_observed_media_note(const json_t *req);

/* req: {user_id, source_user_id} -> ":from:<actor>" or "" when there is no
 * sender: the batch-key suffix that keeps two members' turns apart. */
json_t *w24_telegramgr_e8bd89_shared_sender_suffix(const json_t *req);

/* _OBSERVED_MEDIA_TOOL, verbatim. */
json_t *w24_telegramgr_e8bd89_constants(void);

#ifdef __cplusplus
}
#endif
#endif
