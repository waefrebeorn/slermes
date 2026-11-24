/*
 * noc_w26_updatecmdf_887c4a.h - C surface for upstream
 * hermes_cli/update_cmd_fleet_unit_budget.py (gate w26_updatecmdf_887c4a).
 *
 * systemd unit timing budgets for the update's gateway restarts: how long a
 * restart client waits (the unit's stop + start budgets) and how long an
 * is-active poller must outwait RestartUSec.  The live module reads those
 * properties through update_cmd_fleet._systemctl; here that call is a fixed
 * input -- the fixture supplies the returncode (or the seam failure) and the
 * `show` text the call would have produced, so only the decisions are ported.
 */
#ifndef NOC_W26_UPDATECMDF_887C4A_H
#define NOC_W26_UPDATECMDF_887C4A_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* systemd_restart_timeout:
 * {"show": text, "rc": n, "raise": null|"missing"|"timeout", "start_only": b} */
json_t *w26_updatecmdf_887c4a_restart_timeout(const json_t *in);

/* unit_job_budget: same `show`/`raise` fields plus "verb". */
json_t *w26_updatecmdf_887c4a_unit_job_budget(const json_t *in);

/* unit_job_state: {"show": text, "rc": n, "raise": ...} -> property dict. */
json_t *w26_updatecmdf_887c4a_unit_job_state(const json_t *in);

/* _settled over one ActiveState/SubState/MainPID/Job dict. */
json_t *w26_updatecmdf_887c4a_settled(const json_t *state);

/* service_restart_sec: {"show": text, "default": n, "raise": ...} */
json_t *w26_updatecmdf_887c4a_service_restart_sec(const json_t *in);

/* The module's timing literals as an object. */
json_t *w26_updatecmdf_887c4a_constants(void);

#ifdef __cplusplus
}
#endif
#endif
