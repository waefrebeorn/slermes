#ifndef NOC_W20_GATEWAYSUP_735D69_H
#define NOC_W20_GATEWAYSUP_735D69_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_supervised_restart.py (gate w20_gatewaysup_735d69).
 *
 * `hermes gateway restart` must not treat a supervisor-owned gateway as its own
 * child: the gateway exits back to whatever launched it (SIGUSR1 drain) and
 * success is a fresh supervised PID, never the bare exit. The three decisions
 * that carry that contract -- is this PID externally supervised, did a fresh
 * replacement register, and does the manual fallback start detached or in the
 * foreground -- are ported here; signalling, printing and sys.exit stay
 * upstream.
 */

/* SUPERVISED_REPLACEMENT_VERIFY_TIMEOUT (15.0 s). */
double w20_gatewaysup_735d69_verify_timeout(void);

/* PoP: w20_gatewaysup_735d69_declares_external_supervisor @ hermes_cli/gateway_supervised_restart.py:gateway_declares_external_supervisor
 *
 * True when the running gateway PID was launched for an external supervisor:
 * the control socket's identify answer names a launch supervisor other than
 * "manual" for that same PID, OR the live argv carries the
 * --external-supervisor marker, else the argv the gateway stamped into
 * gateway_state.json when the live command line could not be read.
 *
 * `identity`   the identify answer (object or null)
 * `argv`       _capture_gateway_argv(pid): array or null when unreadable
 * `record`     read_runtime_status(home/"gateway_state.json") or null
 */
int w20_gatewaysup_735d69_declares_external_supervisor(const json_t *pid,
                                                        const json_t *identity,
                                                        const json_t *argv,
                                                        const json_t *record);

/* PoP: w20_gatewaysup_735d69_replacement_pid @ hermes_cli/gateway_supervised_restart.py:_wait_for_supervised_gateway_replacement
 *
 * Poll the pidfile until the supervisor's replacement gateway registers a fresh
 * PID. `observed` is the sequence get_running_pid() answers, one call per poll
 * (the last value repeats). `timeout` is the caller's budget (null means
 * SUPERVISED_REPLACEMENT_VERIFY_TIMEOUT); it is raised to 0.5 s exactly as
 * max(timeout, 0.5) does. Returns the fresh PID, or null once the deadline
 * passes. */
json_t *w20_gatewaysup_735d69_replacement_pid(const json_t *old_pid,
                                              const json_t *observed,
                                              const json_t *timeout,
                                              const json_t *poll_interval);

/* PoP: w20_gatewaysup_735d69_start_mode @ hermes_cli/gateway_supervised_restart.py:start_restart_replacement
 *
 * Start the manual fallback's replacement once the old owner is gone:
 *   {"mode":"detach","home":home,"force":true,"code":<exit code>}
 *       -- --detach (Desktop's bounded client, the updater's recovery call)
 *          starts it the way `gateway ensure` does, and the process exits with
 *          the detached starter's code;
 *   {"mode":"foreground","force":...,"verbose":0}
 *       -- otherwise the caller becomes the long-lived gateway.
 * `detach_code` is what start_detached_replacement returned.
 */
json_t *w20_gatewaysup_735d69_start_mode(const json_t *args, int force,
                                         const char *home, int detach_code);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_GATEWAYSUP_735D69_H */
