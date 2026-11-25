#ifndef ONBOARDING_FLOW_APPLY_H
#define ONBOARDING_FLOW_APPLY_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of session_rules/onboarding_flow.py:apply -- the reduce that turns one onboarding event into
 * the next run record, the model-facing result, and the reminder key that rides on it.
 *
 * Pure: no clock, no I/O. `now` is passed in, never read.
 *
 * `record`   the stored run record (agent/onboarding_record.py); not modified.
 * `event`    {"type": "shown"|"answered"|"wrote"|"timed_out"|"recorded"|"skipped"|"end"|"done"|
 *             "applied", ...}. Field names follow the Python dataclasses.
 * `facts`    {"os","arch","machine_kind","nvidia_platform","facts_source","locale","plugins",
 *             "tours"} -- the safe facts (class A) a run is built from.
 * `eligible_ids` the build beat's eligible task ids (the catalog in
 *             session_rules/onboarding_first_tasks.py is data, ported separately; this is the one
 *             seam). Only consulted for the build beat's pick validation.
 *
 * Returns an owned object {"record": {...}, "result": {...}, "reminder": "..."}.
 * Caller frees. NULL only when `event` has no usable "type".
 */
json_t *onboarding_flow_apply(const json_t *record, const json_t *event, const json_t *facts,
                              const json_t *eligible_ids, double now);

/* The beat ids in flow order, NULL-terminated. */
extern const char *const ONBOARDING_FLOW_BEAT_IDS[];

/* The first beat still pending or shown; NULL when every beat is settled. */
const char *onboarding_flow_next_beat(const json_t *record);

#ifdef __cplusplus
}
#endif

#endif /* ONBOARDING_FLOW_APPLY_H */
