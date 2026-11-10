/*
 * port_scheduler_tick.h — pure helpers from cron/scheduler_tick.py
 * (_advance_or_drop_recurring's decision) and cron/constants.py (is_recurring).
 * The store advance (advance_next_runs IO) is the parameterized fact.
 */
#ifndef PORT_SCHEDULER_TICK_H
#define PORT_SCHEDULER_TICK_H

/* cron/constants.py is_recurring(job): True when job["schedule"]["kind"] is
 * "cron" or "interval"; a falsy/missing schedule is not recurring. */
int js_is_recurring(const char *job_json);

/* scheduler_tick _advance_or_drop_recurring's result: the due jobs unchanged
 * when advance_ok, else the subset that are not recurring. Returns malloc'd
 * JSON array, caller frees. */
char *js_advance_or_drop(const char *due_jobs_json, int advance_ok);

/* scheduler_tick _log_tick_yield_once decision: True when the yield should be
 * logged — the reason/skew signature changed, or the cooldown elapsed. Calling
 * this is the caller's decision input; the caller is responsible for the
 * once-per-episode state bookkeeping like the Python module global. */
int js_tick_yield_should_log(const char *last_reason, double last_at,
                             const char *reason, double now);

/* scheduler_tick _log_tick_yield_once message: the fixed error text with the
 * reason interpolated. Returns malloc'd, caller frees. */
char *js_tick_yield_message(const char *reason);

#endif /* PORT_SCHEDULER_TICK_H */
