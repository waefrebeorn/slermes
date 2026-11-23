#ifndef NOC_W12_MCPTOOLSE_BDF454_H
#define NOC_W12_MCPTOOLSE_BDF454_H

#include "hermes_json.h"

/* PoP port of tools/mcp_tool_server_run.py (gate w12_mcptoolse_bdf454). */

/* Final decision of _wait_for_reconnect_or_shutdown(): "shutdown" wins a tie,
 * then "self-probe" when neither event fired, else "reconnect". */
const char *w12_mcptoolse_bdf454_reconnect_outcome(int shutdown_set, int reconnect_set);

/* _recycle_if_due(reason) -> [bool, marked_reason_or_null]. */
json_t *w12_mcptoolse_bdf454_recycle_if_due(json_t *reason);

#endif /* NOC_W12_MCPTOOLSE_BDF454_H */
