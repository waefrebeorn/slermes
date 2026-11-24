#ifndef NOC_W18_SESSIONLIF_2E5C9B_H
#define NOC_W18_SESSIONLIF_2E5C9B_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tui_gateway/session_lifecycle.py (gate w18_sessionlif_2e5c9b).
 * Pure lease/lifecycle predicates that need no server globals or DB handles. */

/* PoP: w18_sessionlif_2e5c9b_lease_metadata @ tui_gateway/session_lifecycle.py:_lease_metadata */
json_t *w18_sessionlif_2e5c9b_lease_metadata(const char *live_session_id);
/* PoP: w18_sessionlif_2e5c9b_is_gateway_owned_source @ tui_gateway/session_lifecycle.py:_is_gateway_owned_source */
json_t *w18_sessionlif_2e5c9b_is_gateway_owned_source(const char *source);
/* PoP: w18_sessionlif_2e5c9b_lifecycle_own_sid @ tui_gateway/session_lifecycle.py:_lifecycle_own_sid
 * The registry-scan fallback is stubbed to an empty _sessions map in the oracle. */
json_t *w18_sessionlif_2e5c9b_lifecycle_own_sid(const json_t *session, const char *sid_hint);
/* PoP: w18_sessionlif_2e5c9b_session_holds_transcript_claim @ tui_gateway/session_lifecycle.py:_session_holds_transcript_claim */
json_t *w18_sessionlif_2e5c9b_session_holds_transcript_claim(const json_t *session, double now);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SESSIONLIF_2E5C9B_H */
