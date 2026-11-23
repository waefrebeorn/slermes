/*
 * turn_usage.c -- faithful C11 port of upstream Hermes agent/turn_usage.py.
 *
 * PoP: w6_turnusage_47b7c5_add_session_usage @ agent/turn_usage.py:add_session_usage
 * PoP: w6_turnusage_47b7c5_with_write_tier @ agent/turn_usage.py:with_write_tier
 */
#include "noc_w6_turnusage_47b7c5.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_i64(json_t *obj, const char *key, long long v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", v);
    json_set(obj, key, json_string(buf));
}

/* PoP: w6_turnusage_47b7c5_add_session_usage @ agent/turn_usage.py:add_session_usage */
json_t *w6_turnusage_47b7c5_add_session_usage(
    long long sp, long long sc, long long st, long long si, long long so,
    long long sr, long long sw, long long sre,
    long long prompt_tokens, long long completion_tokens, long long total_tokens,
    long long input_tokens, long long output_tokens, long long cache_read_tokens,
    long long cache_write_tokens, long long reasoning_tokens)
{
    json_t *out = json_object();
    if (!out) return NULL;
    set_i64(out, "session_prompt_tokens",    sp + prompt_tokens);
    set_i64(out, "session_completion_tokens", sc + completion_tokens);
    set_i64(out, "session_total_tokens",      st + total_tokens);
    set_i64(out, "session_input_tokens",      si + input_tokens);
    set_i64(out, "session_output_tokens",     so + output_tokens);
    set_i64(out, "session_cache_read_tokens", sr + cache_read_tokens);
    set_i64(out, "session_cache_write_tokens", sw + cache_write_tokens);
    set_i64(out, "session_reasoning_tokens",  sre + reasoning_tokens);
    return out;
}

/* PoP: w6_turnusage_47b7c5_with_write_tier @ agent/turn_usage.py:with_write_tier */
json_t *w6_turnusage_47b7c5_with_write_tier(const char *ttl_json,
                                            long long cache_write_tokens,
                                            const char *tier)
{
    json_t *out = json_array();
    if (!out) return NULL;
    json_t *existing = ttl_json ? json_parse(ttl_json, NULL) : NULL;

    /* Python: if usage.cache_write_by_ttl (truthy) OR not cache_write_tokens
     * OR tier != "1h" -> return usage unchanged. */
    int truthy = existing && existing->type == JSON_ARRAY && existing->c.count > 0;
    if (truthy || !cache_write_tokens || !tier || strcmp(tier, "1h") != 0) {
        if (existing) {
            for (size_t i = 0; i < existing->c.count; i++)
                json_append(out, json_copy(existing->c.items[i]));
        }
        if (existing) json_free(existing);
        return out;
    }

    /* replace(usage, cache_write_by_ttl=(("1h", cache_write_tokens),)) */
    json_t *pair = json_array();
    json_append(pair, json_string("1h"));
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", cache_write_tokens);
    json_append(pair, json_string(buf));
    json_append(out, pair);
    if (existing) json_free(existing);
    return out;
}
