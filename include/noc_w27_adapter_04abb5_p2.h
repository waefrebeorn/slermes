/*
 * noc_w27_adapter_04abb5_p2.h - C surface for the Buzz adapter's CLI
 * plumbing and send-shaping leaves (gate w27_adapter_04abb5_p2).
 *
 * Ported: the member/recent-traffic mention candidates, the profile display
 * name resolver, the @Name -> pubkey matcher, the bounded send-recovery
 * ladder, the SendResult shaping of a CLI receipt, get_chat_info, the
 * WebSocket URL derivation and the REQ subscription filters.
 * The buzz subprocess, the relay socket and the profile CLI are seams.
 */
#ifndef NOC_W27_ADAPTER_04ABB5_P2_H
#define NOC_W27_ADAPTER_04ABB5_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {members: [...], messages: [...]} -> ordered unique pubkeys. */
json_t *w27_adapter_04abb5_member_candidates(const json_t *in);

/* in: {profiles: [...]} -> display name ("" when none). */
json_t *w27_adapter_04abb5_profile_name(const json_t *in);

/* in: {content, members: [{pubkey,name}], self_pubkey} -> [pubkey]. */
json_t *w27_adapter_04abb5_mention_pubkeys(const json_t *in);

/* in: {args, content, mention_pubkeys, self_pubkey, script: [[rc,out,err]]}
 * -> {calls: [[argv]], inputs: [text], rc, out, err}. */
json_t *w27_adapter_04abb5_send_ladder(const json_t *in);

/* in: {code, out, err, redact} -> {success, message_id, error, retryable}. */
json_t *w27_adapter_04abb5_send_result(const json_t *in);

/* in: {chat_id, names, state, cli_path, channel_reply} ->
 * {name, type, chat_id}. */
json_t *w27_adapter_04abb5_chat_info(const json_t *in);

/* in: {relay_url} -> {ok: url} or {error: detail}. */
json_t *w27_adapter_04abb5_websocket_url(const json_t *in);

/* in: {channel_id, state} -> the REQ filter object. */
json_t *w27_adapter_04abb5_channel_subscription(const json_t *in);

/* in: {channel_state, restricted, self_pubkey, membership_since} ->
 * {subs: [[sub_id, channel|null]], sends: [[sub_id, filter]]}. */
json_t *w27_adapter_04abb5_subscriptions(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
