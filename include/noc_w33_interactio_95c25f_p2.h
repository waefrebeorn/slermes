/*
 * noc_w33_interactio_95c25f_p2.h - C surface for upstream
 * plugins/model-providers/gemini/interactions.py, RESPONSE side
 * (gate w33_interactio_95c25f_p2).
 *
 * The SSE reader and the HTTP client are seams: this file starts from the
 * already-decoded InteractionSseEvent payloads and the finished interaction
 * bodies. What it owns is the pure shaping -- Usage -> OpenAI usage (single and
 * multi-hop), Interaction.status -> finish_reason, in-band google.rpc error ->
 * GeminiAPIError fields, one interaction (or every hop of a long decode) ->
 * one chat.completion, and the per-turn stream state machine.
 */
#ifndef NOC_W33_INTERACTIO_95C25F_P2_H
#define NOC_W33_INTERACTIO_95C25F_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w33_interactio_95c25f_p2_usage @ plugins/model-providers/gemini/interactions.py:usage_namespace
 * Interactions Usage -> OpenAI usage object. */
json_t *w33_interactio_95c25f_p2_usage(const json_t *usage);

/* PoP: w33_interactio_95c25f_p2_merge_usage @ plugins/model-providers/gemini/interactions.py:merge_usage
 * Sum every hop's counts; initial_* is hop 1's prompt (the context size). */
json_t *w33_interactio_95c25f_p2_merge_usage(const json_t *usages);

/* PoP: w33_interactio_95c25f_p2_finish_reason @ plugins/model-providers/gemini/interactions.py:_finish_reason
 * spec: {status, has_tool_calls} -> finish_reason. */
json_t *w33_interactio_95c25f_p2_finish_reason(const json_t *spec);

/* PoP: w33_interactio_95c25f_p2_interaction_error @ plugins/model-providers/gemini/interactions.py:interaction_error
 * spec: {errors, fallback?} -> {"message", "code", "status_code", "details"}. */
json_t *w33_interactio_95c25f_p2_interaction_error(const json_t *spec);

/* PoP: w33_interactio_95c25f_p2_translate @ plugins/model-providers/gemini/interactions.py:translate_interaction
 * spec: {interactions, model} -> one chat.completion, or {"__raise__": error} when the
 * last hop failed. */
json_t *w33_interactio_95c25f_p2_translate(const json_t *spec);

/* PoP: w33_interactio_95c25f_p2_stream @ plugins/model-providers/gemini/interactions.py:translate_stream_event
 * spec: {model, script} where script items are {"event": <sse event>} or
 * {"op": "next_hop"|"close_dropped_hop"|"finish_ceiling"} ->
 * {"chunks": [...], "state": {...}} or {"__raise__": error}. */
json_t *w33_interactio_95c25f_p2_stream(const json_t *spec);

/* PoP: w33_interactio_95c25f_p2_event_token @ plugins/model-providers/gemini/interactions.py:event_token
 * The continuation token a checkpoint or hop-end event carries, else null. */
json_t *w33_interactio_95c25f_p2_event_token(const json_t *event);

/* PoP: w33_interactio_95c25f_p2_finish_ceiling @ plugins/model-providers/gemini/interactions.py:finish_ceiling
 * spec: {model, hop_usage} -> the truncated finish chunk with merged usage. */
json_t *w33_interactio_95c25f_p2_finish_ceiling(const json_t *spec);

/* PoP: w33_interactio_95c25f_p2_iter_events @ plugins/model-providers/gemini/interactions.py:iter_events
 * Filter decoded payloads down to the InteractionSseEvent ones. */
json_t *w33_interactio_95c25f_p2_iter_events(const json_t *lines);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W33_INTERACTIO_95C25F_P2_H */
