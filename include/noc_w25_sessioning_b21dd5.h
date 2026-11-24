/*
 * noc_w25_sessioning_b21dd5.h - C surface for upstream
 * gateway/session_ingress_live_turn.py (gate w25_sessioning_b21dd5).
 *
 * The pure decision core of the relay delivery path: whether the chat's in-flight
 * work could consume a delivery, the replay key of a delivery, the bounded ledger
 * of deliveries already consumed here, and the early-out for commands and bot
 * peers.  The runner (turn registry, clarify / slash-confirm stores, the agent's
 * own turn) is a seam: every probe it answers is a fixed input here.
 */
#ifndef NOC_W25_SESSIONING_B21DD5_H
#define NOC_W25_SESSIONING_B21DD5_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deliveries consumed here are never committed; the FIFO's replay check needs
 * this ledger bounded (upstream _CONSUMED_MAX). */
#define W25_SESSIONING_B21DD5_CONSUMED_MAX 512

/* state: {running, clarify_pending, slash_pending, state:{update_prompt_pending}|null}
 * -> bool: may_answer_live_turn over the runner's own answers. */
json_t *w25_sessioning_b21dd5_may_answer(const json_t *state);

/* event: {message_id, profile, platform, chat_id} ->
 * null when the delivery has no message id, else [profile, platform, chat_id, message_id]. */
json_t *w25_sessioning_b21dd5_replay_key(const json_t *event);

/* req: {command, internal, is_bot, message_id, profile, platform, chat_id, reply,
 *       running, approval, preempt, consumed_before} ->
 * {consumed, consumer_reply, keys} for answers_live_turn. */
json_t *w25_sessioning_b21dd5_answers(const json_t *req);

/* req: {message_id, profile, platform, chat_id, consumed} -> bool for was_consumed. */
json_t *w25_sessioning_b21dd5_consumed(const json_t *req);

/* The consumed-ledger budget, as an object. */
json_t *w25_sessioning_b21dd5_constants(void);

#ifdef __cplusplus
}
#endif
#endif
