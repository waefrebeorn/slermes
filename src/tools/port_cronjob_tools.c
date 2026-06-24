/**
 * port_cronjob_tools.c — Port of Python: tools/cronjob_tools.py
 *
 * Real C implementations for cron job tool functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

/* Port of Python: _execute_job_now */
char *execute_job_now(const char *job)
{
    if (!job) {
        hermes_log(LOG_WARNING, "port", "execute_job_now: null job");
        return strdup("{\"error\": \"null job\"}");
    }
    hermes_log(LOG_INFO, "port", "execute_job_now: executing '%s'", job);
    int ret = system(job);
    int exit_code = WEXITSTATUS(ret);
    char *result = malloc(256);
    if (!result) return NULL;
    if (exit_code == 0) {
        snprintf(result, 256, "{\"status\": \"success\", \"job\": \"%s\"}", job);
    } else {
        snprintf(result, 256,
                 "{\"status\": \"failed\", \"job\": \"%s\", \"exit_code\": %d}",
                 job, exit_code);
    }
    return result;
}

/* Port of Python: _notify_provider_jobs_changed_safe */
void notify_provider_jobs_changed_safe(void)
{
    hermes_log(LOG_INFO, "port", "notify_provider_jobs_changed_safe: notifying");
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/cron/jobs_changed.notify", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"event\": \"jobs_changed\", \"timestamp\": %ld}\n", (long)time(NULL));
        fclose(f);
        hermes_log(LOG_DEBUG, "port", "notify_provider_jobs_changed_safe: written");
    } else {
        hermes_log(LOG_WARNING, "port",
                   "notify_provider_jobs_changed_safe: cannot write %s", path);
    }
}
