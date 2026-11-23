#ifndef NOC_W12_BROWSERSES_E75381_H
#define NOC_W12_BROWSERSES_E75381_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_browserses_e75381_resolve_cdp_override @ tools/browser_session_cdp.py:resolve_cdp_override
 * `discovery_payload` is the /json/version response body injected as a seam (null =
 * discovery failed); the live function's network call is otherwise identical.
 * Returns a malloc'd CDP URL string. */
char *w12_browserses_e75381_resolve_cdp_override(const char *cdp_url,
                                                  const struct json_t *discovery_payload);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_BROWSERSES_E75381_H */
