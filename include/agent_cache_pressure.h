/*
 * agent_cache_pressure.h — Ports of pure helpers from
 * gateway/agent_cache_pressure.py (memory-pressure bounds for the gateway's
 * per-session AIAgent cache).  Everything here is pure/read-only so it is
 * testable without a gateway.
 */

#ifndef SLERMES_AGENT_CACHE_PRESSURE_H
#define SLERMES_AGENT_CACHE_PRESSURE_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"   /* json_t / json_* (via libjson) */

#ifdef __cplusplus
extern "C" {
#endif

#define ACB_BYTES_PER_MB            (1024 * 1024)
#define ACB_AUTO_BUDGET_FRACTION    0.65
#define ACB_AUTO_BUDGET_FLOOR_MB  512
#define ACB_DEFAULT_MAX_EVICTIONS_PER_PASS 16
#define ACB_DEFAULT_PROTECT_RECENT  8

typedef struct {
    int  max_size;            /* Optional — NULL in Python = None. Use -1 as sentinel. */
    double idle_ttl_secs;     /* Optional — use -1.0 as sentinel. */
    int  memory_high_mb;      /* Optional — use -1 as sentinel. */
    int  max_evictions_per_pass;
    int  protect_recent;
} agent_cache_bounds_t;

#define ACB_UNSET_INT   (-1)
#define ACB_UNSET_FLOAT (-1.0)

/* PoP: _positive_int @ gateway/agent_cache_pressure.py:_positive_int */
int acb_positive_int(const void *value);

/* PoP: _positive_float @ gateway/agent_cache_pressure.py:_positive_float */
double acb_positive_float(const void *value);

/* PoP: resolve_memory_high_mb @ gateway/agent_cache_pressure.py:resolve_memory_high_mb
 * `setting` is a raw config value node (string/bool/number). */
int acb_resolve_memory_high_mb(const void *setting);

/* PoP: _cgroup_limit_bytes @ gateway/agent_cache_pressure.py:_cgroup_limit_bytes */
long long acb_cgroup_limit_bytes(void);

/* PoP: _total_memory_bytes @ gateway/agent_cache_pressure.py:_total_memory_bytes */
long long acb_total_memory_bytes(void);

/* PoP: resolve_agent_cache_bounds @ gateway/agent_cache_pressure.py:resolve_agent_cache_bounds
 * Reads agent.agent_cache out of a *raw* config mapping (json_t*), so an
 * absent key stays absent — caller can tell "operator chose 128" from
 * "operator said nothing". Returns a fully-resolved bounds (sentinels for
 * unset). */
agent_cache_bounds_t acb_resolve_agent_cache_bounds(const void *raw_config);

/* PoP: read_anon_rss_mb @ gateway/agent_cache_pressure.py:read_anon_rss_mb */
int acb_read_anon_rss_mb(void);

/* PoP: transcript_persistence_caught_up @ gateway/agent_cache_pressure.py:transcript_persistence_caught_up
 * Stub-shim: real transcript liveness lives in the gateway session seam. This
 * pure helper reports the agent object's caught-up state; for now it always
 * returns false (never evict an in-flight agent) — safe default, never
 * causes data loss. */
bool acb_transcript_persistence_caught_up(const void *agent);

/* PoP: plan_pressure_evictions @ gateway/agent_cache_pressure.py:plan_pressure_evictions
 * `ordered_entries` is an array of (key, agent) pairs in LRU→MRU order.
 * Returns a malloc'd array of indices into `ordered_entries` of evictable
 * entries, with `*out_count` set, capped at max_evictions, protecting the
 * most-recent `min(protect_recent, len/2)` entries. Caller frees *out_indices.
 */
size_t *acb_plan_pressure_evictions(const void *ordered_entries, size_t entry_count,
                                    bool (*is_evictable)(const char *key, const void *agent, void *user),
                                    void *user, int max_evictions, int protect_recent,
                                    size_t *out_count);

/* Sentinel defaults matching run.py module constants. */
#define ACB_AGENT_CACHE_MAX_SIZE       128
#define ACB_AGENT_CACHE_IDLE_TTL_SECS  3600.0

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_AGENT_CACHE_PRESSURE_H */
