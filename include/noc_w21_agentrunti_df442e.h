/*
 * noc_w21_agentrunti_df442e.h -- public C API for the port of upstream
 *   agent/agent_runtime_helpers_placeholders.py
 *
 * The module is the single source of the interrupt-placeholder wording that
 * every healing path substitutes into an empty non-final assistant wire row.
 * Both spellings are exported because replay filters must retire rows that
 * carry the pre-#132949 text, and the invisible "hidden" row shape is exported
 * because the pre-call sanitizer must recognise it as already healed.
 */
#ifndef NOC_W21_AGENTRUNTI_DF442E_H
#define NOC_W21_AGENTRUNTI_DF442E_H

#include "hermes_json.h"

/* Current wording (_INTERRUPTED_PLACEHOLDER). Never NULL, never freed. */
const char *w21_agentrunti_df442e_interrupted_placeholder(void);

/* Pre-#132949 wording (_LEGACY_INTERRUPTED_PLACEHOLDER). */
const char *w21_agentrunti_df442e_legacy_placeholder(void);

/* {"role":"assistant","content":"","display_kind":"hidden","api_content":<current>} */
json_t *w21_agentrunti_df442e_hidden_interrupt_placeholder_row(void);

/* True when text is either shipped spelling (the replay-filter retirement test).
 * NULL / non-string input is False. */
int w21_agentrunti_df442e_is_placeholder(const char *text);

#endif /* NOC_W21_AGENTRUNTI_DF442E_H */
