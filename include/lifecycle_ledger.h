/*
 * lifecycle_ledger.h — C11 port of gateway/lifecycle_ledger.py (NS-608).
 *
 * Durable termination-reason evidence for the gateway. Persists a tiny state
 * machine to <HERMES_HOME>/state/gateway.lifecycle.json:
 *   - record_startup() detects an unclean previous exit (phase=running with
 *     no live owner), appends a gateway.previous_unclean_exit record to
 *     logs/gateway-exit-diag.log, then claims the sentinel for this life.
 *   - mark_exited() rewrites the sentinel to phase=exited (idempotent,
 *     ownership-checked against getpid()).
 *   - sample_memory() is the cheap (<1ms) /proc memory snapshot embedded in
 *     the loop heartbeat.
 *
 * Everything is best-effort: a forensics failure must never affect the
 * gateway lifecycle it observes. Opaque, minimal includes, C11 only.
 */
#ifndef GATEWAY_LIFECYCLE_LEDGER_H
#define GATEWAY_LIFECYCLE_LEDGER_H

#include <stdbool.h>
#include <stddef.h>
#include <json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _process_hermes_home @ gateway/lifecycle_ledger.py:_process_hermes_home */
/* HERMES_HOME for process-level identity files (ignore task overrides).
 * Returns the path as a malloc'd string (caller frees) — prefers the
 * HERMES_HOME env var, else the canonical hermes home. */
char *llg_process_hermes_home(void);

/* PoP: get_lifecycle_sentinel_path @ gateway/lifecycle_ledger.py:get_lifecycle_sentinel_path */
/* Return <home>/state/gateway.lifecycle.json. home==NULL => process home.
 * Caller frees the returned string. */
char *llg_get_lifecycle_sentinel_path(const char *home);

/* PoP: sample_memory @ gateway/lifecycle_ledger.py:sample_memory */
/* Cheap memory snapshot as a JSON object: rss_kib / mem_total_kib /
 * mem_available_kib / swap_used_kib ({} when unreadable). Caller frees
 * with json_free. */
json_t *llg_sample_memory(void);

/* PoP: _read_json @ gateway/lifecycle_ledger.py:_read_json */
/* Read + parse a JSON file into a json_t* (NULL on any failure / non-object). */
json_t *llg_read_json(const char *path);

/* PoP: _write_sentinel @ gateway/lifecycle_ledger.py:_write_sentinel */
/* Atomically write the lifecycle sentinel payload (mkdir -p parent). */
void llg_write_sentinel(const json_t *payload, const char *home);

/* PoP: _append_exit_diag @ gateway/lifecycle_ledger.py:_append_exit_diag */
/* Append one JSON line (default=str) to logs/gateway-exit-diag.log. */
void llg_append_exit_diag(const json_t *record, const char *home);

/* PoP: _pid_alive_with_start_time @ gateway/lifecycle_ledger.py:_pid_alive_with_start_time */
/* True when pid is a live process matching start_time (±2s). start_time==NULL
 * (or unparseable) errs toward "alive". */
bool llg_pid_alive_with_start_time(const char *pid, const char *start_time);

/* PoP: detect_unclean_exit @ gateway/lifecycle_ledger.py:detect_unclean_exit */
/* Inspect the previous life's sentinel; return an evidence JSON object when
 * it died uncleanly, else NULL. Read-only. Caller frees with json_free. */
json_t *llg_detect_unclean_exit(const char *home);

/* PoP: record_startup @ gateway/lifecycle_ledger.py:record_startup */
/* Boot-time entry: report any unclean previous exit (persisted + returned),
 * then claim the sentinel for the current life. Never raises; returns the
 * evidence JSON object (caller frees) or NULL. */
json_t *llg_record_startup(const char *home);

/* PoP: mark_exited @ gateway/lifecycle_ledger.py:mark_exited */
/* Mark the current life cleanly exited. Idempotent, ownership-checked,
 * never raises. */
void llg_mark_exited(long exit_code, const char *reason, const char *home);

/* PoP: read_prior_exit_label @ gateway/lifecycle_ledger.py:read_prior_exit_label */
/* One-word summary ("clean"/"unclean"/"unknown") of how the profile's last
 * gateway life ended. Read-only, exception-free. Returns a malloc'd string
 * (caller frees). */
char *llg_read_prior_exit_label(const char *profile_home);

#ifdef __cplusplus
}
#endif
#endif /* GATEWAY_LIFECYCLE_LEDGER_H */
