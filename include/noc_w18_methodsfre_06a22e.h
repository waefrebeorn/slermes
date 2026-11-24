#ifndef NOC_W18_METHODSFRE_06A22E_H
#define NOC_W18_METHODSFRE_06A22E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of tui_gateway/methods_free_tier.py (gate w18_methodsfre_06a22e).
 * The three small JSON-RPC handlers' own parameter shaping. External auth /
 * offer / challenge-store calls are stubbed by the oracle and injected. */

/* PoP: w18_methodsfre_06a22e_ack_notice @ tui_gateway/methods_free_tier.py:_ (free_tier.ack_notice) */
json_t *w18_methodsfre_06a22e_ack_notice(const char *rid, int guest);
/* PoP: w18_methodsfre_06a22e_claim_nudge @ tui_gateway/methods_free_tier.py:_ (free_tier.claim_nudge) */
json_t *w18_methodsfre_06a22e_claim_nudge(const char *rid, const json_t *params);
/* PoP: w18_methodsfre_06a22e_challenge_result @ tui_gateway/methods_free_tier.py:_ (free_tier.challenge_result) */
json_t *w18_methodsfre_06a22e_challenge_result(const char *rid, const json_t *params);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_METHODSFRE_06A22E_H */
