/*
 * cron_jobs_axes.h - cron inference-axes and misc helpers.
 *
 * Module-internal header (port_cron_jobs.c <-> cron_jobs_axes.c).
 */
#ifndef CRON_JOBS_AXES_H
#define CRON_JOBS_AXES_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"   /* json_t */

char * inference_axes_key(const json_t *job);
char * machine_id(void);
char * new_job_id(void);
char * normalize_optional_cstr(const char *value, bool strip_trailing_slash);
void rmtree(const char *path);

#endif /* CRON_JOBS_AXES_H */
