#ifndef NOC_CTXPROMPT_H
#define NOC_CTXPROMPT_H
/*
 * Public C API for the port of agent/context_compressor_prompt_rules.py
 * (gate ctxprompt). All exported symbols are module-prefixed.
 *
 * The three rule constants below are static strings: do NOT free the returned
 * pointer.  _today_for_prompt needs a live clock/date lookup that a pure C port
 * cannot perform; the seam takes the already-resolved date (or clock_ok=0 for
 * the clock-failure path) and returns a malloc'd string the caller frees.
 */
const char *ctxprompt_unverified_section_heading(void);
const char *ctxprompt_epistemic_stance_rule(void);
const char *ctxprompt_unverified_hypotheses_section(void);
char *ctxprompt_today_for_prompt_ex(int clock_ok, const char *date);

#endif /* NOC_CTXPROMPT_H */
