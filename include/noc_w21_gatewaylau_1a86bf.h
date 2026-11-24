#ifndef NOC_W21_GATEWAYLAU_1A86BF_H
#define NOC_W21_GATEWAYLAU_1A86BF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_launchd_runtime.py (gate w21_gatewaylau_1a86bf).
 *
 * macOS-only module: it boots a one-off launchd job through `/usr/bin/osascript`
 * so a gateway that outlives Desktop keeps its Local Network grant.  The pure
 * parts are the deadline budget, the `launchctl print` state machine of
 * `GatewayJob.poll()`, the "finished job" filter of `_remove_finished_jobs` and
 * the job identity `start_gateway_job` composes.  Left upstream: `_launchctl`
 * itself (subprocess), `os.open`/`plistlib.dump`/`bootstrap`, `pgrep`, and
 * `gateway_launchd.launchd_program_arguments` (the ProgramArguments array).
 */

#define W21_GATEWAYLAU_1A86BF_LABEL_PREFIX "com.nousresearch.hermes.runtime."
#define W21_GATEWAYLAU_1A86BF_POLL_INTERVAL_S 0.5

/* PoP: w21_gatewaylau_1a86bf_budget @ hermes_cli/gateway_launchd_runtime.py:_budget
 * `cap` seconds, but never past the caller's startup deadline: the smaller of
 * `cap` and the time left, floored at 0 (0 once the deadline has passed).
 * `deadline` is null or a monotonic timestamp, `now` is what the clock answers.
 * Answer: {"value": <number>}. */
json_t *w21_gatewaylau_1a86bf_budget(const json_t *deadline, double now, double cap);

/* PoP: w21_gatewaylau_1a86bf_poll @ hermes_cli/gateway_launchd_runtime.py:GatewayJob.poll
 * One `poll()` call.  `req` carries the job's recorded state and the answers
 * `launchctl`/`pgrep` gave:
 *   {"prev_status": int|null, "elapsed": <seconds since the last check>,
 *    "rc": <launchctl print returncode>, "stdout": <its output>,
 *    "pid": int|null, "budget": <seconds left for pgrep>,
 *    "pgrep": [<child pid strings>], "pgrep_fail": bool}
 * Answer: {"status": int|null, "pid": int|null, "checked": bool} -- `checked`
 * is false when the call returned before touching `launchctl` (a recorded
 * status, or inside the 0.5 s window).
 *   - a recorded status wins (`self._status is not None`)
 *   - inside the interval: nothing happens, no launchctl call
 *   - a non-zero returncode means the job is gone: status 1
 *   - `  pid = N` records the gateway pid (pgrep -P, else N) and keeps polling
 *   - `  last exit code = N` is the job's status
 */
json_t *w21_gatewaylau_1a86bf_poll(const json_t *req);

/* PoP: w21_gatewaylau_1a86bf_finished_jobs @ hermes_cli/gateway_launchd_runtime.py:_remove_finished_jobs
 * The labels `launchctl list` says are finished: three fields, the first "-"
 * (no pid) and the last starting with LABEL_PREFIX.  `req` is
 * {"rc": <list returncode>, "stdout": <its output>}; a failing `list` yields [].
 * Answer: a JSON array of labels to `remove`. */
json_t *w21_gatewaylau_1a86bf_finished_jobs(const json_t *req);

/* PoP: w21_gatewaylau_1a86bf_job @ hermes_cli/gateway_launchd_runtime.py:start_gateway_job
 * The identity a launched job carries, from `req`:
 *   {"command": [...], "hex12": <uuid4().hex[:12]>, "uid": int,
 *    "log_path": <str>, "cwd": <str>, "env": {...}}
 * Answer: {"label":..., "target":"gui/<uid>/<label>", "plist_name":".<label>.plist",
 *          "wrapped":["/usr/bin/env","XPC_SERVICE_NAME=0", ...command],
 *          "plist":{"Label":..., "EnvironmentVariables":..., "WorkingDirectory":...,
 *                   "RunAtLoad":true}}
 * `ProgramArguments` is the one plist key left out here: it comes from
 * `gateway_launchd.launchd_program_arguments`, which is not part of this module. */
json_t *w21_gatewaylau_1a86bf_job(const json_t *req);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_GATEWAYLAU_1A86BF_H */
