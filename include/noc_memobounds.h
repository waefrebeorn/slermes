#ifndef NOC_MEMOBOUNDS_H
#define NOC_MEMOBOUNDS_H
/*
 * Public C API for the port of agent/model_metadata_memo_bounds.py
 * (gate memobounds). All exported symbols are module-prefixed.
 */
#include "hermes_json.h"

/*
 * remember_endpoint_models decision core. The live function mutates module
 * dicts under a lock; the caller supplies their contents (values/stamps/ttl/now).
 * Semantics: drop_expired(values, stamps, ttl, now) then values[memo_key]=cache.
 * Returns the resulting values dict (caller frees).
 */
json_t *memobounds_remember_endpoint_models(const json_t *values,
        const json_t *stamps, double ttl, double now,
        const char *memo_key, const json_t *cache_value);

/*
 * bound_codex_caches decision core. Semantics:
 *   evict_oldest(context, cache_key, lambda e: e[1], limit, sibling)
 * Returns {"context": <post table>, "max_context": <post sibling>} (caller frees).
 */
json_t *memobounds_bound_codex_caches(const json_t *context_cache,
        const char *cache_key, int limit, const json_t *max_context_cache);

#endif /* NOC_MEMOBOUNDS_H */
