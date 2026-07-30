/* Slermes C11 port of gateway/restart_loop_guard.py
 *
 * Auto-resume restart-loop breaker (#30719, defense-3). Pure, best-effort
 * file + timestamp logic. State lives in <HERMES_HOME>/gateway/restart_loop.json
 * so it survives process death. Any read/write failure fails OPEN (no false
 * trip) because a broken breaker must never wedge a healthy gateway.
 *
 * PoP: exact port. Semantic source of truth = gateway/restart_loop_guard.py.
 */
#ifndef SLERMES_RESTART_LOOP_GUARD_H
#define SLERMES_RESTART_LOOP_GUARD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RL_DEFAULT_MAX_RESTARTS 3
#define RL_DEFAULT_WINDOW_SECONDS 60

/* Return the path to the persisted boot log (gateway/restart_loop.json under
 * hermes_home). `buf` must hold PATH_MAX bytes; returns buf or NULL on error. */
char *restart_loop_state_path(char *buf, size_t bufsz, const char *hermes_home);

/* Load the pruned list of boot timestamps (most recent last). Returns the
 * number of timestamps written into `out` (up to `cap`), or -1 on read error.
 * Best-effort: errors yield an empty list (0). */
int restart_loop_load_boots(double *out, int cap, const char *hermes_home);

/* Persist the boot list. Returns true on success. Best-effort: failures are
 * swallowed (return false, never raise). */
bool restart_loop_save_boots(const double *boots, int n, const char *hermes_home);

/* Record that the gateway just booted with restart-interrupted sessions.
 * Prunes boots older than window_seconds and appends `now`. Writes the file.
 * Returns the count of boots now in the (persisted) window, or -1 on failure. */
int restart_loop_record_boot(int window_seconds, double now, const char *hermes_home);

/* True iff >= max_restarts restart-interrupted boots occurred within
 * window_seconds. Fails OPEN (false) on any error. */
bool restart_loop_is_tripped(int max_restarts, int window_seconds, double now,
                             const char *hermes_home);

/* Remove the persisted boot log. Best-effort. */
void restart_loop_clear(const char *hermes_home);

/* Record this boot and report whether the loop is now tripped. Returns true
 * when auto-resume should be SKIPPED to break the loop. */
bool restart_loop_check_and_record(int max_restarts, int window_seconds, double now,
                                   const char *hermes_home);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_RESTART_LOOP_GUARD_H */
