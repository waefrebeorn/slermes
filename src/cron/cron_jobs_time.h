/*
 * cron_jobs_time.h — ISO8601 / wall-clock helpers for the cron subsystem.
 *
 * Module-internal header (port_cron_jobs.c <-> cron_jobs_time.c).
 */
#ifndef CRON_JOBS_TIME_H
#define CRON_JOBS_TIME_H

#include <time.h>

time_t iso_to_ts(const char *iso);
int    iso_utc_offset(const char *iso);
time_t iso_wall_clock(const char *iso);

#endif /* CRON_JOBS_TIME_H */
