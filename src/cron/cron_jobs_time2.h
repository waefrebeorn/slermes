/*
 * cron_jobs_time2.h - cron wall-clock / ticker-age helpers.
 *
 * Module-internal header (port_cron_jobs.c <-> cron_jobs_time2.c).
 */
#ifndef CRON_JOBS_TIME2_H
#define CRON_JOBS_TIME2_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "hermes_json.h"   /* json_t */

char * iso_local(time_t ts);
char * now_iso(void);
char * recoverable_oneshot_run_at(const json_t *schedule, time_t now, const char *last_run_at);
bool stored_wall_clock_is_future(const char *stored_iso, time_t now);
bool timezone_offset_mismatch(const char *stored_iso, int now_off);

#endif /* CRON_JOBS_TIME2_H */
