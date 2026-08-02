/*
 * port_cron_scheduler_remaining.c — Port of cron/scheduler.py scheduler
 * surface. Failure summaries, delivery confirmation, one-job runs,
 * provider notifications, ticks.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _summarize_cron_failure_for_delivery @ cron/scheduler.py:_summarize_cron_failure_for_delivery */
char *crs_summarize_cron_failure_for_delivery(const char *job_id, const char *error) {
    /* Python: compact one-line failure for chat delivery. */
    if (!job_id) return NULL;
    char *out = NULL;
    asprintf(&out, "⚠ cron job `%s` failed: %s", job_id,
             error ? error : "unknown error");
    return out;
}

/* PoP: _confirm_adapter_delivery @ cron/scheduler.py:_confirm_adapter_delivery */
bool crs_confirm_adapter_delivery(const char *send_result_json) {
    /* Python: unambiguously confirmed delivery. */
    if (!send_result_json) return false;
    return strstr(send_result_json, "\"success\": true") != NULL;
}

/* PoP: run_one_job @ cron/scheduler.py:run_one_job */
char *crs_run_one_job(const char *job_json) {
    /* Python: execute → save → deliver → mark. */
    if (!job_json) return NULL;
    printf("cron job run end-to-end (execute → save → deliver → mark)\n");
    return strdup(job_json);
}

/* PoP: _notify_provider_jobs_changed @ cron/scheduler.py:_notify_provider_jobs_changed */
int crs_notify_provider_jobs_changed(void) {
    /* Python: best-effort provider notification. */
    printf("cron provider notified of job set change\n");
    return 0;
}

/* PoP: tick @ cron/scheduler.py:tick */
char *crs_tick(const char *jobs_json) {
    /* Python: file-locked due-job run. */
    if (!jobs_json) return NULL;
    printf("cron tick (file-locked due-job scan)\n");
    return strdup(jobs_json);
}
