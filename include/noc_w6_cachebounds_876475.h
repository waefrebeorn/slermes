#ifndef NOC_W6_CACHEBOUNDS_876475_H
#define NOC_W6_CACHEBOUNDS_876475_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_cachebounds_876475_drop_expired @ agent/cache_bounds.py:drop_expired
 * Returns {"values": {survivors}, "stamps": {surviving stamps}} as a new
 * json_t (caller frees). A missing stamp is treated as 0. */
struct json_t *w6_cachebounds_876475_drop_expired(const struct json_t *values,
                                                  const struct json_t *stamps,
                                                  double ttl, double now);

/* PoP: w6_cachebounds_876475_evict_oldest @ agent/cache_bounds.py:evict_oldest
 * cache/siblings values are either {"stamp": number} objects or bare numbers.
 * Returns {"cache": {...}, "siblings": [...]} (caller frees). */
struct json_t *w6_cachebounds_876475_evict_oldest(const struct json_t *cache, const char *key,
                                                  double limit, const struct json_t *siblings);

/* PoP: w6_cachebounds_876475_publish_lru @ agent/cache_bounds.py:publish_lru
 * Returns {"evicted": [keys], "table": {...}, "siblings": [...]} (caller frees). */
struct json_t *w6_cachebounds_876475_publish_lru(const struct json_t *table, const char *key,
                                                 const struct json_t *value, double limit,
                                                 const struct json_t *siblings);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_CACHEBOUNDS_876475_H */
