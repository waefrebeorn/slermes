#ifndef NOC_W12_GATEWAYCLI_0AF67A_H
#define NOC_W12_GATEWAYCLI_0AF67A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/gateway_client.py (gate w12_gatewaycli_0af67a). */

/* gateway_ws_target(endpoint, ticket) -> [url, [protocol, ticket-protocol]].
 * `api_origin` is injected (endpoint.api_origin). */
json_t *w12_gatewaycli_0af67a_gateway_ws_target(const char *api_origin, const char *ticket);

/* _bounded_reason(value) -> bounded reason string or null. */
json_t *w12_gatewaycli_0af67a_bounded_reason(const json_t *value);

/* _bounded_error_data(data) -> bounded scalar-only object. */
json_t *w12_gatewaycli_0af67a_bounded_error_data(const json_t *data);

/* rpc_error(error) -> {"reason", "code", "data", "unknown_method"}. */
json_t *w12_gatewaycli_0af67a_rpc_error(const json_t *error);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_GATEWAYCLI_0AF67A_H */
