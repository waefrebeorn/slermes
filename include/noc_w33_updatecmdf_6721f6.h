/*
 * noc_w33_updatecmdf_6721f6.h - C surface for upstream
 * hermes_cli/update_cmd_fleet_verify.py (gate w33_updatecmdf_6721f6).
 *
 * The post-restart fleet VERIFICATION decision core: whether the fleet probe
 * should have produced rows, which snapshot rows are still publishing identity,
 * which live gateway PIDs vouch for a profile, which unit names count as
 * serve/dashboard, whether the restart phase still owes a gateway, how the
 * unaccounted outcomes split, how a settled/dead unit's systemctl properties
 * read, and the exact ``gateway_restart`` follow-up reason text.
 *
 * Seams kept OUT of this file (values are passed in instead):
 *   - ``collect_fleet_versions`` / ``print_fleet_version_matrix`` (probe IO);
 *   - ``_systemctl`` subprocess calls, modelled as a ``responses`` object keyed
 *     ``"<scope>/<unit>"`` -> stdout text (a missing key / non-string is the
 *     FileNotFoundError-TimeoutExpired-OSError path);
 *   - the fleet-restart obligation marker read/write, the receipt, the settle
 *     sleep loop and every ``print``.
 *
 * JSON numbers are doubles in libjson, so an integral number stands in for a
 * Python ``int``; fixtures never feed a float where Python tests ``int``.
 */
#ifndef NOC_W33_UPDATECMDF_6721F6_H
#define NOC_W33_UPDATECMDF_6721F6_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w33_updatecmdf_6721f6_fleet_probe_expected @
 *      hermes_cli/update_cmd_fleet.py:_fleet_probe_expected_runtimes */
json_t *w33_updatecmdf_6721f6_fleet_probe_expected(
    const json_t *plan_runtimes, const json_t *pre_restart_pids,
    const json_t *restarted_services, const json_t *killed_pids);

/* PoP: w33_updatecmdf_6721f6_identity_pending @
 *      hermes_cli/update_cmd_fleet_verify.py:_fleet_row_identity_pending */
json_t *w33_updatecmdf_6721f6_identity_pending(const json_t *row,
                                               const json_t *pre_restart_pids);

/* PoP: w33_updatecmdf_6721f6_live_gateway_pids @
 *      hermes_cli/update_cmd_fleet_verify.py:_live_gateway_pids_from_fleet */
json_t *w33_updatecmdf_6721f6_live_gateway_pids(const json_t *fleet_rows);

/* PoP: w33_updatecmdf_6721f6_is_serve_unit @
 *      hermes_cli/update_cmd_fleet_verify.py:_is_serve_unit */
json_t *w33_updatecmdf_6721f6_is_serve_unit(const json_t *name);

/* PoP: w33_updatecmdf_6721f6_restart_phase_owes_gateway @
 *      hermes_cli/update_cmd_fleet_verify.py:_restart_phase_owes_gateway */
json_t *w33_updatecmdf_6721f6_restart_phase_owes_gateway(const json_t *restart);

/* PoP: w33_updatecmdf_6721f6_split_unaccounted @
 *      hermes_cli/update_cmd_fleet_verify.py:_split_unaccounted
 * -> [gateway_missed: bool, serve/dashboard pid list] */
json_t *w33_updatecmdf_6721f6_split_unaccounted(const json_t *outcomes);

/* PoP: w33_updatecmdf_6721f6_owed_restart_reason @
 *      hermes_cli/update_cmd_fleet_verify.py:_owed_restart_reason */
json_t *w33_updatecmdf_6721f6_owed_restart_reason(const json_t *restart,
                                                  const json_t *stopped,
                                                  const json_t *gateway_owed,
                                                  const json_t *stale_serves,
                                                  const json_t *down_serves);

/* PoP: w33_updatecmdf_6721f6_row_is_external @
 *      hermes_cli/update_receipt.py:row_is_external */
json_t *w33_updatecmdf_6721f6_row_is_external(const json_t *row);

/* PoP: w33_updatecmdf_6721f6_restarted_units_gone @
 *      hermes_cli/update_cmd_fleet_verify.py:_restarted_units_gone */
json_t *w33_updatecmdf_6721f6_restarted_units_gone(const json_t *scoped_units,
                                                   const json_t *responses);

/* PoP: w33_updatecmdf_6721f6_gateway_unit_runs_one_of @
 *      hermes_cli/update_cmd_fleet_verify.py:_gateway_unit_runs_one_of */
json_t *w33_updatecmdf_6721f6_gateway_unit_runs_one_of(const json_t *unit,
                                                       const json_t *pids,
                                                       const json_t *responses);

/* The follow-up reason literals, as an object (prefix + the two clamps). */
json_t *w33_updatecmdf_6721f6_constants(void);

#ifdef __cplusplus
}
#endif
#endif
