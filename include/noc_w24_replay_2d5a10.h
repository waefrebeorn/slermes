/*
 * noc_w24_replay_2d5a10.h - C surface for upstream
 * agent/prompt_cache_lifecycle/replay.py (gate w24_replay_2d5a10).
 *
 * What a cache keep-alive may replay: whether the request carries
 * ``cache_control`` markers, whether a bounded replay leaves the cache entry
 * untouched, the smallest output cap the model accepts, the rewritten Chat
 * Completions body, and whether a given client honours that cap.
 *
 * The seams are the SDK: ``replay_cap_honoured`` takes a client descriptor
 * instead of an object, and ``client_kind``/``sdk`` say what
 * ``isinstance(client, _sdk_client_types(api_mode))`` would say.  No client is
 * ever constructed here.
 */
#ifndef W24_REPLAY_2D5A10_H
#define W24_REPLAY_2D5A10_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* True when system/tools/messages carries a ``cache_control`` marker. */
json_t *w24_replay_2d5a10_has_cache_markers(const json_t *api_kwargs);

/* Whether replaying with the small output cap leaves the cache entry intact. */
json_t *w24_replay_2d5a10_is_replayable(const json_t *api_kwargs);

/* 16 for the OpenAI GPT-5 family and o-series, else 1. */
json_t *w24_replay_2d5a10_output_cap(const json_t *model);

/* The Chat Completions keep-alive body: same prompt, smallest cap, no output
 * cap hidden in ``extra_body``, never streamed. */
json_t *w24_replay_2d5a10_bounded_chat_replay(const json_t *api_kwargs, const json_t *model);

/* client: {"kind": "openai"|"anthropic"|"anthropic_bedrock"|"anthropic_vertex"|...,
 *          "effective": <num>|null,
 *          "sdk": {"openai": bool, "anthropic": bool}} */
json_t *w24_replay_2d5a10_cap_honoured(const json_t *client, const json_t *api_kwargs,
                                       const json_t *model, const json_t *api_mode);

/* agent.anthropic_adapter._supports_adaptive_thinking, the predicate
 * is_replayable consults for Claude behind a reasoning gateway. */
json_t *w24_replay_2d5a10_adaptive(const json_t *model);

/* The literal sets/patterns this module is tuned with. */
json_t *w24_replay_2d5a10_constants(void);

#ifdef __cplusplus
}
#endif
#endif
