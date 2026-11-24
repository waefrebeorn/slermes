#ifndef NOC_W17_ONBOARDING_4983BC_H
#define NOC_W17_ONBOARDING_4983BC_H

#include "hermes_json.h"

/* C home for upstream Hermes session_rules/onboarding_flow.py.
 * Pure record/answer helpers of the first-run onboarding state machine. */

json_t *w17_onboarding_4983bc_next_beat(const json_t *record);
json_t *w17_onboarding_4983bc_is_active(const json_t *record);
json_t *w17_onboarding_4983bc_applies_to(const char *tool);
json_t *w17_onboarding_4983bc_has_suggestions(const json_t *suggest);
json_t *w17_onboarding_4983bc_is_names(const json_t *value);
json_t *w17_onboarding_4983bc_valid_answer(const char *beat, const json_t *answer,
                                           const json_t *facts, const json_t *picks);
json_t *w17_onboarding_4983bc_build_values(const json_t *facts, const json_t *picks);
json_t *w17_onboarding_4983bc_constants(void);

#endif /* NOC_W17_ONBOARDING_4983BC_H */
