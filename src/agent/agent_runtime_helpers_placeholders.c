/*
 * agent_runtime_helpers_placeholders.c -- real C11 port of the interrupt
 * placeholder leaf module of upstream Hermes
 *   agent/agent_runtime_helpers_placeholders.py.
 *
 * PoP: w21_agentrunti_df442e_interrupted_placeholder @ agent/agent_runtime_helpers_placeholders.py:_INTERRUPTED_PLACEHOLDER
 * PoP: w21_agentrunti_df442e_legacy_placeholder @ agent/agent_runtime_helpers_placeholders.py:_LEGACY_INTERRUPTED_PLACEHOLDER
 * PoP: w21_agentrunti_df442e_hidden_interrupt_placeholder_row @ agent/agent_runtime_helpers_placeholders.py:hidden_interrupt_placeholder_row
 *
 * The module has no imports and no other state: the two literals plus the row
 * builder ARE the behaviour.  `is_placeholder` is the comparison the replay
 * filters make against both spellings (a row carrying the old text stays
 * poisoned after the wording change, so it must still be recognised).
 */
#include "hermes_json.h"
#include "noc_w21_agentrunti_df442e.h"

#include <string.h>

/* Structural label, never prose: the model has no conversational reason to
 * reproduce it (#132949, #81841). */
static const char ARTI_CURRENT_PLACEHOLDER[] = "[interrupt: no assistant output for this turn]";

/* The spelling shipped before that change; models reproduce it verbatim. */
static const char ARTI_LEGACY_PLACEHOLDER[] = "[response interrupted]";

const char *w21_agentrunti_df442e_interrupted_placeholder(void) {
    return ARTI_CURRENT_PLACEHOLDER;
}

const char *w21_agentrunti_df442e_legacy_placeholder(void) {
    return ARTI_LEGACY_PLACEHOLDER;
}

json_t *w21_agentrunti_df442e_hidden_interrupt_placeholder_row(void) {
    json_t *row = json_object();
    json_set(row, "role", json_string("assistant"));
    json_set(row, "content", json_string(""));
    json_set(row, "display_kind", json_string("hidden"));
    /* non-empty api_content: the pre-call sanitizer must not re-heal it every call */
    json_set(row, "api_content", json_string(ARTI_CURRENT_PLACEHOLDER));
    return row;
}

int w21_agentrunti_df442e_is_placeholder(const char *text) {
    if (!text) return 0;
    return (!strcmp(text, ARTI_CURRENT_PLACEHOLDER) ||
            !strcmp(text, ARTI_LEGACY_PLACEHOLDER)) ? 1 : 0;
}
