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

#endif /* PORT_SCHEDULER_TICK_H */
