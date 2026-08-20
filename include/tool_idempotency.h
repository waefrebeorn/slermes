/* tool_idempotency.h — process-local idempotency cache for tool side effects.
 *
 * Problem this solves (TianPan-cohort research): agent tool retries are
 * structurally broken because the harness retries blindly, turning a transient
 * failure into a DOUBLE side effect (two emails sent, two payments, ...).
 *
 * Solution: a tool call is keyed by (tool_name, idempotency_key|args). The first
 * successful execution stores its result under that key with a TTL. A retry with
 * the same key within the TTL returns the CACHED result WITHOUT re-executing the
 * handler, so a retry is safe. A caller may supply an explicit
 * "idempotency_key" in the args to control the key; otherwise the keyed material
 * is the normalized args JSON.
 *
 * Storage is an omap (string key -> entry). The entry carries the stored result
 * and an expiry timestamp; lookups past expiry treat the entry as absent and
 * lazily drop it. This is a proper allocated structure (no landlocked static),
 * satisfying the project Hive/omap mandate. Process-local by design; persistence
 * across restarts is a documented future enhancement (hermes_state/sqlite).
 */
#ifndef TOOL_IDEMPOTENCY_H
#define TOOL_IDEMPOTENCY_H

#include <stdbool.h>
#include <stddef.h>   /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_idem_cache tool_idem_cache_t;

/* Create/destroy a cache. value_free is not needed (results are strdup'd and
 * freed by the cache). Returns NULL on allocation failure. */
tool_idem_cache_t *tool_idem_new(void);
void tool_idem_free(tool_idem_cache_t *c);

/* Build a stable cache key for (tool, args). If args JSON contains an
 * "idempotency_key" field, it is used as the key material; otherwise the raw
 * args string is used. Caller frees the returned string. */
char *tool_idem_key(const char *tool, const char *args);

/* Store result under key with a TTL in seconds (0 = default 600s).
 * Replaces any existing entry. Returns false on OOM. The cache takes ownership
 * of a COPY of result. */
bool tool_idem_put(tool_idem_cache_t *c, const char *key,
                   const char *result, int ttl_sec);

/* Fetch a cached result for key if present and unexpired. Returns a malloc'd
 * copy the caller must free, or NULL when absent/expired. On expiry the stale
 * entry is dropped. */
char *tool_idem_get(tool_idem_cache_t *c, const char *key);

/* Remove a key (e.g. on explicit invalidate). Returns true if it was present. */
bool tool_idem_invalidate(tool_idem_cache_t *c, const char *key);

/* Attach a JSON persistence file. On attach, any non-expired entries in the file
 * are loaded into the cache (surviving process restarts). Thereafter every
 * tool_idem_put rewrites the file atomically, so the cache is durable. Pass NULL
 * to detach (stops auto-save). The path should live under HERMES_HOME. */
void tool_idem_attach(tool_idem_cache_t *c, const char *path);

/* Current entry count (diagnostics). */
size_t tool_idem_count(const tool_idem_cache_t *c);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_IDEMPOTENCY_H */
