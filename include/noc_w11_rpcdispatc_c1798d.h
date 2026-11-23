/* noc_w11_rpcdispatc_c1798d.h — public C API for faithful port of upstream
 * Hermes tui_gateway/rpc_dispatch.py (JSON-RPC admission path).
 *
 * PoP: w11_rpcdispatc_c1798d_admit @ tui_gateway/rpc_dispatch.py:_handle_admitted_request
 */
#ifndef NOC_W11_RPCDISPATC_C1798D_H
#define NOC_W11_RPCDISPATC_C1798D_H

#include "hermes_json.h"

/* Faithful port of _handle_admitted_request(req) with every collaborator
 * injected through `io` (scripted seams):
 *   io.normalize   : {"__response__": <dict>} | {"rid":..,"method":..,"params":..}
 *   io.methods     : {method: {"__response__": <dict>} | {"__raise__":...}}
 *   io.contracts   : {"methods":[...], "unknown_message":"...{method}...",
 *                     "validate":{"params":..,"problem":null|"..."}}
 * Returns {"ret": <response or null>, "trace": [ {"name","args"}, ... ]}. */
json_t *w11_rpcdispatc_c1798d_admit(json_t *req, json_t *io);

#endif /* NOC_W11_RPCDISPATC_C1798D_H */
