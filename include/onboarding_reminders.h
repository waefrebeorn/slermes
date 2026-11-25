#ifndef ONBOARDING_REMINDERS_H
#define ONBOARDING_REMINDERS_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/onboarding_text.py's reminder layer: the text that rides on a run step and tells the
 * model what to do next.
 *
 * `key`     a reminder key from onboarding_flow_apply ("after_name", "tour_quick", "moved_on", ...).
 * `record`  the run record (after the step was applied).
 * `facts`   the run's safe facts.
 * `beat`    the card a "wrote"/"timed_out" key is about (NULL: read it from the record's `waiting`).
 * `seam`    the task-catalog seam: {"eligible_rows": [{"id","label","goal"}, ...] in catalog order,
 *           "fixed_label": "<the fixed row's label>"}. Read only when a reminder has to describe the
 *           build menu.
 *
 * Returns an owned string, or NULL when the key is not ported here (build_* guidance needs the task
 * catalog's per-task prompt fields and skills).
 */
char *onboarding_reminder(const char *key, const json_t *record, const json_t *facts,
                          const char *beat, const json_t *seam);

/* agent/onboarding_text.py:turn_note -- the note on a user turn while no card waits. Owned string,
 * never NULL (empty once every card is settled). */
char *onboarding_turn_note(const json_t *record);

/* agent/onboarding_text.py:finish_reminder -- how to close the run. Owned string. */
char *onboarding_finish_reminder(const json_t *record, const json_t *seam);

#ifdef __cplusplus
}
#endif

#endif /* ONBOARDING_REMINDERS_H */
