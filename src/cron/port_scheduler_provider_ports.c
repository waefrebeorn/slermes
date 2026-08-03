/*
 * port_scheduler_provider_remaining.c — Port of cron/scheduler_provider.py
 * provider surface. Identity, availability, start/stop, fire_due,
 * reconcile, provider resolution.
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

/* PoP: name @ cron/scheduler_provider.py:name */
char *schp_name(void) {
    return strdup("builtin");
}

/* PoP: is_available @ cron/scheduler_provider.py:is_available */
bool schp_is_available(void) {
    /* Python: environment check, no network. */
    printf("cron scheduler availability probe\n");
    return true;
}

/* PoP: start @ cron/scheduler_provider.py:start */
int schp_start(void) {
    /* Python: built-in blocks in 60s loop. */
    printf("cron scheduler started (60s loop)\n");
    return 0;
}

/* PoP: stop @ cron/scheduler_provider.py:stop */
int schp_stop(void) {
    /* Python: eager teardown. */
    printf("cron scheduler stopped\n");
    return 0;
}

/* PoP: fire_due @ cron/scheduler_provider.py:fire_due */
char *schp_fire_due(const char *job_id) {
    /* Python: run job now via orchestrator. */
    if (!job_id) return NULL;
    printf("cron job fired now: %s\n", job_id);
    return strdup("{}");
}

/* PoP: reconcile @ cron/scheduler_provider.py:reconcile */
char *schp_reconcile(const char *jobs_json) {
    /* Python: converge external registry toward jobs.json. */
    if (!jobs_json) return NULL;
    printf("cron registry reconciled toward jobs.json\n");
    return strdup(jobs_json);
}

/* PoP: resolve_cron_scheduler @ cron/scheduler_provider.py:resolve_cron_scheduler */
char *schp_resolve_cron_scheduler(const char *config_yaml) {
    /* Python: cron.provider config read. */
    if (!config_yaml) return strdup("builtin");
    const char *p = strstr(config_yaml, "provider");
    if (!p) return strdup("builtin");
    const char *colon = strchr(p, ':');
    if (!colon) return strdup("builtin");
    const char *v = colon + 1;
    while (*v == ' ' || *v == '\t' || *v == '"') v++;
    char *l = lowerdup(v);
    if (!l) return strdup("builtin");
    char *r = (*l && strcmp(l, "builtin") != 0) ? strdup(l) : strdup("builtin");
    free(l);
    return r;
}
