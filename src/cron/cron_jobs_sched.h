/*
 * cron_jobs_sched.h - cron schedule parsing and next-run computation.
 *
 * Module-internal header (port_cron_jobs.c <-> cron_jobs_sched.c).
 */
#ifndef CRON_JOBS_SCHED_H
#define CRON_JOBS_SCHED_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_json.h"   /* json_t */

int cronjobs_compute_grace_seconds(const json_t *schedule);
char * cronjobs_compute_next_run(const json_t *schedule, const char *last_run_at);
json_t * cronjobs_parse_schedule(const char *schedule_in, char **err);

#endif /* CRON_JOBS_SCHED_H */
