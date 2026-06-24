/**
 * port_scheduler.c — Port of Python: cron/scheduler.py
 *
 * Real C implementations for cron scheduler functions.
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
#include <errno.h>

/* Port of Python: _confirm_adapter_delivery */
bool confirm_adapter_delivery(json_t *send_result)
{
    if (!send_result) {
        hermes_log(LOG_WARNING, "port", "confirm_adapter_delivery: null result");
        return false;
    }
    const char *status = json_node_get_string(json_object_get(send_result, "status"));
    if (status && strcmp(status, "delivered") == 0) {
        hermes_log(LOG_INFO, "port", "confirm_adapter_delivery: confirmed");
        return true;
    }
    const char *error = json_node_get_string(json_object_get(send_result, "error"));
    if (error) {
        hermes_log(LOG_WARNING, "port", "confirm_adapter_delivery: error=%s", error);
    }
    return false;
}

/* Port of Python: _notify_provider_jobs_changed */
void notify_provider_jobs_changed(void)
{
    hermes_log(LOG_INFO, "port", "notify_provider_jobs_changed: notifying providers");
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    char path[4096];
    snprintf(path, sizeof(path), "%s/cron/jobs_changed.notify", home);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "{\"event\": \"jobs_changed\", \"timestamp\": %ld}\n", (long)time(NULL));
        fclose(f);
        hermes_log(LOG_DEBUG, "port", "notify_provider_jobs_changed: notification written");
    } else {
        hermes_log(LOG_WARNING, "port", "notify_provider_jobs_changed: cannot write notification");
    }
}

/* Port of Python: _summarize_cron_failure_for_delivery */
char *summarize_cron_failure_for_delivery(const char *job, const char *error)
{
    if (!job) {
        return strdup("{\"error\": \"unknown job\"}");
    }
    char *summary = malloc(4096);
    if (!summary) return NULL;
    if (error) {
        snprintf(summary, 4096,
                 "{\"job\": \"%s\", \"status\": \"failed\", \"error\": \"%s\"}",
                 job, error);
    } else {
        snprintf(summary, 4096,
                 "{\"job\": \"%s\", \"status\": \"failed\", \"error\": \"unknown\"}",
                 job);
    }
    hermes_log(LOG_INFO, "port", "summarize_cron_failure: %s", summary);
    return summary;
}

/* Port of Python: run_one_job */
bool run_one_job(const char *job)
{
    if (!job) {
        hermes_log(LOG_WARNING, "port", "run_one_job: null job");
        return false;
    }
    hermes_log(LOG_INFO, "port", "run_one_job: executing '%s'", job);
    int ret = system(job);
    if (ret != 0) {
        int exit_code = WEXITSTATUS(ret);
        hermes_log(LOG_WARNING, "port", "run_one_job: job failed (exit=%d)", exit_code);
        return false;
    }
    hermes_log(LOG_INFO, "port", "run_one_job: job completed successfully");
    return true;
}
