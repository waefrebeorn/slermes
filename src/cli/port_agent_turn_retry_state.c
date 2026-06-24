/*
 * port_agent_turn_retry_state.c — C port of agent/turn_retry_state.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_turn_retry_state___iter__ @ agent/turn_retry_state.py:__iter__ */

/*
 * TurnRetryState: Per-attempt recovery bookkeeping for the conversation turn loop.
 *
 * Mirrors the Python dataclass with all boolean guards and restart signals.
 */
typedef struct {
    /* Per-provider OAuth / credential refresh guards */
    bool codex_auth_retry_attempted;
    bool anthropic_auth_retry_attempted;
    bool nous_auth_retry_attempted;
    bool nous_paid_entitlement_refresh_attempted;
    bool copilot_auth_retry_attempted;

    /* Format / payload recovery guards */
    bool thinking_sig_retry_attempted;
    bool invalid_encrypted_content_retry_attempted;
    bool image_shrink_retry_attempted;
    bool multimodal_tool_content_retry_attempted;
    bool oauth_1m_beta_retry_attempted;
    bool llama_cpp_grammar_retry_attempted;

    /* Transport / rate-limit recovery */
    bool primary_recovery_attempted;
    bool has_retried_429;

    /* Restart signals */
    bool restart_with_compressed_messages;
    bool restart_with_length_continuation;
} turn_retry_state_t;

/* Field descriptor for iteration */
typedef struct {
    const char *field_name;
    bool field_value;
} retry_state_field_t;

#define RETRY_STATE_FIELDS(X) \
    X(codex_auth_retry_attempted) \
    X(anthropic_auth_retry_attempted) \
    X(nous_auth_retry_attempted) \
    X(nous_paid_entitlement_refresh_attempted) \
    X(copilot_auth_retry_attempted) \
    X(thinking_sig_retry_attempted) \
    X(invalid_encrypted_content_retry_attempted) \
    X(image_shrink_retry_attempted) \
    X(multimodal_tool_content_retry_attempted) \
    X(oauth_1m_beta_retry_attempted) \
    X(llama_cpp_grammar_retry_attempted) \
    X(primary_recovery_attempted) \
    X(has_retried_429) \
    X(restart_with_compressed_messages) \
    X(restart_with_length_continuation)

/*
 * __iter__: Iterate (name, value) pairs for debugging/tests.
 *
 * p1 = pointer to turn_retry_state_t
 * p2 = pointer to output array of retry_state_field_t (size >= 15)
 * p3 = pointer to int for count of fields returned
 *
 * Returns: pointer to output array.
 */
void* cli_agent_turn_retry_state___iter__(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p4; (void)p5;

    turn_retry_state_t *state = (turn_retry_state_t *)p1;
    retry_state_field_t *out = (retry_state_field_t *)p2;
    int *count = (int *)p3;

    if (!state || !out || !count) return NULL;

    int i = 0;
    #define ADD_FIELD(fname) do { \
        out[i].field_name = #fname; \
        out[i].field_value = state->fname; \
        i++; \
    } while(0);

    RETRY_STATE_FIELDS(ADD_FIELD)

    #undef ADD_FIELD

    *count = i;

    hermes_log(LOG_DEBUG, "port",
               "turn_retry_state___iter__: iterated %d fields", i);

    return out;
}
