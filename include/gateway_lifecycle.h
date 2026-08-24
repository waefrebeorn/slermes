/*
 * gateway_lifecycle.h — Gateway lifecycle ports (hermes_cli/gateway.py).
 *
 * Loop-liveness probing, process kill/stop, zero-decision service install,
 * launchd label enumeration + supervision wait, and the gateway_command
 * error-mapping wrapper.
 *
 * MIT License — Slermes Fork
 */
#ifndef SLERMES_GATEWAY_LIFECYCLE_H
#define SLERMES_GATEWAY_LIFECYCLE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Loop-liveness classifications (probe_gateway_loop_liveness). */
#define GATEWAY_LOOP_ALIVE   "alive"
#define GATEWAY_LOOP_WEDGED  "wedged"
#define GATEWAY_LOOP_UNKNOWN "unknown"

/* Heartbeat cadence is 30s; three missed beats is decisive. */
#define DEFAULT_LOOP_LIVENESS_STALE_AFTER_S 90.0
#define LAUNCHD_SUPERVISION_VERIFY_TIMEOUT  15.0

/* Error sentinels an inner gateway-command runner may return so that
 * gateway_command() maps them to the friendly Python except-branches. */
#define GW_ERR_OK                        0
#define GW_ERR_USER_SYSTEMD_UNAVAILABLE  2
#define GW_ERR_SYSTEM_SCOPE_REQUIRES_ROOT 3

/* PoP: probe_gateway_loop_liveness @ hermes_cli/gateway.py:probe_gateway_loop_liveness
 * Classify a gateway PID's event loop as alive / wedged / unknown by reading
 * the state/gateway.heartbeat JSON file. stale_after <= 0 → default (90s).
 * Never fails: any ambiguity returns GATEWAY_LOOP_UNKNOWN. */
const char *probe_gateway_loop_liveness(int pid, double stale_after);

/* PoP: kill_gateway_processes @ hermes_cli/gateway.py:kill_gateway_processes
 * Kill running gateway processes found by the cross-platform PID scan.
 * Returns the number of terminate calls dispatched successfully. */
int kill_gateway_processes(bool force, const int *exclude_pids,
                           size_t n_exclude, bool all_profiles);

/* PoP: stop_profile_gateway @ hermes_cli/gateway.py:stop_profile_gateway
 * Stop only this profile's gateway via its PID file (SIGTERM + bounded wait
 * + pid-file cleanup). True when a recorded process was signalled. */
bool stop_profile_gateway(void);

/* PoP: ensure_gateway_service @ hermes_cli/gateway.py:ensure_gateway_service
 * Install-and-start the gateway service without prompting (setup/import
 * path). Never prompts, never aborts the caller. context is informational
 * ("setup" / "import"). */
bool ensure_gateway_service(const char *context);

/* PoP: launchd_gateway_labels_for_install @ hermes_cli/gateway.py:launchd_gateway_labels_for_install
 * launchd gateway labels for every profile of THIS install, derived from the
 * profile layout (root label first, then sorted profile labels). Caller owns
 * the returned array of malloc'd strings (free each + the array). */
char **launchd_gateway_labels_for_install(size_t *out_n);

/* PoP: wait_for_launchd_gateway_supervision @ hermes_cli/gateway.py:wait_for_launchd_gateway_supervision
 * Poll `launchctl list <label>` until it reports a live supervised PID or
 * timeout elapses. label NULL → this install's default gateway label. */
bool wait_for_launchd_gateway_supervision(const char *label, double timeout,
                                          double poll_interval);

/* PoP: gateway_command @ hermes_cli/gateway.py:gateway_command
 * Run the inner gateway subcommand dispatcher, mapping the systemd-unavailable
 * and system-scope-requires-root failure sentinels onto the friendly messages
 * + exit(1) of the Python except branches. */
int gateway_command(int (*inner)(void *ctx, int argc, char **argv),
                    void *ctx, int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_GATEWAY_LIFECYCLE_H */
