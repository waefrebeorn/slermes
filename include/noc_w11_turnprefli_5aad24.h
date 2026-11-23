#ifndef NOC_W11_TURNPREFLI_5AAD24_H
#define NOC_W11_TURNPREFLI_5AAD24_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_turnprefli_5aad24_run_preflight_gate @ agent/turn_preflight_gate.py:run_preflight_gate
 * Full guard-chain port. The injected world is carried in `in`:
 *   runtime_error (str|null) -> _ollama_context_limit_error
 *   threshold_tokens, provider_overflow_recovery_pending, last_preflight_pressure,
 *   warrants -> _compression_warrants_another_preflight_pass, anchored/defer,
 *   compression_action/post_pressure -> run_preflight_compression stub result.
 * Returns the rebound PreflightGateVerdict fields plus two injected-call probes. */
json_t *w11_turnprefli_5aad24_run_preflight_gate(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_TURNPREFLI_5AAD24_H */
