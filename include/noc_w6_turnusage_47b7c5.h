#ifndef NOC_W6_TURNUSAGE_47B7C5_H
#define NOC_W6_TURNUSAGE_47B7C5_H

/* Ports of upstream Hermes agent/turn_usage.py pure helpers.
 * Token counts are int64: callers pass/receive them as DECIMAL STRINGS so no
 * double rounding occurs; the C side uses strtoll/lltoa internally. */
#include "hermes_json.h"

/* add_session_usage: fold one reply's token counts into the eight running
 * session counters. Returns an object with the eight updated counters, each
 * a decimal string, keyed as in the Python attributes (without the
 * `session_` prefix on the first three where noted). */
json_t *w6_turnusage_47b7c5_add_session_usage(
    long long session_prompt_tokens, long long session_completion_tokens,
    long long session_total_tokens, long long session_input_tokens,
    long long session_output_tokens, long long session_cache_read_tokens,
    long long session_cache_write_tokens, long long session_reasoning_tokens,
    long long prompt_tokens, long long completion_tokens, long long total_tokens,
    long long input_tokens, long long output_tokens, long long cache_read_tokens,
    long long cache_write_tokens, long long reasoning_tokens);

/* with_write_tier(usage, tier): returns the resulting cache_write_by_ttl as a
 * JSON array of [ttl, tokens] pairs (tokens as decimal strings). ttl_json is a
 * JSON array of existing [ttl, tokens] pairs (empty array == falsey). tier is
 * a string or NULL. */
json_t *w6_turnusage_47b7c5_with_write_tier(const char *ttl_json,
                                            long long cache_write_tokens,
                                            const char *tier);

#endif /* NOC_W6_TURNUSAGE_47B7C5_H */
