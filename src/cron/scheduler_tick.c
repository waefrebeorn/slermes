/*
 * scheduler_tick.c — real C11 port of upstream cron/scheduler_tick.py.
 *
 * PoP: schedtick_advance_or_drop_recurring @ cron/scheduler_tick.py:_advance_or_drop_recurring
 * PoP: schedtick_log_tick_yield_once      @ cron/scheduler_tick.py:_log_tick_yield_once
 * PoP: schedtick_yield_interval           @ cron/scheduler_tick.py:_YIELD_LOG_INTERVAL_SECONDS
 *
 * The two ported helpers are the deterministic decision points of the tick:
 * at-most-once advance/drop of recurring jobs, and the stale-code yield log
 * throttle. Lock/process/job execution is not reproducible in a pure harness.
 */
#include "noc_schedtick.h"
#include <stdlib.h>
#include <string.h>

#define SCHEDTICK_YIELD_INTERVAL_SECONDS 3600.0

/* Port of cron.constants.is_recurring: (job.get("schedule") or {}).get("kind")
 * in {"cron", "interval"}. */
static int schedtick_is_recurring(const json_t *job) {
    const json_t *sched, *kind;
    if (!job || job->type != JSON_OBJECT) return 0;
    sched = json_obj_get(job, "schedule");
    if (!sched || sched->type != JSON_OBJECT) return 0;
    kind = json_obj_get(sched, "kind");
    if (!kind || kind->type != JSON_STRING) return 0;
    return (!strcmp(kind->str_val, "cron") || !strcmp(kind->str_val, "interval")) ? 1 : 0;
}

/*
 * PoP: schedtick_advance_or_drop_recurring @ cron/scheduler_tick.py:_advance_or_drop_recurring
 */
json_t *schedtick_advance_or_drop_recurring(int raise_error, json_t *due_jobs) {
    json_t *out = json_array();
    if (!due_jobs || due_jobs->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(due_jobs); i++) {
        json_t *job = json_get(due_jobs, i);
        if (!raise_error || !schedtick_is_recurring(job)) {
            json_t *id = job && job->type == JSON_OBJECT ? json_obj_get(job, "id") : NULL;
            json_append(out, id ? json_copy(id) : json_null());
        }
    }
    return out;
}

static char *schedtick_dup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return (char *)calloc(1, 1);
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Module state: _last_yield_log = {"reason": ..., "at": ...}. */
static char *last_reason = NULL;
static double last_at = 0.0;
static int have_last = 0;

/*
 * PoP: schedtick_log_tick_yield_once @ cron/scheduler_tick.py:_log_tick_yield_once
 */
int schedtick_log_tick_yield_once(const char *reason, double now) {
    const char *rs = reason ? reason : "";
    int fire;
    if (!have_last || strcmp(last_reason, rs) != 0 ||
        (now - last_at) >= SCHEDTICK_YIELD_INTERVAL_SECONDS)
        fire = 1;
    else
        fire = 0;
    free(last_reason);
    last_reason = schedtick_dup(rs);
    last_at = now;
    have_last = 1;
    return fire;
}

double schedtick_yield_interval(void) { return SCHEDTICK_YIELD_INTERVAL_SECONDS; }
