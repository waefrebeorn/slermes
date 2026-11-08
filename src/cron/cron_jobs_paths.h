/*
 * cron_jobs_paths.h - cron jobs directory / lock-file helpers.
 *
 * Module-internal header (port_cron_jobs.c <-> cron_jobs_paths.c).
 */
#ifndef CRON_JOBS_PATHS_H
#define CRON_JOBS_PATHS_H

#include <stddef.h>
#include <stdbool.h>

char * cronjobs_cron_dir(void);
bool cronjobs_ensure_dirs(void);
char * cronjobs_job_output_dir(const char *job_id, char **err);
char * cronjobs_jobs_file(void);
int cronjobs_lock(void);
void cronjobs_secure_dir(const char *path);
void cronjobs_secure_file(const char *path);
char * cronjobs_ticker_heartbeat_file(void);
char * cronjobs_ticker_success_file(void);
void cronjobs_unlock(int fd);
char * set_err(char **err, const char *msg);
char * xstrdup(const char *s);

#endif /* CRON_JOBS_PATHS_H */
