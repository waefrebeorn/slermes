/*
 * run_startup_flow.h — gateway startup lifecycle FLOW cores of
 * gateway/run_startup.py.
 *
 * Ports the startup DECISION sequences: the serving-state stamp, the
 * abort gate (short-circuit restart/draining/shutdown-event), resume-owner
 * authorization (allow/deny + error path), mid-run fatal-platform parking,
 * the no-connections degrade/exit decision tree, and the fatal-config exit
 * request (exit 78). All live I/O (allowlist probes, runtime-status writes,
 * conflict classification, clean-exit requests, logging) enters through
 * seams; runner state is injected json_t mutated in place.
 */
#ifndef RUN_STARTUP_FLOW_H
#define RUN_STARTUP_FLOW_H

#include "gateway_lifecycle_flow.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Generic logging seam: level is the numeric logging level (DEBUG=10, INFO=20,
 * WARNING=30, ERROR=40); fmt is the raw format string and args the borrowed
 * argument array (the seam copies what it records). exc_info is not modeled. */
typedef void (*gsu_log_fn)(void *ctx, int level, const char *fmt, const json_t *args);

/* ── 1. _serving_state: degraded when a platform is parked, else running ────
 * st borrowed (reads _startup_parked_platforms). Returns a static string. */
/* PoP: gsu_flow_serving_state @ gateway/run_startup.py:_serving_state */
const char *gsu_flow_serving_state(const json_t *st);

/* ── 2. _startup_should_abort: restart OR draining OR shutdown-event set ────
 * Short-circuit: the shutdown_event.is_set seam is called only when both
 * _restart_requested and _draining are falsy. st borrowed. */
typedef struct gsu_abort_seams {
    void *ctx;
    bool (*shutdown_event_is_set)(void *ctx);
} gsu_abort_seams_t;

/* PoP: gsu_flow_startup_should_abort @ gateway/run_startup.py:_startup_should_abort */
bool gsu_flow_startup_should_abort(const json_t *st, const gsu_abort_seams_t *s);

/* ── 3. _resume_owner_authorized: validate the session owner vs allowlist ───
 * Seams:
 *   is_user_authorized -> runner._is_user_authorized_for_source(source);
 *                         returns the verdict, sets *raised + *err_msg on exc.
 *   log                -> gateway logger.
 * source borrowed (a JSON model). Returns the allow/deny verdict. */
typedef struct gsu_resume_seams {
    void *ctx;
    bool (*is_user_authorized)(void *ctx, const json_t *source, bool *raised,
                               const char **err_msg);
    gsu_log_fn log;
} gsu_resume_seams_t;

/* PoP: gsu_flow_resume_owner_authorized @ gateway/run_startup.py:_resume_owner_authorized */
bool gsu_flow_resume_owner_authorized(const char *session_key, const json_t *source,
                                      const gsu_resume_seams_t *s);

/* ── 4. _park_last_fatal_platform: park the last non-retryable platform ─────
 * Seams:
 *   is_global_startup_conflict -> gateway.restart.is_global_startup_conflict(code)
 *   update_runtime_status      -> runner._update_runtime_status(state)
 *   log                        -> gateway logger.
 * st holds _external_drain_active/_draining/_running/_startup_parked_platforms
 * (the last mutated to true when parked). adapter borrowed. Returns true when
 * parked (stay up degraded), false when the gateway must stop instead. */
typedef struct gsu_park_seams {
    void *ctx;
    bool (*is_global_startup_conflict)(void *ctx, const char *error_code);
    void (*update_runtime_status)(void *ctx, const char *state);
    gsu_log_fn log;
} gsu_park_seams_t;

/* PoP: gsu_flow_park_last_fatal_platform @ gateway/run_startup.py:_park_last_fatal_platform */
bool gsu_flow_park_last_fatal_platform(json_t *st, const json_t *adapter,
                                       const gsu_park_seams_t *s);

/* ── 5. _start_handle_no_connections: the shared degrade/exit decision gate ─
 * Seams:
 *   log                       -> gateway logger.
 *   write_runtime_status_quiet-> gateway.run._write_runtime_status_quiet(state, reason)
 *   startup_fail_fatal_config -> runner._startup_fail_fatal_config(reason)
 *                                (scripted helper on this path; records one event).
 * st holds _startup_ownership_conflict/_startup_parked_platforms (last mutated).
 * retryable/nonretryable borrowed arrays of error strings. Returns true when
 * startup must exit. */
typedef struct gsu_noconn_seams {
    void *ctx;
    gsu_log_fn log;
    void (*write_runtime_status_quiet)(void *ctx, const char *gateway_state,
                                       const char *exit_reason);
    void (*startup_fail_fatal_config)(void *ctx, const char *reason);
} gsu_noconn_seams_t;

/* PoP: gsu_flow_start_handle_no_connections @ gateway/run_startup.py:_start_handle_no_connections */
bool gsu_flow_start_handle_no_connections(json_t *st, int connected_count,
                                          int enabled_platform_count,
                                          const json_t *retryable,
                                          const json_t *nonretryable,
                                          const gsu_noconn_seams_t *s);

/* ── 6. _startup_fail_fatal_config: record fatal-config failure (exit 78) ───
 * Seams:
 *   write_runtime_status_quiet -> gateway.run._write_runtime_status_quiet
 *   request_clean_exit         -> runner._request_clean_exit(reason)
 * st holds _exit_code (set to 78) and _startup_restore_in_progress (set false).
 * reason borrowed. Returns nothing (None). */
typedef struct gsu_fatal_seams {
    void *ctx;
    void (*write_runtime_status_quiet)(void *ctx, const char *gateway_state,
                                       const char *exit_reason);
    void (*request_clean_exit)(void *ctx, const char *reason);
} gsu_fatal_seams_t;

/* PoP: gsu_flow_startup_fail_fatal_config @ gateway/run_startup.py:_startup_fail_fatal_config */
void gsu_flow_startup_fail_fatal_config(json_t *st, const char *reason,
                                        const gsu_fatal_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RUN_STARTUP_FLOW_H */
