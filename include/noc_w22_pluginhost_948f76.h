#ifndef NOC_W22_PLUGINHOST_948F76_H
#define NOC_W22_PLUGINHOST_948F76_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/plugin_host_http_auth.py (gate w22_pluginhost_948f76).
 *
 * Two decisions carry a route-local admission over the private plugin-host
 * channel without credentials. The token check itself lives in
 * hermes_cli.web_server._require_token (imported lazily, an auth seam that the
 * oracle stubs), and the ASGI app is a network seam; what is left -- the 401
 * fold and the request-scoped state merge -- is pure and ported whole.
 */

/* PoP: w22_pluginhost_948f76_admitted @ hermes_cli/plugin_host_http_auth.py:admitted_for_sensitive_route
 * `guard` describes what _require_token does for this request:
 *   {"kind":"ok"}                       -> {"admitted": true}
 *   {"kind":"http","status":401,...}    -> {"admitted": false}   (a 401 is swallowed)
 *   {"kind":"http","status":<other>}    -> {"raised":{"type":"HTTPException","status":n,"detail":...}}
 *   {"kind":"raise","type":...,"detail":...} -> {"raised":{"type":...,"detail":...}}
 * Any status that is not exactly the int 401 is re-raised (`status_code != 401`),
 * so a string "401" re-raises too. A missing `guard` behaves like {"kind":"ok"}. */
json_t *w22_pluginhost_948f76_admitted(const json_t *guard);

/* PoP: w22_pluginhost_948f76_scope_state @ hermes_cli/plugin_host_http_auth.py:with_hosted_admission
 * The scope an ASGI app is called with:
 *   state = dict(scope.get("state") or {})       (falsy / missing state -> {})
 *   state["hosted_http_authenticated"] = authenticated is True
 *   app({**scope, "state": state}, receive, send)
 * -> {"scope": <merged scope>} or {"raised": "<ExceptionName>"} when the live
 * call raises (`scope.get` on a non-mapping, or a `state` that dict() refuses).
 * `authenticated is True` is an identity test: only JSON true passes, so 1,
 * "true" and null all store false. */
json_t *w22_pluginhost_948f76_scope_state(const json_t *scope, const json_t *authenticated);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_PLUGINHOST_948F76_H */
