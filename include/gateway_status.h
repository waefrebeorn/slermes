/*
 * gateway_status.h — Slermes C11 port of gateway/status.py.
 *
 * Gateway runtime status helpers: PID-file based detection of whether the
 * gateway daemon is running, cross-process runtime + scope locks, process
 * fingerprinting (start-time + cmdline identity), runtime health JSON, and
 * the --replace takeover / planned-stop marker protocol.
 *
 * The Python module lives at ``{HERMES_HOME}/gateway.pid`` etc.; this port
 * uses the Slermes home (``slermes_home()`` — SLERMES_HOME or ~/.slermes) as
 * the faithful equivalent so separate homes get separate PID files.
 *
 * Opaque to callers: only the public liveness/lifecycle surface is exported;
 * all path-building, JSON-record, and identity internals stay in status.c.
 *
 * MIT License — Slermes Fork.
 */
#ifndef SLERMES_GATEWAY_STATUS_H
#define SLERMES_GATEWAY_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>   /* pid_t */


/* ── Unified liveness resolver (Python resolve_gateway_liveness) ──────── */

/* Result of the liveness ladder. source is one of "pid", "health",
 * "runtime_status", "none". health_body is a malloc'd serialized JSON string
 * (or NULL) that the caller must free. pid is -1 when no PID was found. */
typedef struct {
    bool running;
    pid_t pid;
    const char *source;
    char *health_body;
    bool probe_error;
} gwstatus_liveness_t;

/* Single source of truth for "is the gateway up?" across dashboard surfaces.
 * Mirrors Python gateway/status.py:resolve_gateway_liveness():
 *   1. PID file + runtime lock (scoped to profile_dir when non-NULL),
 *      TTL-cached when use_cache is true.
 *   2. HTTP health probe (health_probe may be NULL; when non-NULL it is
 *      called as health_probe(&body) and returns true when the gateway is
 *      alive, storing a malloc'd serialized body for the caller to free).
 *   3. Runtime status PID validated against the live process table with
 *      expected_home=profile_dir.
 * runtime_json may be pre-read state (caller owns it); NULL means "not yet
 * read" and the resolver reads it itself. Returns false only on invalid
 * arguments; the ladder result lands in *out. */


bool gwstatus_resolve_gateway_liveness(
    const char *profile_dir,
    const char *runtime_json,
    bool use_cache,
    bool (*health_probe)(char **out_body),
    gwstatus_liveness_t *out);
#ifdef __cplusplus
extern "C" {
#endif

/* ── Process control ─────────────────────────────────────────────────── */

/* Terminate a PID with platform-appropriate force semantics.
 * POSIX: SIGTERM, or SIGKILL when force is true. Returns 0 on success,
 * -1 on failure (errno set). */
int gwstatus_terminate_pid(pid_t pid, bool force);

/* Return a stable per-process start-time fingerprint, or -1 if unavailable.
 * Linux: field 22 of /proc/<pid>/stat (clock ticks since boot). Used as a
 * PID-reuse guard so a recycled PID never matches the original. */
long gwstatus_get_process_start_time(pid_t pid);

/* Cross-platform "is this PID alive" check that does NOT kill the target.
 * Reports zombies as dead (so --replace takeover proceeds). */
bool gwstatus_pid_exists(pid_t pid);

/* ── Command-line identity ───────────────────────────────────────────── */

/* Return true only for a real ``gateway run`` process command line. */
bool gwstatus_looks_like_gateway_command_line(const char *command);

/* Broader matcher: accepts ``run`` and ``restart`` (no-supervisor fallback
 * runtime). Use only for Hermes-owned runtime records / cleanup scans. */
bool gwstatus_looks_like_gateway_runtime_command_line(const char *command);

/* ── Runtime lock ────────────────────────────────────────────────────── */

/* Claim the cross-process runtime lock. Owned by the live process — released
 * automatically by the OS if the process dies. Returns true on acquire. */
bool gwstatus_acquire_gateway_runtime_lock(void);

/* Release the runtime lock if owned by this process. */
void gwstatus_release_gateway_runtime_lock(void);

/* True when some process currently owns the runtime lock. lock_path may be
 * NULL to use the default (this profile's gateway.lock). */
bool gwstatus_is_gateway_runtime_lock_active(const char *lock_path);

/* ── PID file ────────────────────────────────────────────────────────── */

/* Write current PID + metadata atomically (O_CREAT|O_EXCL). Returns 0 on
 * success, -1 with errno==EEXIST when another gateway won the race. */
int gwstatus_write_pid_file(void);

/* Remove the PID file, but only if it belongs to this process. */
void gwstatus_remove_pid_file(void);

/* Return the PID of a running gateway instance, or -1. Verifies the runtime
 * lock, PID file, liveness, and start-time/cmdline identity; cleans up stale
 * PID files when cleanup_stale is true. pid_path may be NULL for default. */
pid_t gwstatus_get_running_pid(const char *pid_path, bool cleanup_stale);

/* Convenience: is the gateway daemon currently running? */
bool gwstatus_is_gateway_running(const char *pid_path, bool cleanup_stale);

/* Conservative fallback liveness from the runtime status record (used when the
 * PID file is absent but a launch-service left a fresh gateway_state.json).
 * runtime_json may be NULL to read the default file; expected_home may be NULL
 * to accept any live gateway command line. Returns a live PID, or -1. */
pid_t gwstatus_get_runtime_status_running_pid(const char *runtime_json,
                                              const char *expected_home);

/* ── Runtime health status JSON ──────────────────────────────────────── */

/* Coerce a persisted active_agents value to a clamped non-negative int. */
int gwstatus_parse_active_agents_str(const char *raw);

/* Whether the gateway is actively processing in-flight turns. */
bool gwstatus_derive_gateway_busy(bool gateway_running,
                                  const char *gateway_state,
                                  int active_agents);

/* Whether the gateway can accept a begin-drain request right now. */
bool gwstatus_derive_gateway_drainable(bool gateway_running,
                                       const char *gateway_state);

/* Persist a subset of runtime health fields. Pass NULL/negative sentinels to
 * leave a field unchanged:
 *   gateway_state    — NULL leaves unchanged
 *   exit_reason      — NULL leaves unchanged (pass "" to set empty/null)
 *   restart_requested/active_agents — negative leaves unchanged
 *   platform         — NULL: no platform sub-record update
 *   platform_state/error_code/error_message — only used when platform != NULL
 * Returns 0 on success, -1 on write failure. */
int gwstatus_write_runtime_status(const char *gateway_state,
                                  const char *exit_reason,
                                  int restart_requested,
                                  int active_agents,
                                  const char *platform,
                                  const char *platform_state,
                                  const char *error_code,
                                  const char *error_message);

/* Read the runtime status JSON as a malloc'd serialized string (caller frees),
 * or NULL if absent/unreadable. path may be NULL for the default file. */
char *gwstatus_read_runtime_status(const char *path);

/* ── Scope locks (per external identity, e.g. one bot token) ──────────── */

/* Acquire a machine-local lock keyed by scope + identity. metadata_json may
 * be NULL. Returns true on acquire; on contention returns false. When
 * out_existing is non-NULL it receives a malloc'd serialized copy of the
 * conflicting record (or NULL), which the caller must free. */
bool gwstatus_acquire_scoped_lock(const char *scope, const char *identity,
                                  const char *metadata_json,
                                  char **out_existing);

/* Release a previously-acquired scope lock when owned by this process. */
void gwstatus_release_scoped_lock(const char *scope, const char *identity);

/* Remove scoped lock files. When owner_pid > 0, only remove records owned by
 * that pid (further narrowed by owner_start_time when >= 0). owner_pid <= 0
 * removes every scoped lock. Returns the number of lock files removed. */
int gwstatus_release_all_scoped_locks(pid_t owner_pid, long owner_start_time);

/* ── --replace takeover / planned-stop markers ───────────────────────── */

/* Record that target_pid is being replaced by the current process.
 * Best-effort; returns true on successful write. */
bool gwstatus_write_takeover_marker(pid_t target_pid);

/* Check & unlink the takeover marker if it names the current process.
 * True => the current SIGTERM is a planned --replace takeover (exit 0). */
bool gwstatus_consume_takeover_marker_for_self(void);

/* Remove the takeover marker unconditionally. */
void gwstatus_clear_takeover_marker(void);

/* Record that target_pid is being stopped intentionally. */
bool gwstatus_write_planned_stop_marker(pid_t target_pid);

/* True when the current process is being intentionally stopped (consumes). */
bool gwstatus_consume_planned_stop_marker_for_self(void);

/* Non-destructive probe: true only when a live planned-stop marker names the
 * current process. Cleans up malformed/stale markers but never unlinks a
 * matching one (the shutdown handler consumes authoritatively). */
bool gwstatus_planned_stop_marker_targets_self(void);

/* Remove the planned-stop marker unconditionally. */
void gwstatus_clear_planned_stop_marker(void);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_GATEWAY_STATUS_H */
