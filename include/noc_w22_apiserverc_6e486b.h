#ifndef NOC_W22_APISERVERC_6E486B_H
#define NOC_W22_APISERVERC_6E486B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/api_server_config_guard.py (gate w22_apiserverc_6e486b).
 *
 * `ConfigBackendUnavailable` is a SystemExit on purpose (a config reader must not
 * serve defaults instead of the selected backend's config). Escaping an aiohttp
 * handler task it would stop the event loop, so this middleware turns one
 * request's outage into that request's 503. The handler is an I/O seam and the
 * `web` framework is a dependency seam; the decision is the middleware itself.
 *
 * Inputs to `w22_apiserverc_6e486b_middleware`:
 *   outcome      "return" (the handler finished; also the default when the key
 *                is missing), "config_unavailable" (the handler raised
 *                ConfigBackendUnavailable) or "runtime_error" (anything else).
 *   value        what the handler returned, echoed through untouched.
 *   code         `exc.code` for the outage. A SystemExit subclass always carries
 *                `.code` (None when it was built without arguments), so a
 *                missing key behaves exactly like null: str(None) == "None".
 *
 * Result:
 *   {"action":"return","value":<v>}
 *   {"action":"response","status":503,
 *    "body":{"error":{"message":str(code),"type":"server_error",
 *                     "code":"config_backend_unavailable"}}}
 *   {"action":"raise","type":<ExceptionName>}
 * `str(code)` follows Python: null -> "None", true -> "True", 5 -> "5",
 * 3.5 -> "3.5", a string verbatim. */
json_t *w22_apiserverc_6e486b_middleware(const char *outcome, const json_t *value,
                                         const json_t *code);

/* PoP: w22_apiserverc_6e486b_config_unavailable_body @ gateway/platforms/api_server_config_guard.py:config_unavailable_middleware
 * The 503 payload alone: {"error": {"message": str(code), "type": "server_error",
 * "code": "config_backend_unavailable"}}. */
json_t *w22_apiserverc_6e486b_config_unavailable_body(const json_t *code);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_APISERVERC_6E486B_H */
