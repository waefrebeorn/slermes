#ifndef HERMES_GATEWAY_LIFECYCLE_H
#define HERMES_GATEWAY_LIFECYCLE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Lifecycle API ─────────────────────────────────────────────── */

/**
 * Initialize gateway lifecycle state.
 */
void gw_lifecycle_init(void);

/**
 * Start the gateway lifecycle (transition to STARTING).
 */
void gw_lifecycle_start(void);

/**
 * Signal that the gateway has fully started (transition to RUNNING).
 */
void gw_lifecycle_started(void);

/**
 * Stop the gateway lifecycle (transition to STOPPING).
 */
void gw_lifecycle_stop(void);

/**
 * Signal that the gateway has fully stopped (transition to STOPPED).
 */
void gw_lifecycle_stopped(void);

/**
 * Request a restart. Returns 0 on success, 1 if rate limited.
 */
int gw_lifecycle_restart(void);

/**
 * Get lifecycle status as JSON string. Caller must free.
 */
char *gw_lifecycle_get_status_json(void);

/**
 * Check if gateway is in a running state.
 */
bool gw_lifecycle_is_running(void);

/**
 * Graceful shutdown with timeout.
 */
void gw_shutdown(int timeout_sec);

/* ── PID file API ─────────────────────────────────────────────── */

/**
 * Write current PID to ~/.slermes/gateway.pid.
 * Port of Python gateway/run.py: GatewayRunner saves PID on start.
 */
void gw_lifecycle_write_pid(void);

/**
 * Remove PID file on shutdown.
 */
void gw_lifecycle_remove_pid(void);

/* ── Reconnection API ──────────────────────────────────────────── */

/**
 * Register a platform for automatic reconnection.
 */
void gw_reconnect_register(const char *name,
                            void (*restart_fn)(void),
                            int max_fails,
                            int interval_sec);

/**
 * Report a platform failure, triggering reconnection if threshold met.
 */
void gw_reconnect_report_failure(const char *name);

/**
 * Report a successful reconnection (resets failure count).
 */
void gw_reconnect_report_success(const char *name);

/**
 * Start the reconnect watcher background thread.
 */
void gw_reconnect_start_watcher(void);

/**
 * Stop the reconnect watcher background thread.
 */
void gw_reconnect_stop_watcher(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_LIFECYCLE_H */
