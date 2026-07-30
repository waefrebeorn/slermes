/**
 * @file hermes_gateway_pool.h
 * @brief Gateway HTTP connection pool API (P101).
 */
#ifndef HERMES_GATEWAY_POOL_H
#define HERMES_GATEWAY_POOL_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P101: HTTP connection pool API
 * ================================================================ */

/* Get an HTTP client from the pool (or create one).
 * endpoint: the API base URL for connection affinity.
 * Returns NULL on failure. */
http_client_t *gw_pool_get_client(const char *endpoint);

/* Return an HTTP client to the pool for reuse.
 * Pass NULL to free without returning. */
void gw_pool_return_client(http_client_t *client, const char *endpoint);

/* Close all idle connections in the pool */
void gw_pool_cleanup(void);

#endif /* HERMES_GATEWAY_POOL_H */