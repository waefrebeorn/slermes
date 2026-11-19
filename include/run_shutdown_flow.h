/*
 * run_shutdown_flow.h — gateway shutdown lifecycle FLOW cores of
 * gateway/run_shutdown.py.
 *
 * Ports the shutdown DECISION sequences: the failure-exit verdict, the
 * process exit-verdict resolution (exit codes / SystemExit paths), the
 * shutdown-notice allow/deny gate, the interrupt-reason pick, the best-effort
 * quiet teardown step (error path), and the best-effort scale-to-zero status
 * write (error path). All live I/O (logging, runtime-status writes, the
 * failure-verdict helper) enters through seams; runner state is injected
 * json_t.
 */
#ifndef RUN_SHUTDOWN_FLOW_H
#define RUN_SHUTDOWN_FLOW_H

#include "gateway_lifecycle_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Numeric-level logging seam (DEBUG=10, INFO=20, WARNING=30, ERROR=40); fmt is
 * the raw format string, args the borrowed argument array. exc_info unmodeled. */
typedef void (*gsd_log_fn)(void *ctx, int level, const char *fmt, const json_t *args);

/* ── 1. _exit_with_failure_verdict: true when a failure exit was requested ──
 * runner borrowed ({should_exit_with_failure, exit_reason}). Logs the reason
 * at ERROR when present. Returns the verdict. */
typedef struct gsd_failure_seams {
    void *ctx;
    gsd_log_fn log;
} gsd_failure_seams_t;

/* PoP: gsd_flow_exit_with_failure_verdict @ gateway/run_shutdown.py:_exit_with_failure_verdict */
bool gsd_flow_exit_with_failure_verdict(const json_t *runner, const gsd_failure_seams_t *s);

/* ── 2. _resolve_gateway_exit_verdict: process verdict after stop/abort ─────
 * Seams:
 *   exit_with_failure_verdict -> the helper above (scripted on this path;
 *                                records one event, returns the verdict).
 *   log                       -> gateway logger.
 * runner borrowed ({should_exit_with_failure, exit_reason, exit_code,
 * _restart_requested, _restart_via_service}). signal_initiated_shutdown is the
 * bool argument. The result models a returned verdict OR a raised SystemExit. */
typedef struct gsd_exit_verdict {
    bool exited;      /* SystemExit raised */
    int exit_code;    /* valid when exited */
    bool verdict;     /* return value when not exited */
} gsd_exit_verdict_t;

typedef struct gsd_resolve_seams {
    void *ctx;
    gsd_log_fn log;
    bool (*exit_with_failure_verdict)(void *ctx, const json_t *runner);
} gsd_resolve_seams_t;

/* PoP: gsd_flow_resolve_gateway_exit_verdict @ gateway/run_shutdown.py:_resolve_gateway_exit_verdict */
gsd_exit_verdict_t gsd_flow_resolve_gateway_exit_verdict(const json_t *runner,
                                                         bool signal_initiated_shutdown,
                                                         const gsd_resolve_seams_t *s);

/* ── 3. _notice_allowed: shutdown-notice allow/deny gate ────────────────────
 * config_platforms borrowed (map platform_value -> {gateway_restart_notification});
 * platform_value/what borrowed strings; platform_cfg a SERVED profile's own
 * entry (borrowed) or NULL to read config_platforms. Inlines
 * _restart_notification_allowed. Logs the INFO suppression line when denied. */
typedef struct gsd_notice_seams {
    void *ctx;
    gsd_log_fn log;
} gsd_notice_seams_t;

/* PoP: gsd_flow_notice_allowed @ gateway/run_shutdown.py:_notice_allowed */
bool gsd_flow_notice_allowed(const json_t *config_platforms, const char *platform_value,
                             const char *what, const json_t *platform_cfg,
                             const gsd_notice_seams_t *s);

/* ── 4. _shutdown_interrupt_reason: restart vs shutdown interrupt text ──────
 * Pure pick. Returns a static string. */
/* PoP: gsd_flow_shutdown_interrupt_reason @ gateway/run_shutdown.py:_shutdown_interrupt_reason */
const char *gsd_flow_shutdown_interrupt_reason(bool restart_requested);

/* ── 5. _quiet_step: one best-effort teardown step ──────────────────────────
 * Seams:
 *   fn  -> the step callable; returns a borrowed scripted result, sets *raised
 *          + *err_msg on exception.
 *   log -> gateway logger (DEBUG "<label>: <exc>" on failure).
 * label borrowed. Returns a NEW json value (caller frees): the step result, or
 * json null when the step raised. */
typedef struct gsd_quiet_seams {
    void *ctx;
    json_t *(*fn)(void *ctx, bool *raised, const char **err_msg);
    gsd_log_fn log;
} gsd_quiet_seams_t;

/* PoP: gsd_flow_quiet_step @ gateway/run_shutdown.py:_quiet_step */
json_t *gsd_flow_quiet_step(const char *label, const gsd_quiet_seams_t *s);

/* ── 6. _scale_to_zero_status: best-effort runtime-status write ─────────────
 * Seams:
 *   update_runtime_status -> runner._update_runtime_status(state); sets *raised
 *                            on exception (records the call either way).
 *   log                   -> gateway logger (DEBUG fail_msg on failure).
 * state/fail_msg borrowed. Returns nothing (None). */
typedef struct gsd_stz_seams {
    void *ctx;
    void (*update_runtime_status)(void *ctx, const char *state, bool *raised);
    gsd_log_fn log;
} gsd_stz_seams_t;

/* PoP: gsd_flow_scale_to_zero_status @ gateway/run_shutdown.py:_scale_to_zero_status */
void gsd_flow_scale_to_zero_status(const char *state, const char *fail_msg,
                                   const gsd_stz_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RUN_SHUTDOWN_FLOW_H */
