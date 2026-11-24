/*
 * noc_w24_anthropict_2a0525.h - C surface for upstream
 * agent/anthropic_thinking_policy.py (gate w24_anthropict_2a0525).
 *
 * The one place that decides whether prior assistant ``thinking`` blocks are
 * kept for a Claude model, and which replay contract an Anthropic-Messages
 * endpoint follows (native / minimax / kimi / deepseek / third_party).
 *
 * Seams: the endpoint predicates are ported here as pure URL/text logic; the
 * ``NOUS_INFERENCE_BASE_URL`` host override (an operator env var) is modelled as
 * unset, and nothing touches the network, credentials or the message converter.
 */
#ifndef W24_ANTHROPICT_2A0525_H
#define W24_ANTHROPICT_2A0525_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* "claude[-_.]<family>[-_.]<major>[.<minor>]" -> [family, [major, minor]] or
 * null when the id is not a string or carries no version. */
json_t *w24_anthropict_2a0525_claude_family_version(const json_t *model);

/* true when Anthropic keeps prior assistant thinking in model-visible context. */
json_t *w24_anthropict_2a0525_model_preserves_prior_thinking(const json_t *model);

/* "kimi" | "deepseek" | "native" | "minimax" | "third_party" for a base URL. */
json_t *w24_anthropict_2a0525_thinking_route(const json_t *base_url, const json_t *model);

/* true when the route replays signed thinking on turns before the tool loop. */
json_t *w24_anthropict_2a0525_route_replays_prior_thinking(const json_t *route,
                                                           const json_t *model);

/* route(base_url, model) + model capability, in one decision. */
json_t *w24_anthropict_2a0525_native_preserves_prior_thinking(const json_t *base_url,
                                                              const json_t *model);

/* Literal tables: signed routes, keep-from versions and the two regexes. */
json_t *w24_anthropict_2a0525_constants(void);

#ifdef __cplusplus
}
#endif
#endif
