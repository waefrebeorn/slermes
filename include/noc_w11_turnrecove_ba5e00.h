#ifndef NOC_W11_TURNRECOVE_BA5E00_H
#define NOC_W11_TURNRECOVE_BA5E00_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_turnrecove_ba5e00_codex_soft_failure_error @ agent/turn_recovery_codex.py:_codex_soft_failure_error
 * `error` is the response.error value (json object / scalar / null). */
json_t *w11_turnrecove_ba5e00_codex_soft_failure_error(const json_t *error);

/* PoP: w11_turnrecove_ba5e00_is_codex_token_expired @ agent/turn_recovery_codex.py:_is_codex_token_expired
 * status_code must be a JSON NUMBER equal to 401 (Python `!= 401` compares types);
 * `reason` is the injected _extract_api_error_context(...).get("reason") value. */
int w11_turnrecove_ba5e00_is_codex_token_expired(const json_t *status_code, const char *reason);

/* PoP: w11_turnrecove_ba5e00_classify_codex_soft_failure @ agent/turn_recovery_codex.py:classify_codex_soft_failure
 * Returns {"classified": <obj|null>, "error_context": <obj>}. classify_api_error
 * and _extract_api_error_context are injected and driven by the same inputs. */
json_t *w11_turnrecove_ba5e00_classify_codex_soft_failure(
    const char *api_mode, const char *status, const json_t *error,
    const char *provider, const char *model, const char *base_url, const json_t *api_key);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TURNRECOVE_BA5E00_H */
