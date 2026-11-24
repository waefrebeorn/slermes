#ifndef NOC_W17_GATEWAYAUT_143263_H
#define NOC_W17_GATEWAYAUT_143263_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/gateway_automation.py (gate w17_gatewayaut_143263). */
json_t *w17_gatewayaut_143263_decode(const json_t *value);
/* gateway_connection(cli) decision: is the session store the gateway's AND is
 * the gateway session open? connection == NULL models store.connection is None. */
json_t *w17_gatewayaut_143263_gateway_connection(int is_gateway_store, int session_open,
                                                 const char *connection);

#endif /* NOC_W17_GATEWAYAUT_143263_H */
