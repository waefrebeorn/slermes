/*
 * noc_w27_adapter_e9cb6b_p4.h - C surface for the p4 slice of upstream
 * plugins/platforms/matrix/adapter.py (gate w27_adapter_e9cb6b_p4).
 *
 * Inbound classification and policy: self/bridge sender filters, MediaType
 * mapping, the voice-park predicate, the bot-mention shortcut from event
 * content, invite DM signalling, prompt expiry, authorization/reaction policy,
 * state-event value extraction, room server/identity derivation and presence
 * validation.  The event objects, the state store and the clock are seams.
 */
#ifndef NOC_W27_ADAPTER_E9CB6B_P4_H
#define NOC_W27_ADAPTER_E9CB6B_P4_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* is the sender the bot itself (case-insensitive; no known id => true). */
json_t *w27_adapter_is_self_sender(const json_t *user_id, const json_t *sender);

/* appservice/bridge/malformed identity. */
json_t *w27_adapter_is_system_or_bridge_sender(const json_t *sender);

/* (msgtype, mimetype, content) -> [message_type, mime, is_voice]. */
json_t *w27_adapter_classify_inbound_media(const json_t *msgtype, const json_t *mimetype,
                                           const json_t *content);

/* request {room_id, body, content, relates_to, mention_claimed, require_mention,
 *          free_rooms, allowed_rooms, bot_threads, user_id} -> may park. */
json_t *w27_adapter_voice_may_park(const json_t *req);

/* (user_id, body, content) -> content mentions the bot. */
json_t *w27_adapter_content_mentions_bot(const json_t *user_id, const json_t *body,
                                         const json_t *content);

/* (user_id, invited_room) -> [is_direct, inviter]. */
json_t *w27_adapter_extract_invite_dm_signal(const json_t *user_id, const json_t *invited_room);

/* (expires_at, now) -> now > expires_at, false when expires_at is null. */
json_t *w27_adapter_matrix_prompt_expired(const json_t *expires_at, const json_t *now);

/* (user_id, allow_all, allowed_users) -> authorized. */
json_t *w27_adapter_is_authorized_user(const json_t *user_id, const json_t *allow_all,
                                       const json_t *allowed_users);

/* (sender, requester, require_sender) -> the reactor may act. */
json_t *w27_adapter_reactor_allowed(const json_t *sender, const json_t *requester,
                                    const json_t *require_sender);

/* (event, key) -> the first truthy top-level then content value as a string. */
json_t *w27_adapter_state_event_value(const json_t *event, const json_t *key);

/* room_id -> server part, null when there is none. */
json_t *w27_adapter_room_server_name(const json_t *room_id);

/* request {room_id, room_name, canonical_alias, member_count, is_direct} ->
 * {server_name, display_name, has_explicit_name, chat_type, conflict}. */
json_t *w27_adapter_room_identity(const json_t *req);

/* presence state in {online, offline, unavailable}. */
json_t *w27_adapter_presence_valid(const json_t *state);

#ifdef __cplusplus
}
#endif
#endif
