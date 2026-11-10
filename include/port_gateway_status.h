/*
 * port_gateway_status.h -- pure helpers ported from gateway/status.py.
 *
 * The runtime-status writer / process liveness / PID probing half stays Python.
 */
#ifndef PORT_GATEWAY_STATUS_H
#define PORT_GATEWAY_STATUS_H

#include <stddef.h>

/* _scope_hash -- sha256(identity)[:16], the lock-filename hash. Caller frees. */
char *gs_scope_hash(const char *identity);

/* _start_times_agree -- every fingerprint positive and within 1 ms of the
 * current one. `recorded` is a NULL-terminated array of numeric strings. */
int gs_start_times_agree(const char *current, const char *const *recorded);

/* start_time_fingerprints_match -- |int(current) - int(recorded)| <= tolerance.
 * The default tolerance is 200 (2 s on either fingerprint scale). */
int gs_fingerprints_match(const char *recorded, const char *current, int tolerance);

/* scoped_lock_owner_label -- the validated `profile` field, else a label
 * inferred from `hermes_home`. NULL means PID-only wording. Caller frees. */
char *gs_lock_owner_label(const char *const *record, const char *default_root);

/* _canonical_hermes_home -- expanduser + resolve. `resolved` is the resolve()
 * answer; NULL falls back to `path`. Caller frees. */
char *gs_canonical_home(const char *path, const char *resolved);

/* _same_hermes_home -- compare two HERMES_HOME paths with the host's case
 * semantics. `*_resolved` stand in for the two resolve() answers. */
int gs_same_hermes_home(const char *left, const char *left_resolved,
                        const char *right, const char *right_resolved);

/* retained_gateway_state -- "startup_failed" / "degraded" / "stopped" for a
 * NOT-running gateway. `runtime` is a flat [key, value, ...] array.
 * Caller frees. */
char *gs_retained_state(const char *const *runtime);

#endif /* PORT_GATEWAY_STATUS_H */
