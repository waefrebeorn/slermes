#ifndef NOC_W20_GATEWAYRUN_A66BB8_H
#define NOC_W20_GATEWAYRUN_A66BB8_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/gateway_runtime_attach.py (gate w20_gatewayrun_a66bb8).
 *
 * One client attach: find (or start) the profile's owner, then mint its
 * single-use session ticket.  The discovery/ensure controls, the control
 * socket and the mint RPC are injected as a scripted receipt + mint result,
 * so the launch policy, the receipt verdict text and the attach decision path
 * are real C.
 */

/* PoP: w20_gatewayrun_a66bb8_client_launch_env @ hermes_cli/gateway_runtime_attach.py:CLIENT_LAUNCH_ENV
 * The launch-policy env names a client scrubs before an attach that may start
 * a daemon, in source order. */
json_t *w20_gatewayrun_a66bb8_client_launch_env(void);

/* PoP: w20_gatewayrun_a66bb8_discover_timeout @ hermes_cli/gateway_runtime_attach.py:DISCOVER_TIMEOUT
 * `DISCOVER_TIMEOUT` (5.0 seconds). */
double w20_gatewayrun_a66bb8_discover_timeout(void);

/* PoP: w20_gatewayrun_a66bb8_service_session_warning @ hermes_cli/gateway_runtime_attach.py:SERVICE_SESSION_START_WARNING
 * The one-line elevated-session warning, verbatim. */
const char *w20_gatewayrun_a66bb8_service_session_warning(void);

/* PoP: w20_gatewayrun_a66bb8_windows_service_session @ hermes_cli/gateway_runtime_attach.py:windows_service_session
 * `windows_session_id() == 0 and process_is_elevated()`, False whenever either
 * fact raises the probe's error (AttributeError/OSError/TypeError/ValueError).
 * Request: {"session_id": int|null, "elevated": bool, "error": str|null}. */
int w20_gatewayrun_a66bb8_windows_service_session(const json_t *probe);

/* PoP: w20_gatewayrun_a66bb8_failure_message @ hermes_cli/gateway_runtime_attach.py:failure_message
 * `gateway <state>: <reason_code or 'not ready'>`, then the detail.  A
 * `runtime_exited` detail is appended verbatim; any other detail is split into
 * stripped, non-empty lines and prefixed with the same verdict head.  A missing
 * detail yields the head alone. */
json_t *w20_gatewayrun_a66bb8_failure_message(const json_t *receipt);

/* PoP: w20_gatewayrun_a66bb8_bounded_detail @ hermes_cli/gateway_runtime_discovery.py:DiscoveryError.bounded_detail
 * Request: {"detail": str|null, "fix": str|null, "limit": int}.
 * When there is a fix, an overlong detail loses the repair command behind the
 * "; repair command omitted (path too long to print)" notice, and the cause is
 * cut visibly with the truncation marker. */
json_t *w20_gatewayrun_a66bb8_bounded_detail(const json_t *req);

/* PoP: w20_gatewayrun_a66bb8_ticket_unavailable @ hermes_cli/gateway_runtime_attach.py:TicketUnavailable
 * The exception's message and attributes: {"message": "ticket_unavailable: <reason>",
 * "reason": <reason>, "detail": <detail or null>}. */
json_t *w20_gatewayrun_a66bb8_ticket_unavailable(const json_t *req);

/* PoP: w20_gatewayrun_a66bb8_attach @ hermes_cli/gateway_runtime_attach.py:attach_gateway
 * Request:
 *   {"home": str, "start": bool, "recover": bool, "timeout": number,
 *    "receipts": {"discover": <receipt>, "ensure": <receipt>},
 *    "mint": {"ok": bool, "ticket": str, "error": "<exc type>"|null,
 *             "reason": str, "detail": str|null, "fix": str|null, "message": str}}
 *   a <receipt> is {"state", "reason_code", "detail", "attachable": bool, "endpoint"}.
 * Answer:
 *   {"path": "discover"|"ensure", "ensures": int,
 *    "discover_timeout": number|null, "ensure_timeout": number|null,
 *    "attached": bool, "state": str, "ticket": str|null,
 *    "mint": {"home":..., "endpoint":..., "purpose": str|null, "scope": str|null}|null}
 * or, when the mint failed,
 *   {"status": "raised", "type": "TicketUnavailable",
 *    "message": "ticket_unavailable: <reason>", "reason": ..., "detail": ...|null}.
 * A discovery waits at most DISCOVER_TIMEOUT of an attach's timeout. */
json_t *w20_gatewayrun_a66bb8_attach(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_GATEWAYRUN_A66BB8_H */
