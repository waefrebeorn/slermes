/**
 * @file hermes_gateway_msgraph.h
 * @brief Microsoft Graph webhook platform declarations.
 */
#ifndef HERMES_GATEWAY_MS_GRAPH_H
#define HERMES_GATEWAY_MS_GRAPH_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  msgraph_webhook — raw socket HTTP server for Microsoft Graph notifications
 * ================================================================ */

void msgraph_webhook_init(const char *webhook_path, const char *health_path, int port);
void msgraph_webhook_run(void);

#endif /* HERMES_GATEWAY_MS_GRAPH_H */