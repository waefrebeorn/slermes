#ifndef NOC_W20_CREDENTIAL_86C0F9_H
#define NOC_W20_CREDENTIAL_86C0F9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/credential_pool_model_cooldowns.py (gate w20_credential_86c0f9).
 *
 * Model-scoped rate-limit cooldowns for pooled credentials: a 429 for one model
 * benches that model only, while auth/billing failures keep benching the whole
 * credential. The pool object (its lock, its entries tuple and its persistence
 * adapter) stays upstream; every function here is the pure policy that object
 * applies, with `now`, the scripted TTL and the scripted reset_at injected.
 */

/* MODEL_ENTITLEMENT_BENCH_SECONDS = 365 * 24 * 60 * 60 */
double w20_credential_86c0f9_model_entitlement_bench_seconds(void);

/* FAILURE_REASON_USAGE_WINDOW */
const char *w20_credential_86c0f9_usage_window_reason(void);

/* PoP: w20_credential_86c0f9_pool_failure_reason @ agent/credential_pool_model_cooldowns.py:pool_failure_reason
 * The pool's failure reason for a rotation. `reason` is the FailoverReason
 * member's value ("billing", "rate_limit", ...). A usage-window 402 benches only
 * the refused model; an ambiguous billing body is sized as transient. */
json_t *w20_credential_86c0f9_pool_failure_reason(const char *reason,
                                                  const json_t *error_context,
                                                  int billing_unverified);

/* PoP: w20_credential_86c0f9_model_cooldown_until @ agent/credential_pool_model_cooldowns.py:model_cooldown_until
 * Active cooldown blocking the entry for `model`, or null. A missing/blank
 * `model` is conservative: any active model cooldown blocks. */
json_t *w20_credential_86c0f9_model_cooldown_until(const json_t *cooldowns,
                                                   const json_t *model, double now);

/* PoP: w20_credential_86c0f9_merge_model_cooldowns @ agent/credential_pool_model_cooldowns.py:merge_model_cooldowns
 * Latest reset per model across snapshots. Non-dict snapshots are skipped, and a
 * non-number entry is ignored exactly like `isinstance(until, (int, float))`. */
json_t *w20_credential_86c0f9_merge_model_cooldowns(const json_t *maps);

/* PoP: w20_credential_86c0f9_is_model_scoped_failure @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin._is_model_scoped_failure
 * Anthropic per-model 429s, and a Codex ChatGPT-account model entitlement 400. */
int w20_credential_86c0f9_is_model_scoped_failure(const char *provider,
                                                  const json_t *status_code,
                                                  const json_t *model,
                                                  const json_t *failure_reason);

/* PoP: w20_credential_86c0f9_token_is_blocked @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin.token_is_blocked
 * Whether a pool cooldown blocks `token` for `model`. Unknown tokens fail open. */
int w20_credential_86c0f9_token_is_blocked(const json_t *entries, const char *token,
                                           const json_t *model, double now);

/* PoP: w20_credential_86c0f9_cool_down_model @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin._cool_down_model
 * Record a cooldown for `model` on the entry at `entry_index` and every sibling
 * sharing its key. `sole_credential`, `exhausted_ttl` (the scripted
 * _exhausted_ttl(429, sole_credential=sole) result) and `reset_at` (what
 * _normalize_error_context(error_context).get("reset_at") answered) are injected.
 * Answer: {"adopts":[{"index":i,"persist":false,"model_cooldowns":{...}}],
 *          "persisted":true}. */
json_t *w20_credential_86c0f9_cool_down_model(const json_t *entries, int entry_index,
                                              const char *model, const json_t *reset_at,
                                              const char *failure_reason, double now,
                                              int sole_credential, double exhausted_ttl);

/* PoP: w20_credential_86c0f9_limit_state @ agent/credential_pool_model_cooldowns.py:CredentialPoolModelCooldownMixin.limit_state
 * What a picker should say about the pool's rate limits, or null.
 * Each entry carries the scripted _exhausted_until answer as "until".
 *   {"scope":"account","resets_at":epoch} | {"scope":"models","models":{model:epoch}} */
json_t *w20_credential_86c0f9_limit_state(const json_t *entries, const json_t *models,
                                          double now);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CREDENTIAL_86C0F9_H */
