/*
 * deadline.h — Unified deadline layer (agent/deadline.py, 544 lines).
 *
 * One bounded-execution primitive + one timeout resolver, ported to C11.
 * Opaque-ish struct + minimal includes. Faithful 1:1 name parity with the
 * Python module:
 *   MAX_SAFE_TIMEOUT_S        (constant)
 *   DeadlineExpired            (error type, returned via result code)
 *   BoundedResult             (outcome struct)
 *   clamp_timeout              (platform-safe normalization)
 *   resolve_timeout            (config.yaml timeouts: > env > default)
 *   run_bounded_async          (wall-clock deadline; thread-based in C — no
 *                               event loop exists, semantics identical)
 *   run_bounded_sync           (worker-thread deadline)
 *   kill_process_tree          (portable whole-tree termination, POSIX + Win)
 */

#ifndef DEADLINE_H
#define DEADLINE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Upper bound for any timeout handed to platform wait primitives.
 * One year is semantically "unbounded" for every wait while staying far
 * below any platform time_t conversion limit (mirrors Python MAX_SAFE_TIMEOUT_S). */
#define DEADLINE_MAX_SAFE_TIMEOUT_S 31536000.0  /* 365 days */

/* Outcome of a bounded operation. timed_out is the reified outcome; on
 * completion value holds the operation's return value (caller owns/frees).
 * Operation failures are reported via a non-zero error_code, never swallowed. */
typedef struct {
    bool        timed_out;
    void       *value;       /* result payload (caller interprets); NULL on timeout/error */
    double      elapsed_s;   /* wall-clock seconds elapsed */
    double      timeout_s;   /* resolved timeout used (or -1.0 for unbounded) */
    const char *label;       /* borrowed string (do not free) */
    int         error_code;  /* 0 = no error; non-zero = operation raised */
} deadline_bounded_result_t;

/* DeadlineExpired sentinel: a timeout produced by THIS layer is our deadline,
 * not the provider's. raise_if_timed_out() returns value or signals expiry. */
bool deadline_result_timed_out(const deadline_bounded_result_t *r);
void *deadline_result_raise_if_timed_out(deadline_bounded_result_t *r, const char *label);

/* Normalize a timeout value for platform wait primitives.
 * NULL/empty -> -1.0 (unbounded). Non-positive -> -1.0 (unbounded).
 * NaN/non-numeric -> -1.0 with a warning. Over-cap at MAX_SAFE_TIMEOUT_S.
 * Returns seconds (double) or -1.0 for unbounded. */
double clamp_timeout(double timeout);
double clamp_timeout_str(const char *timeout);

/* Resolve a timeout (seconds) for a dotted config key, precedence:
 *   1. timeouts.<key> in config.yaml (dotted walk)
 *   2. env_var when set and non-empty
 *   3. default
 * The winner is passed through clamp_timeout. Returns seconds or -1.0. */
double resolve_timeout(const char *key, double default_s, const char *env_var);

/* Run fn() in a daemon worker thread under a wall-clock deadline.
 * On completion value = fn()'s result (fn writes into *out_value, or returns
 * via a supplied closure). Exceptions are surfaced via result.error_code.
 * On expiry the worker thread is abandoned (daemon) and on_timeout (if given)
 * runs best-effort in the caller's thread. timeout < 0 = unbounded. */
typedef void (*deadline_work_fn)(void *ctx, void **out_value);
typedef void (*deadline_on_timeout_fn)(void *ctx);
deadline_bounded_result_t run_bounded_sync(deadline_work_fn fn, void *ctx,
                                            double timeout_s, const char *label,
                                            deadline_on_timeout_fn on_timeout);

/* Async flavor: identical wall-clock contract (no event loop in C). Runs the
 * work on a daemon worker thread with a separate watchdog thread driving the
 * deadline, exactly like run_bounded_sync. Kept as a distinct symbol for
 * name parity with the Python module. */
deadline_bounded_result_t run_bounded_async(deadline_work_fn fn, void *ctx,
                                             double timeout_s, const char *label,
                                             deadline_on_timeout_fn on_abandon);

/* Terminate pid and all descendants portably.
 * POSIX: snapshot descendants via /proc PPID walk (identity-aware: existence
 * check guards recycled PIDs), signal the process group when pid leads one,
 * then signal every snapshotted descendant individually. sig defaults to
 * SIGKILL (pass 0 to use default). Windows: taskkill /F /T.
 * Returns true when anything was signalled. */
bool kill_process_tree(int pid, int sig);

#ifdef __cplusplus
}
#endif

#endif /* DEADLINE_H */
