#ifndef NOC_W5_QUOTAHOLD_3054BF_H
#define NOC_W5_QUOTAHOLD_3054BF_H

/*
 * Public C API for the port of cron/quota_hold.py (gate w5_quotahold_3054bf).
 * All exported symbols are quotahold_-prefixed to avoid collisions.
 *
 * hold_seconds walks a resolved cause chain (each entry carries whether it is
 * a rate-limited AuthError, its retry_after hint and its text). Instants are
 * ISO-8601 strings parsed to UTC seconds. Job-mutating helpers return the
 * updated job object.
 */
#include "hermes_json.h"

/* Port of hold_seconds_from_failure(). `chain` is an array of
 * {"rate_limited":bool, "retry_after":number|null, "text":str} in cause order.
 * Returns json number|null (caller frees). */
json_t *quotahold_hold_seconds(const json_t *chain);

/* Port of hold_active(job, now): STATE_KEY instant strictly after now.
 * Returns json bool (caller frees). */
json_t *quotahold_hold_active(const json_t *job, const char *now);

/* Port of clear_state(job): returns job with STATE_KEY/SCHEDULE_EXPR_KEY
 * removed (caller frees). */
json_t *quotahold_clear_state(const json_t *job);

/* Port of is_recovery_fire(job, next_run). Returns json bool (caller frees). */
json_t *quotahold_is_recovery_fire(const json_t *job, const char *next_run);

#endif /* NOC_W5_QUOTAHOLD_3054BF_H */
