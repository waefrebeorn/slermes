#ifndef NOC_W21_APISERVERR_960557_H
#define NOC_W21_APISERVERR_960557_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/platforms/api_server_request_options.py (gate w21_apiserverr_960557).
 *
 * The two per-request readers of the api_server facade's model_options blob:
 * the structured/legacy reasoning translation and the service_tier override.
 * Both are pure: they only read the decoded request dict. The helpers they
 * lazily import from the facade (_REASONING_EFFORTS, _REQUEST_OPTION_MISSING,
 * _clean_request_string, _coerce_request_bool) are re-implemented here with
 * their exact facade semantics, because the C side has no import cycle to
 * avoid.
 */

/* PoP: w21_apiserverr_960557_reasoning_config @ gateway/platforms/api_server_request_options.py:_request_reasoning_config
 * `model_options` may be NULL (an absent key reads as Python None). Returns
 * json_null() for None, else {"enabled":false} | {"enabled":true} |
 * {"enabled":true,"effort":<norm>}. Fresh node, caller frees. */
json_t *w21_apiserverr_960557_reasoning_config(const json_t *model_options);

/* PoP: w21_apiserverr_960557_service_tier @ gateway/platforms/api_server_request_options.py:_request_service_tier
 * Python returns `_REQUEST_OPTION_MISSING` for "no override", which has no JSON
 * form, so the sentinel is carried as {"present":false,"value":null};
 * otherwise {"present":true,"value":<str|null>}. */
json_t *w21_apiserverr_960557_service_tier(const json_t *model_options);

/* The facade's `_REASONING_EFFORTS` frozenset, sorted, as a JSON array of the
 * effort names the reader accepts. */
json_t *w21_apiserverr_960557_reasoning_efforts(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_APISERVERR_960557_H */
