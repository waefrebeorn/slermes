#ifndef NOC_W21_UPDATECMDF_E182AA_H
#define NOC_W21_UPDATECMDF_E182AA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/update_cmd_fleet_gatewayless.py (gate w21_updatecmdf_e182aa):
 * gateway-less host evidence for the update-restart obligation.
 *
 * An inventory-less obligation has no owed set, so the live gateway matrix is
 * its whole evidence. On a host that runs no gateway (the Desktop app alone)
 * that matrix is empty forever, so these readers ask the live host directly. A
 * historical receipt is never consulted. */

/* PoP: w21_updatecmdf_e182aa_runtime_outside_gateway_evidence @ hermes_cli/update_cmd_fleet_gatewayless.py:runtime_outside_gateway_evidence
 * A serve/dashboard row the gateway matrix neither covers nor needs to:
 * `kind in ("serve", "dashboard")` and either the manual-serve transfer
 * (`defer_manual_serve`, ported whole) succeeded or its supervisor owns the
 * restart (`desktop`, `desktop-ssh`, `systemd`, `launchd`, `windows-service`,
 * `service`). Unclassified backends and failed transfers stay evidence.
 *
 * `alive` is the `process_identity._pid_alive_matches` seam: true, false, or
 * null/absent for "could not tell". Returns a JSON bool. */
json_t *w21_updatecmdf_e182aa_runtime_outside_gateway_evidence(const json_t *runtime,
                                                               const json_t *alive);

/* PoP: w21_updatecmdf_e182aa_host_owes_no_gateway_restart @ hermes_cli/update_cmd_fleet_gatewayless.py:host_owes_no_gateway_restart
 * True when no profile expects a gateway to be running and every live runtime
 * is outside the matrix.
 *
 * `records` is one `read_runtime_status(home / "gateway_state.json")` answer
 * per profile (null for a profile that never ran a gateway) -- the filesystem
 * seam. A record that does not say `stopped`/`startup_failed` belongs to a
 * gateway that went away without a clean stop, which is what an update that
 * died mid-restart leaves behind. `runtimes` is `collect_runtime_inventory()`
 * as plain rows and `alive_map` maps a stringified pid to that row's
 * liveness (true/false/null). Returns a JSON bool. */
json_t *w21_updatecmdf_e182aa_host_owes_no_gateway_restart(const json_t *records,
                                                           const json_t *runtimes,
                                                           const json_t *alive_map);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_UPDATECMDF_E182AA_H */
