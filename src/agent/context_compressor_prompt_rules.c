/*
 * context_compressor_prompt_rules.c — C port of upstream Hermes
 * agent/context_compressor_prompt_rules.py.
 * PoP: ctxprompt_unverified_section_heading @ agent/context_compressor_prompt_rules.py:UNVERIFIED_SECTION_HEADING
 * PoP: ctxprompt_epistemic_stance_rule @ agent/context_compressor_prompt_rules.py:EPISTEMIC_STANCE_RULE
 * PoP: ctxprompt_unverified_hypotheses_section @ agent/context_compressor_prompt_rules.py:UNVERIFIED_HYPOTHESES_SECTION
 * PoP: ctxprompt_today_for_prompt_ex @ agent/context_compressor_prompt_rules.py:_today_for_prompt
 *
 * The three literal rule strings are mirrored byte-for-byte (UTF-8 em dash).
 * The date lookup is a seam: the caller supplies the resolved strftime result.
 */
#include "noc_ctxprompt.h"

#include <stdlib.h>
#include <string.h>

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

const char *ctxprompt_unverified_section_heading(void) {
    return "## Unverified / Working Hypotheses";
}

const char *ctxprompt_epistemic_stance_rule(void) {
    return "Never restate an uncertain claim (suspicion, hypothesis, unverified inference) as fact: list each under ## Unverified / Working Hypotheses as \"UNVERIFIED: <claim> — <basis>\". ";
}

const char *ctxprompt_unverified_hypotheses_section(void) {
    return "\n\n## Unverified / Working Hypotheses\n[UNVERIFIED: <claim> — <basis so far>, one per line. If none, write \"None.\"]";
}

char *ctxprompt_today_for_prompt_ex(int clock_ok, const char *date) {
    return dup_str((clock_ok && date) ? date : "");
}
