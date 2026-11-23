#ifndef NOC_W7_RUNWATCHERS_DB91CE_H
#define NOC_W7_RUNWATCHERS_DB91CE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_runwatchers_db91ce_interruptible_sleep_ticks @ gateway/run_watchers.py:_interruptible_sleep
 * Number of `await asyncio.sleep(1)` iterations before runner._running flips false.
 * running_ticks is the number of ticks the runner stays running (explicit seam). */
int w7_runwatchers_db91ce_interruptible_sleep_ticks(int seconds, int running_ticks);

/* PoP: w7_runwatchers_db91ce_prune_due @ gateway/run_watchers.py:GatewaySessionWatchersMixin._session_housekeeping
 * The hourly SessionStore prune gate: (now - last_prune_ts) > 3600.0. */
int w7_runwatchers_db91ce_prune_due(double now, double last_ts);

/* PoP: w7_runwatchers_db91ce_stall_timeout @ gateway/run_watchers.py:GatewaySessionWatchersMixin._session_stall_timeout_seconds
 * _float_env("HERMES_SESSION_STALL_TIMEOUT", 300): unset/empty/malformed -> 300.
 * env_value is an explicit seam for the environment value (or NULL when unset). */
double w7_runwatchers_db91ce_stall_timeout(const char *env_value);

/* PoP: w7_runwatchers_db91ce_drain_releases @ gateway/run_watchers.py:drain_pending_cache_releases
 * Every release in _pending_cache_releases.values() is called once, in insertion
 * order. `release_keys` is the ordered key list; returns the keys in call order. */
struct json_t *w7_runwatchers_db91ce_drain_releases(const struct json_t *release_keys);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_RUNWATCHERS_DB91CE_H */
