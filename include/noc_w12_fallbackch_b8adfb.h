#ifndef NOC_W12_FALLBACKCH_B8ADFB_H
#define NOC_W12_FALLBACKCH_B8ADFB_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/fallback_check.py (gate w12_fallbackch_b8adfb). */

/* _canonical_model_id(model): strip+lower, collapse a Vertex publisher path
 * to "<publisher>/<model>". */
json_t *w12_fallbackch_b8adfb_canonical_model_id(const char *model);

/* answered_by_requested_model(requested, returned) -> bool. */
json_t *w12_fallbackch_b8adfb_answered(const char *requested, const char *returned);

/* EntryCheck.ready / .status. `inference`: 0=None, 1=True, 2=False. */
json_t *w12_fallbackch_b8adfb_check_status(int configured, int auth_ready,
                                           int inference, const char *error_class);

/* _secret_values(*candidates): strings with len >= 8. */
json_t *w12_fallbackch_b8adfb_secret_values(const json_t *candidates);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_FALLBACKCH_B8ADFB_H */
