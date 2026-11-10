/*
 * cron_jobs_fields.h - cron job-record field normalisers.
 *
 * Module-internal header (port_cron_jobs.c <-> cron_jobs_fields.c).
 */
#ifndef CRON_JOBS_FIELDS_H
#define CRON_JOBS_FIELDS_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"   /* json_t */

json_t * cronjobs_apply_skill_fields(const json_t *job);
char * cronjobs_coerce_job_text(const json_t *value, const char *fallback);
json_t * cronjobs_normalize_skill_list(const char *skill, const json_t *skills);
char * cronjobs_normalize_workdir(const char *workdir, char **err);

#endif /* CRON_JOBS_FIELDS_H */
