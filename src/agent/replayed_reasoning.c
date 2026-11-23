/*
 * replayed_reasoning.c — C home for upstream Hermes agent/replayed_reasoning.py.
 *
 * Faithful C11 ports of the replayed encrypted-reasoning pricing helpers.
 * The provider-stamped token count (agent.message_metadata.stamped_reasoning_tokens)
 * is injected through the args object as "stamp"; ciphertext character counts use
 * the same per-issuer divisors as the Python module.
 *
 * PoP: w10_replayedre_d3ae9b_strip_opaque_replay_items @ agent/replayed_reasoning.py:strip_opaque_replay_items
 * PoP: w10_replayedre_d3ae9b_legacy_tokens @ agent/replayed_reasoning.py:_legacy_tokens
 * PoP: w10_replayedre_d3ae9b_codex_reasoning_replay_tokens @ agent/replayed_reasoning.py:codex_reasoning_replay_tokens
 * PoP: w10_replayedre_d3ae9b_signed_thinking_replay_tokens @ agent/replayed_reasoning.py:signed_thinking_replay_tokens
 */
#include "noc_w10_replayedre_d3ae9b.h"

#include <string.h>

#define DEFAULT_LEGACY_CPT 13.5
#define XAI_LEGACY_CPT 1.6

/* `_legacy_tokens` for one item: int(len(encrypted_content) / chars_per_token). */
static long long legacy_one(json_t *item)
{
    if (!json_is_object(item)) return 0;
    json_t *enc = json_obj_get(item, "encrypted_content");
    if (!json_is_string(enc)) return 0;
    const char *s = json_string_value(enc);
    if (!s || !*s) return 0;
    const char *issuer = "";
    json_t *iss = json_obj_get(item, "_issuer_kind");
    if (json_is_string(iss)) issuer = json_string_value(iss);
    double cpt = (issuer && !strcmp(issuer, "xai_responses")) ? XAI_LEGACY_CPT : DEFAULT_LEGACY_CPT;
    return (long long)(((double)strlen(s)) / cpt);
}

static long long legacy_sum(json_t *items)
{
    long long total = 0;
    if (!json_is_array(items)) return 0;
    for (size_t i = 0; i < json_len(items); i++)
        total += legacy_one(json_get(items, i));
    return total;
}

static long long stamp_value(json_t *a)
{
    json_t *s = a ? json_obj_get(a, "stamp") : NULL;
    if (json_is_number(s)) return (long long)json_number_value(s);
    return 0;
}

/* PoP: w10_replayedre_d3ae9b_strip_opaque_replay_items @ agent/replayed_reasoning.py:strip_opaque_replay_items */
json_t *w10_replayedre_d3ae9b_strip_opaque_replay_items(json_t *a)
{
    json_t *items = a ? json_obj_get(a, "items") : NULL;
    if (!json_is_array(items))
        return items ? json_copy(items) : json_null();
    json_t *out = json_array();
    for (size_t i = 0; i < json_len(items); i++) {
        json_t *item = json_get(items, i);
        if (!json_is_object(item)) {
            json_append(out, json_copy(item));
            continue;
        }
        json_t *d = json_object();
        for (size_t j = 0; j < json_object_size(item); j++) {
            const char *k = json_object_get_key_at(item, j);
            json_t *v = json_object_get_at(item, j);
            if (!k) continue;
            if (!strcmp(k, "encrypted_content"))
                json_set(d, k, json_string(""));
            else
                json_set(d, k, json_copy(v));
        }
        json_append(out, d);
    }
    return out;
}

/* PoP: w10_replayedre_d3ae9b_legacy_tokens @ agent/replayed_reasoning.py:_legacy_tokens */
json_t *w10_replayedre_d3ae9b_legacy_tokens(json_t *a)
{
    json_t *items = a ? json_obj_get(a, "items") : NULL;
    return json_number((double)legacy_sum(items));
}

/* PoP: w10_replayedre_d3ae9b_codex_reasoning_replay_tokens @ agent/replayed_reasoning.py:codex_reasoning_replay_tokens */
json_t *w10_replayedre_d3ae9b_codex_reasoning_replay_tokens(json_t *a)
{
    json_t *msg = a ? json_obj_get(a, "msg") : NULL;
    if (json_is_object(msg)) {
        json_t *t = json_obj_get(msg, "type");
        if (json_is_string(t) && !strcmp(json_string_value(t), "reasoning"))
            return json_number((double)legacy_one(msg));

        json_t *role = json_obj_get(msg, "role");
        if (json_is_string(role) && !strcmp(json_string_value(role), "assistant")) {
            json_t *items = json_obj_get(msg, "codex_reasoning_items");
            if (json_is_array(items)) {
                json_t *reasoning = json_array();
                for (size_t i = 0; i < json_len(items); i++) {
                    json_t *it = json_get(items, i);
                    if (!json_is_object(it)) continue;
                    json_t *ty = json_obj_get(it, "type");
                    if (json_is_string(ty) && !strcmp(json_string_value(ty), "compaction"))
                        continue;
                    json_append(reasoning, json_copy(it));
                }
                long long lt = legacy_sum(reasoning);
                json_free(reasoning);
                if (!lt) return json_number(0);
                long long st = stamp_value(a);
                return json_number((double)(st ? st : lt));
            }
        }
    }
    return json_number(0);
}

/* PoP: w10_replayedre_d3ae9b_signed_thinking_replay_tokens @ agent/replayed_reasoning.py:signed_thinking_replay_tokens */
json_t *w10_replayedre_d3ae9b_signed_thinking_replay_tokens(json_t *a)
{
    long long st = stamp_value(a);
    long long readable = 0;
    json_t *r = a ? json_obj_get(a, "readable_tokens") : NULL;
    if (json_is_number(r)) readable = (long long)json_number_value(r);
    return json_number((double)(st ? st : readable));
}
