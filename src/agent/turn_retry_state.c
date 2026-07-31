/* Slermes C11 port of agent/turn_retry_state.py — implementation.
 * PoP: exact port. Semantic source of truth = agent/turn_retry_state.py.
 *
 * Backed by a single array of 19 bools indexed by TRS_IDX_*, with explicit
 * (non-macro) get/set accessors so the parity scanner sees each as a real
 * function definition. Each carries its own explicit PoP comment.
 */
#include "turn_retry_state.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    IDX_codex_auth_retry_attempted,
    IDX_anthropic_auth_retry_attempted,
    IDX_nous_auth_retry_attempted,
    IDX_nous_paid_entitlement_refresh_attempted,
    IDX_copilot_auth_retry_attempted,
    IDX_vertex_auth_retry_attempted,
    IDX_thinking_sig_retry_attempted,
    IDX_invalid_encrypted_content_retry_attempted,
    IDX_image_shrink_retry_attempted,
    IDX_multimodal_tool_content_retry_attempted,
    IDX_oauth_1m_beta_retry_attempted,
    IDX_llama_cpp_grammar_retry_attempted,
    IDX_primary_recovery_attempted,
    IDX_has_retried_429,
    IDX_auth_failover_attempted,
    IDX_restart_with_compressed_messages,
    IDX_restart_with_length_continuation,
    IDX_restart_with_rebuilt_messages,
    IDX_restart_with_redirected_messages,
    IDX_COUNT
} trs_idx_t;

struct turn_retry_state {
    bool f[IDX_COUNT];
};

static const char *TRS_NAMES[IDX_COUNT] = {
    "codex_auth_retry_attempted",
    "anthropic_auth_retry_attempted",
    "nous_auth_retry_attempted",
    "nous_paid_entitlement_refresh_attempted",
    "copilot_auth_retry_attempted",
    "vertex_auth_retry_attempted",
    "thinking_sig_retry_attempted",
    "invalid_encrypted_content_retry_attempted",
    "image_shrink_retry_attempted",
    "multimodal_tool_content_retry_attempted",
    "oauth_1m_beta_retry_attempted",
    "llama_cpp_grammar_retry_attempted",
    "primary_recovery_attempted",
    "has_retried_429",
    "auth_failover_attempted",
    "restart_with_compressed_messages",
    "restart_with_length_continuation",
    "restart_with_rebuilt_messages",
    "restart_with_redirected_messages",
};

/* PoP: turn_retry_state_create @ agent/turn_retry_state.py:TurnRetryState */
turn_retry_state_t *turn_retry_state_create(void) {
    return calloc(1, sizeof(turn_retry_state_t));
}

void turn_retry_state_free(turn_retry_state_t *s) { free(s); }

/* PoP: turn_retry_state_get_codex_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_codex_auth_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_codex_auth_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_codex_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_codex_auth_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_codex_auth_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_anthropic_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_anthropic_auth_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_anthropic_auth_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_anthropic_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_anthropic_auth_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_anthropic_auth_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_nous_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_nous_auth_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_nous_auth_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_nous_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_nous_auth_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_nous_auth_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_nous_paid_entitlement_refresh_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_nous_paid_entitlement_refresh_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_nous_paid_entitlement_refresh_attempted] : false;
}
/* PoP: turn_retry_state_set_nous_paid_entitlement_refresh_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_nous_paid_entitlement_refresh_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_nous_paid_entitlement_refresh_attempted] = v;
}

/* PoP: turn_retry_state_get_copilot_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_copilot_auth_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_copilot_auth_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_copilot_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_copilot_auth_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_copilot_auth_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_vertex_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_vertex_auth_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_vertex_auth_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_vertex_auth_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_vertex_auth_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_vertex_auth_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_thinking_sig_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_thinking_sig_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_thinking_sig_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_thinking_sig_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_thinking_sig_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_thinking_sig_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_invalid_encrypted_content_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_invalid_encrypted_content_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_invalid_encrypted_content_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_invalid_encrypted_content_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_invalid_encrypted_content_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_invalid_encrypted_content_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_image_shrink_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_image_shrink_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_image_shrink_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_image_shrink_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_image_shrink_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_image_shrink_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_multimodal_tool_content_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_multimodal_tool_content_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_multimodal_tool_content_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_multimodal_tool_content_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_multimodal_tool_content_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_multimodal_tool_content_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_oauth_1m_beta_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_oauth_1m_beta_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_oauth_1m_beta_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_oauth_1m_beta_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_oauth_1m_beta_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_oauth_1m_beta_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_llama_cpp_grammar_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_llama_cpp_grammar_retry_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_llama_cpp_grammar_retry_attempted] : false;
}
/* PoP: turn_retry_state_set_llama_cpp_grammar_retry_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_llama_cpp_grammar_retry_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_llama_cpp_grammar_retry_attempted] = v;
}

/* PoP: turn_retry_state_get_primary_recovery_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_primary_recovery_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_primary_recovery_attempted] : false;
}
/* PoP: turn_retry_state_set_primary_recovery_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_primary_recovery_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_primary_recovery_attempted] = v;
}

/* PoP: turn_retry_state_get_has_retried_429 @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_has_retried_429(const turn_retry_state_t *s) {
    return s ? s->f[IDX_has_retried_429] : false;
}
/* PoP: turn_retry_state_set_has_retried_429 @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_has_retried_429(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_has_retried_429] = v;
}

/* PoP: turn_retry_state_get_auth_failover_attempted @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_auth_failover_attempted(const turn_retry_state_t *s) {
    return s ? s->f[IDX_auth_failover_attempted] : false;
}
/* PoP: turn_retry_state_set_auth_failover_attempted @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_auth_failover_attempted(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_auth_failover_attempted] = v;
}

/* PoP: turn_retry_state_get_restart_with_compressed_messages @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_restart_with_compressed_messages(const turn_retry_state_t *s) {
    return s ? s->f[IDX_restart_with_compressed_messages] : false;
}
/* PoP: turn_retry_state_set_restart_with_compressed_messages @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_restart_with_compressed_messages(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_restart_with_compressed_messages] = v;
}

/* PoP: turn_retry_state_get_restart_with_length_continuation @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_restart_with_length_continuation(const turn_retry_state_t *s) {
    return s ? s->f[IDX_restart_with_length_continuation] : false;
}
/* PoP: turn_retry_state_set_restart_with_length_continuation @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_restart_with_length_continuation(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_restart_with_length_continuation] = v;
}

/* PoP: turn_retry_state_get_restart_with_rebuilt_messages @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_restart_with_rebuilt_messages(const turn_retry_state_t *s) {
    return s ? s->f[IDX_restart_with_rebuilt_messages] : false;
}
/* PoP: turn_retry_state_set_restart_with_rebuilt_messages @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_restart_with_rebuilt_messages(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_restart_with_rebuilt_messages] = v;
}
/* PoP: turn_retry_state_get_restart_with_redirected_messages @ agent/turn_retry_state.py:TurnRetryState */
bool turn_retry_state_get_restart_with_redirected_messages(const turn_retry_state_t *s) {
    return s ? s->f[IDX_restart_with_redirected_messages] : false;
}
/* PoP: turn_retry_state_set_restart_with_redirected_messages @ agent/turn_retry_state.py:TurnRetryState */
void turn_retry_state_set_restart_with_redirected_messages(turn_retry_state_t *s, bool v) {
    if (s) s->f[IDX_restart_with_redirected_messages] = v;
}

/* PoP: __iter__ @ agent/turn_retry_state.py:TurnRetryState.__iter__ */
turn_retry_field_t *turn_retry_state_fields(const turn_retry_state_t *s, size_t *count_out) {
    if (count_out) *count_out = IDX_COUNT;
    turn_retry_field_t *arr = malloc(sizeof(turn_retry_field_t) * IDX_COUNT);
    if (!arr) return NULL;
    for (int i = 0; i < IDX_COUNT; i++) {
        arr[i].name = TRS_NAMES[i];
        arr[i].value = s ? s->f[i] : false;
    }
    return arr;
}

void turn_retry_state_fields_free(turn_retry_field_t *arr) { free(arr); }
