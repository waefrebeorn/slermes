/*
 * noc_w26_client_1fcd78.h - C surface for upstream
 * tools/connectors/portal/client.py (gate w26_client_1fcd78).
 *
 * The portal connector client's pure decision surface: the slug gate, the
 * route key a call is logged under (no slugs or ids), and the bearer token a
 * header set carries.  The transport, the endpoint resolver and the header
 * provider are injected in the live class; here their output is an input, so
 * no request is ever made.
 */
#ifndef NOC_W26_CLIENT_1FCD78_H
#define NOC_W26_CLIENT_1FCD78_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* validate_slug: {"slug": s} -> {"valid": b, "error": null|"InvalidConnectorSlug"|"TypeError"} */
json_t *w26_client_1fcd78_validate_slug(const json_t *req);

/* _route_key: {"path": "/api/v1/connectors/gmail/tools"} -> "portal.connectors.tools" */
json_t *w26_client_1fcd78_route_key(const json_t *req);

/* authorization_token over a header set:
 * {"headers": {...}} -> {"token": s|null, "error": null|"GatewayAuthError"} */
json_t *w26_client_1fcd78_auth_token(const json_t *req);

/* DEFAULT_TIMEOUT_SECONDS, _SLUG_RE and _PORTAL_ROUTE_WORDS. */
json_t *w26_client_1fcd78_constants(void);

#ifdef __cplusplus
}
#endif
#endif
