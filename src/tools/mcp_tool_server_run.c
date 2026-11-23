/*
 * mcp_tool_server_run.c — C home for upstream Hermes tools/mcp_tool_server_run.py.
 * PoP: w12_mcptoolse_bdf454_reconnect_outcome @ tools/mcp_tool_server_run.py:MCPServerRunMixin._wait_for_reconnect_or_shutdown
 * PoP: w12_mcptoolse_bdf454_recycle_if_due @ tools/mcp_tool_server_run.py:MCPServerRunMixin._recycle_if_due
 *
 * Deterministic lifecycle decision slices from the MCPServerTask run mixin.
 * Async waits / network transports stay in Python; only the pure post-wait
 * decision and the recycle latch decision are mirrored here.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include "noc_w12_mcptoolse_bdf454.h"

const char *w12_mcptoolse_bdf454_reconnect_outcome(int shutdown_set, int reconnect_set) {
    /* if self._shutdown_event.is_set(): return "shutdown"
     * if not self._reconnect_event.is_set(): return "self-probe"
     * self._reconnect_event.clear(); return "reconnect" */
    if (shutdown_set) return "shutdown";
    if (!reconnect_set) return "self-probe";
    return "reconnect";
}

json_t *w12_mcptoolse_bdf454_recycle_if_due(json_t *reason) {
    json_t *res = json_array();
    if (!reason || reason->type == JSON_NULL) {
        json_append(res, json_bool(false));
        json_append(res, json_null());
    } else {
        /* Reason is a string in practice; copy the value out before any free. */
        json_append(res, json_bool(true));
        json_append(res, json_copy(reason));
    }
    return res;
}
