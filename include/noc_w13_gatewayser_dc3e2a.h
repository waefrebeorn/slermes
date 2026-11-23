#ifndef NOC_W13_GATEWAYSER_DC3E2A_H
#define NOC_W13_GATEWAYSER_DC3E2A_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the pure module-level helpers of acp_adapter/gateway_server.py
 * (gate w13_gatewayser_dc3e2a). The async GatewayACPAgent class is not ported:
 * it is gateway state with no pure computation. All returned strings / json_t
 * trees are heap allocated and owned by the caller (free() / json_free()). */

/* _tools_changed_notice(payload): payload is a JSON object; "mcp_servers" is a
 * JSON array of server-name strings (or missing/null). */
char *w13_gatewayser_dc3e2a_tools_changed_notice(const struct json_t *payload);

/* _bypass_route() with env values supplied explicitly (no process env reads):
 * `model`/`inference_model` map to HERMES_MODEL / HERMES_INFERENCE_MODEL and
 * `provider` to HERMES_INFERENCE_PROVIDER. Returns {"model":..,"provider":..}
 * or {"error": "<GatewayClientError message>"}. */
struct json_t *w13_gatewayser_dc3e2a_bypass_route(const char *model,
        const char *inference_model, const char *provider);

/* _slash_command(text, commands, routes): `commands` is an object mapping a
 * command name/alias to its canonical command.name (models resolve_command);
 * `routes` is an array of registered ACP route names. Returns the canonical
 * command name, the route name, or NULL (Python None). */
char *w13_gatewayser_dc3e2a_slash_command(const char *text,
        const struct json_t *commands, const struct json_t *routes);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_GATEWAYSER_DC3E2A_H */
