#ifndef NOC_W11_GATEWAYMED_35CEBD_H
#define NOC_W11_GATEWAYMED_35CEBD_H

/* C API for the acp_adapter/gateway_media.py port (gate w11_gatewaymed_35cebd). */
#include "hermes_json.h"

/* submit_fingerprint(session_id, content) -> [session_id, sha256_hex] */
json_t *w11_gatewaymed_35cebd_submit_fingerprint(const char *session_id, const json_t *content);

#endif /* NOC_W11_GATEWAYMED_35CEBD_H */
