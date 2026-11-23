#ifndef NOC_W11_BILLINGVIE_7423E1_H
#define NOC_W11_BILLINGVIE_7423E1_H

/* C API for the tui_gateway/billing_view.py port (gate w11_billingvie_7423e1). */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_billingvie_7423e1_wire_str @ tui_gateway/billing_view.py:_wire_str */
json_t *w11_billingvie_7423e1_wire_str(const json_t *value);
/* PoP: w11_billingvie_7423e1_billing_pick @ tui_gateway/billing_view.py:_billing_pick */
json_t *w11_billingvie_7423e1_billing_pick(const json_t *result, const json_t *fields);
/* PoP: w11_billingvie_7423e1_payment_method @ tui_gateway/billing_view.py:_serialize_payment_method */
json_t *w11_billingvie_7423e1_payment_method(const json_t *pm);
/* PoP: w11_billingvie_7423e1_subscription_preview @ tui_gateway/billing_view.py:_serialize_subscription_preview */
json_t *w11_billingvie_7423e1_subscription_preview(const json_t *p);
/* PoP: w11_billingvie_7423e1_auto_reload @ tui_gateway/billing_view.py:_serialize_auto_reload */
json_t *w11_billingvie_7423e1_auto_reload(const json_t *ar);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_BILLINGVIE_7423E1_H */
