#ifndef NOC_W21_METHODSSES_5B7963_H
#define NOC_W21_METHODSSES_5B7963_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tui_gateway/methods_session_model_guard.py (gate w21_methodsses_5b7963).
 *
 * `session.create`'s composer overrides and the model x provider coherence
 * routing: which (model, provider) pair the create params pin, whether the
 * provider had to come from the session's build scope, and the per-session
 * (model, reasoning, service_tier) override triple -- including
 * `parse_reasoning_effort` and `parse_exact_service_tier`'s strict ValueError.
 *
 * Not ported: the coherence verdict itself. `static_model_provider_conflict`
 * lives in hermes_cli/models_validate.py over the curated catalogs plus
 * difflib's `get_close_matches` -- another module with its own gate -- so the
 * port delivers exactly what this module contributes to it: the pair to ask
 * about, or "nothing to ask".
 */

/* PoP: w21_methodsses_5b7963_str_or_empty @ tui_gateway/methods_session_model_guard.py:create_overrides
 * `str(params.get(key) or "").strip()`; NULL reads as None. Caller frees. */
char *w21_methodsses_5b7963_str_or_empty(const json_t *value);

/* PoP: w21_methodsses_5b7963_truthy @ utils/env.py:is_truthy_value
 * None -> default(False); a str must strip/lower into {1,true,yes,on}. */
int w21_methodsses_5b7963_truthy(const json_t *value);

/* PoP: w21_methodsses_5b7963_reasoning @ hermes_constants.py:parse_reasoning_effort
 * dict form passes `effort` verbatim; bare words are strict. NULL = no override. */
json_t *w21_methodsses_5b7963_reasoning(const json_t *effort);

/* PoP: w21_methodsses_5b7963_service_tier @ agent/fast_mode.py:parse_exact_service_tier
 *   {"value":"priority"} | {"raised":"ValueError","message":"unknown service tier: x"} */
json_t *w21_methodsses_5b7963_service_tier(const json_t *raw);

/* PoP: w21_methodsses_5b7963_model_override @ tui_gateway/methods_session_model_guard.py:model_override_conflict
 * The pair the coherence check would receive; `scope_provider` is what
 * `resolve_requested_provider()` answers inside `build_scope` (config/env read).
 *   {"model":"...","provider":"...","used_scope":bool,"checked":true,
 *    "result":<conflict>|null}   <- the coherence record (other module's answer)
 *   {"model":"","checked":false} */
json_t *w21_methodsses_5b7963_model_override(const json_t *params, const char *scope_provider,
                                             const json_t *conflict);

/* PoP: w21_methodsses_5b7963_create_overrides @ tui_gateway/methods_session_model_guard.py:create_overrides
 * [model_override|null, reasoning_override|null, service_tier_override|null], or
 * {"status":"error","type":"ValueError","message":"..."} for an unknown tier word. */
json_t *w21_methodsses_5b7963_create_overrides(const json_t *params);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_METHODSSES_5B7963_H */
