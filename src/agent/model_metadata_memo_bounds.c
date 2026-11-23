/*
 * model_metadata_memo_bounds.c — C port of upstream Hermes
 * agent/model_metadata_memo_bounds.py.
 * PoP: memobounds_remember_endpoint_models @ agent/model_metadata_memo_bounds.py:remember_endpoint_models
 * PoP: memobounds_bound_codex_caches @ agent/model_metadata_memo_bounds.py:bound_codex_caches
 *
 * The real helpers are agent.cache_bounds.drop_expired / evict_oldest. Both are
 * reproduced faithfully: expired stamps default to 0; eviction drops the
 * oldest-stamped key from the cache AND every sibling; an existing key or a
 * cache below the limit short-circuits. The module-level memo dicts and the
 * 128-entry constant are supplied by the caller (header).
 */
#include "noc_memobounds.h"

#include <stdlib.h>

/* drop_expired: pop keys whose now - stamps.get(k, 0) >= ttl. */
static json_t *drop_expired(const json_t *values, const json_t *stamps,
                            double ttl, double now) {
    json_t *out = values && values->type == JSON_OBJECT
                ? json_copy(values) : json_object();
    if (!values || values->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(values); i++) {
        const char *k = json_object_get_key_at(values, i);
        if (!k) continue;
        json_t *st = json_obj_get(stamps, k);
        double stamp = (st && st->type == JSON_NUMBER) ? st->num_val : 0.0;
        if (now - stamp >= ttl) json_obj_del(out, k);
    }
    return out;
}

json_t *memobounds_remember_endpoint_models(const json_t *values,
        const json_t *stamps, double ttl, double now,
        const char *memo_key, const json_t *cache_value) {
    json_t *out = drop_expired(values, stamps, ttl, now);
    if (out && out->type == JSON_OBJECT && memo_key)
        json_set(out, memo_key, cache_value ? json_copy(cache_value) : json_null());
    return out;
}

/* stamp_of(entry) = entry[1] (the timestamp slot of a paired cache entry). */
static double entry_stamp(const json_t *entry) {
    if (entry && entry->type == JSON_ARRAY && entry->c.count > 1 &&
        entry->c.items[1] && entry->c.items[1]->type == JSON_NUMBER)
        return entry->c.items[1]->num_val;
    return 0.0;
}

json_t *memobounds_bound_codex_caches(const json_t *context_cache,
        const char *cache_key, int limit, const json_t *max_context_cache) {
    json_t *ctx = context_cache && context_cache->type == JSON_OBJECT
                ? json_copy(context_cache) : json_object();
    json_t *mx = max_context_cache && max_context_cache->type == JSON_OBJECT
               ? json_copy(max_context_cache) : json_object();
    size_t n = json_object_size(ctx);
    int key_present = cache_key && json_obj_get(ctx, cache_key) != NULL;
    if (!key_present && n > 0 && n >= (size_t)limit) {
        /* min(list(cache.items()), key=stamp_of); ties -> first inserted. */
        const char *oldest = NULL;
        double oldest_stamp = 0.0;
        for (size_t i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(ctx, i);
            double s = entry_stamp(json_object_get_at(ctx, i));
            if (!oldest || s < oldest_stamp) { oldest = k; oldest_stamp = s; }
        }
        if (oldest) {
            json_obj_del(ctx, oldest);
            json_obj_del(mx, oldest);
        }
    }
    json_t *res = json_object();
    json_set(res, "context", ctx);
    json_set(res, "max_context", mx);
    return res;
}
