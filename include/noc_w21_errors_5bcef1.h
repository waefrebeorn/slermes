#ifndef NOC_W21_ERRORS_5BCEF1_H
#define NOC_W21_ERRORS_5BCEF1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/connectors/gateway/errors.py (gate w21_errors_5bcef1).
 *
 * Every symbol the module exports is pure: three exception classes that only
 * carry attributes, `parse_gateway_error` (envelope -> exception) and
 * `render_connection_required` (-> model-facing payload, with its
 * `_default_message` helper).  Nothing here touches a socket.
 */

/* PoP: w21_errors_5bcef1_parse @ tools/connectors/gateway/errors.py:parse_gateway_error
 * Answer: {"type":"ToolGatewayError"|"GatewayAuthError"|"GatewayUnavailable"|
 *          "IdempotencyConflict"|"RateLimited",
 *          "message":str, "code":str, "status":int, "request_id":str|null,
 *          "retryable":bool} plus "retry_after":number on a 429.
 * {"error_type":"ValueError"} when a non-numeric `Retry-After` header makes
 * `float(header)` raise, which the live function lets propagate.
 *
 * `status` is the HTTP code (a JSON number; a string is read with strtoll).
 * `body` is the decoded JSON body: an object is a Mapping, anything else takes
 * the `elif body:` path and is str()'d then cut at 500 characters.  `headers`
 * is the decoded response headers object (or null). */
json_t *w21_errors_5bcef1_parse(const json_t *status, const json_t *body,
                                const json_t *headers);

/* PoP: w21_errors_5bcef1_render @ tools/connectors/gateway/errors.py:render_connection_required
 * The model-facing CONNECTION_REQUIRED / CONNECTION_EXPIRED payload.  `req`
 * carries the keyword arguments: connector, message, connect_url, hint,
 * surface (default "link"), code (default "CONNECTION_REQUIRED").
 * `connector` and `message` are falsiness-tested like the live keyword
 * arguments, so "" omits the connector and falls back to the default message. */
json_t *w21_errors_5bcef1_render(const json_t *req);

/* PoP: w21_errors_5bcef1_default_message @ tools/connectors/gateway/errors.py:_default_message
 * The default `message` for a code and connector, exposed because the render
 * answer depends on it. */
char *w21_errors_5bcef1_default_message(const char *code, const char *connector);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_ERRORS_5BCEF1_H */
