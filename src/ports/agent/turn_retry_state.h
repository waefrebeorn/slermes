/* Slermes C11 port of agent/turn_retry_state.py
 *
 * Per-attempt recovery bookkeeping for the conversation turn loop. One-shot
 * boolean guards + restart signals the inner retry loop mutates in place, so
 * each recovery branch fires at most once per API-call attempt.
 *
 * Pure, dependency-free. Opaque turn_retry_state_t; accessors mirror the
 * Python dataclass fields. Loop-mechanics locals (retry_count, max_retries,
 * max_compression_attempts) are intentionally NOT on the object.
 *
 * PoP: exact port. Semantic source of truth = agent/turn_retry_state.py.
 */
#ifndef SLERMES_TURN_RETRY_STATE_H
#define SLERMES_TURN_RETRY_STATE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque per-attempt recovery state. */
typedef struct turn_retry_state turn_retry_state_t;

/* Create a fresh state (all guards false, all restart signals false). */
turn_retry_state_t *turn_retry_state_create(void);
void turn_retry_state_free(turn_retry_state_t *s);

/* Per-provider OAuth / credential refresh guards. */
bool turn_retry_state_get_codex_auth_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_codex_auth_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_anthropic_auth_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_anthropic_auth_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_nous_auth_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_nous_auth_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_nous_paid_entitlement_refresh_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_nous_paid_entitlement_refresh_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_copilot_auth_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_copilot_auth_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_vertex_auth_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_vertex_auth_retry_attempted(turn_retry_state_t *s, bool v);

/* Format / payload recovery guards. */
bool turn_retry_state_get_thinking_sig_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_thinking_sig_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_invalid_encrypted_content_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_invalid_encrypted_content_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_image_shrink_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_image_shrink_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_multimodal_tool_content_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_multimodal_tool_content_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_oauth_1m_beta_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_oauth_1m_beta_retry_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_llama_cpp_grammar_retry_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_llama_cpp_grammar_retry_attempted(turn_retry_state_t *s, bool v);

/* Transport / rate-limit recovery. */
bool turn_retry_state_get_primary_recovery_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_primary_recovery_attempted(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_has_retried_429(const turn_retry_state_t *s);
void turn_retry_state_set_has_retried_429(turn_retry_state_t *s, bool v);

/* Auth-failure provider failover (escalation after per-provider refresh failed). */
bool turn_retry_state_get_auth_failover_attempted(const turn_retry_state_t *s);
void turn_retry_state_set_auth_failover_attempted(turn_retry_state_t *s, bool v);

/* Restart signals (read by the outer loop after the attempt). */
bool turn_retry_state_get_restart_with_compressed_messages(const turn_retry_state_t *s);
void turn_retry_state_set_restart_with_compressed_messages(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_restart_with_length_continuation(const turn_retry_state_t *s);
void turn_retry_state_set_restart_with_length_continuation(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_restart_with_rebuilt_messages(const turn_retry_state_t *s);
void turn_retry_state_set_restart_with_rebuilt_messages(turn_retry_state_t *s, bool v);
bool turn_retry_state_get_restart_with_redirected_messages(const turn_retry_state_t *s);
void turn_retry_state_set_restart_with_redirected_messages(turn_retry_state_t *s, bool v);

/* Iteration support (mirrors Python __iter__ -> (name, value) pairs), as a
 * null-terminated array of {name, value}. Caller frees with
 * turn_retry_state_iter_free. */
typedef struct { const char *name; bool value; } turn_retry_field_t;
turn_retry_field_t *turn_retry_state_fields(const turn_retry_state_t *s, size_t *count_out);
void turn_retry_state_fields_free(turn_retry_field_t *arr);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_TURN_RETRY_STATE_H */
