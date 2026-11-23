#ifndef NOC_W5_EXTRAFIRE_54BEBE_H
#define NOC_W5_EXTRAFIRE_54BEBE_H

/*
 * Public C API for the port of cron/extra_fire.py (gate w5_extrafire_54bebe).
 * All exported symbols are extrafire_-prefixed to avoid collisions.
 *
 * Job-mutating functions return the updated job object rather than mutating
 * in place, so the differential harness can observe the result directly.
 * ISO-8601 instants are parsed to UTC seconds for the future-slot predicate.
 */
#include "hermes_json.h"

/* Port of queued(job). Returns json bool (caller frees). */
json_t *extrafire_queued(const json_t *job);

/* Port of restore_target(job): RESTORE_KEY when queued else next_run_at.
 * Returns a copy of the stored value (json string/null, caller frees). */
json_t *extrafire_restore_target(const json_t *job);

/* Port of is_extra(job). Returns json bool (caller frees). */
json_t *extrafire_is_extra(const json_t *job);

/* Port of held_markers(job). Returns json object (caller frees). */
json_t *extrafire_held_markers(const json_t *job);

/* Port of pending_slot(job, now, manual_fire). `now` is an ISO instant string.
 * Returns json string|null (caller frees). */
json_t *extrafire_pending_slot(const json_t *job, const char *now, int manual_fire);

/* Port of stamp_claim(job, slot): returns the updated job (caller frees). */
json_t *extrafire_stamp_claim(const json_t *job, const char *slot);

/* Port of reschedule(job, at): returns the updated job (caller frees). */
json_t *extrafire_reschedule(const json_t *job, const char *at);

#endif /* NOC_W5_EXTRAFIRE_54BEBE_H */
