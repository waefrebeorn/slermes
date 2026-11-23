#ifndef NOC_CONPAYLOAD_H
#define NOC_CONPAYLOAD_H
#include "hermes_json.h"
/* connector_ui_payload: recursively redact connector payloads while keeping
 * authorize links (connect_url strings) intact.  Returns a new tree (free with
 * json_free).  PoP: conpayload_connector_ui_payload @ tui_gateway/connector_payload.py:connector_ui_payload */
json_t *conpayload_connector_ui_payload(const json_t *value);
#endif
