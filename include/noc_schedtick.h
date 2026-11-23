#ifndef NOC_SCHEDTICK_H
#define NOC_SCHEDTICK_H

#include "hermes_json.h"

/* C port of cron/scheduler_tick.py. All exports schedtick_-prefixed.
 *
 * Only the deterministic, side-effect-free decision logic of the tick module is
 * ported: recurring-job advance/drop filtering and the once-per-episode yield
 * log gate. Scheduler I/O, locking and dispatch are intentionally out of scope.
 */

/* Model _advance_or_drop_recurring: on a clean advance all due job ids survive;
 * when advance_next_runs raises OSError, only non-recurring ids survive
 * (is_recurring == schedule.kind in {"cron","interval"}). Returns a JSON array. */
json_t *schedtick_advance_or_drop_recurring(int raise_error, json_t *due_jobs);

/* Model _log_tick_yield_once: 1 when logger.error fires for this reason/now,
 * else 0. Module-level state persists across calls in one process. */
int schedtick_log_tick_yield_once(const char *reason, double now);

double schedtick_yield_interval(void);

#endif
