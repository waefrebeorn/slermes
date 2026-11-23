#ifndef NOC_W15_UPDATECMD_1B7D31_H
#define NOC_W15_UPDATECMD_1B7D31_H

#include "hermes_json.h"

/* PoP port of hermes_cli/update_cmd_fleet.py (gate w15_updatecmd_1b7d31). */

/* _is_serve_or_dashboard_unit(unit) -> bool. */
json_t *w15_updatecmd_1b7d31_is_serve_or_dashboard_unit(const char *unit);
/* _is_hermes_gateway_unit(unit) -> bool. */
json_t *w15_updatecmd_1b7d31_is_hermes_gateway_unit(const char *unit);
/* _receipt_looks_unfinished(receipt) -> bool. */
json_t *w15_updatecmd_1b7d31_receipt_looks_unfinished(const json_t *receipt);
/* _restart_phase_failure_is_incomplete(surviving|null, pre_restart_pids|null) -> bool. */
json_t *w15_updatecmd_1b7d31_restart_phase_failure_is_incomplete(const json_t *surviving,
                                                                const json_t *pre_restart_pids);

#endif /* NOC_W15_UPDATECMD_1B7D31_H */
