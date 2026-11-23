#ifndef NOC_W11_GATEWAYREP_9B4876_H
#define NOC_W11_GATEWAYREP_9B4876_H

/* C API for the acp_adapter/gateway_reply.py port (gate w11_gatewayrep_9b4876). */
#include "hermes_json.h"

/* _text(payload) -> str (malloc'd) */
char *w11_gatewayrep_9b4876_text(const json_t *payload);
/* _close_segment(streamed, shown) -> new shown array (json_t *) */
json_t *w11_gatewayrep_9b4876_close_segment(const char *streamed, const json_t *shown);
/* _final_remainder(...) -> malloc'd str; cut==NULL means cut was absent. */
char *w11_gatewayrep_9b4876_final_remainder(const char *text, const char *streamed,
                                            const json_t *shown, const char *cut,
                                            const char *outcome, bool response_reused,
                                            bool has_receipt);

#endif /* NOC_W11_GATEWAYREP_9B4876_H */
