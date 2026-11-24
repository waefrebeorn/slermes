/*
 * noc_w25_gatewaycon_1c907d.h - C surface for upstream
 * hermes_cli/gateway_connections_probe.py (gate w25_gatewaycon_1c907d).
 *
 * The pure decision core of the Webapp's Test button: which credential headers
 * a saved gateway entry yields (token mode, oauth mode), and what one
 * authenticated GET /api/status answer means (ok + version, or the refusal the
 * route maps to HTTP).  The store read, the HTTP client and the error type are
 * seams: every one of them is a fixed input here.
 */
#ifndef NOC_W25_GATEWAYCON_1C907D_H
#define NOC_W25_GATEWAYCON_1C907D_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {snapshot:{entry:{authMode}, url, token, headers, tokens}} ->
 * {url, headers} | {error}.   (_credential_headers) */
json_t *w25_gatewaycon_1c907d_credentials(const json_t *req);

/* req: {base_url, status, body, json_error, net_error} ->
 * {ok:true, baseUrl, version} | {error}.   (_probe) */
json_t *w25_gatewaycon_1c907d_probe(const json_t *req);

/* The probe timeout, as an object. */
json_t *w25_gatewaycon_1c907d_constants(void);

#ifdef __cplusplus
}
#endif
#endif
