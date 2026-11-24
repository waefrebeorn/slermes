#ifndef NOC_W16_GATEWAYMUT_FDC014_H
#define NOC_W16_GATEWAYMUT_FDC014_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/gateway_mutations.py (gate w16_gatewaymut_fdc014). */

/* model_refusal_text(refusal, confirmed): guarded-model refusal explanation. */
json_t *w16_gatewaymut_fdc014_model_refusal_text(const json_t *refusal, int confirmed);

/* confirmation_title(refusal): the single guard-warning title. */
json_t *w16_gatewaymut_fdc014_confirmation_title(const json_t *refusal);

/* confirm_choice(raw, choices): typed answer -> a choice key or null. */
json_t *w16_gatewaymut_fdc014_confirm_choice(const char *raw, const json_t *choices);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W16_GATEWAYMUT_FDC014_H */
