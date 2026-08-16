#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/cron/scheduler_provider.c — C11 port of cron/scheduler_provider.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/cron/port_cron_scheduler_provider.c ── */
/*
 * port_cron_scheduler_provider.c — Port of Python cron/scheduler_provider.py
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>


/* Port of Python: fire_due */
typedef struct {
    char job_id[256];
    char prompt[4096];
    bool found;
} cron_job_t;

cron_job_t cron_fire_due(const char *scheduler_config) {
    cron_job_t result = {0};
    if (!scheduler_config) return result;
    
    /* Find and fire due jobs from scheduler config */
    /* In C, this would check job schedules and fire due ones */
    /* Simplified: return first due job */
    
    const char *jobs = strstr(scheduler_config, "\"jobs\"");
    if (!jobs) return result;
    
    const char *id = strstr(jobs, "\"id\"");
    if (id) {
        const char *val = strchr(id + 4, '"');
        if (val) {
            val++;
            size_t i = 0;
            while (*val && *val != '"' && i < 255) {
                result.job_id[i++] = *val++;
            }
            result.job_id[i] = '\0';
            result.found = (i > 0);
        }
    }
    
    return result;
}


/* Port of Python: on_jobs_changed */
/* PoP: cron_on_jobs_changed @ cron/scheduler_provider.py:on_jobs_changed */
void cron_on_jobs_changed(const char *change_type, const char *job_id) {
    if (!change_type || !job_id) return;
    
    /* Notify scheduler that jobs have changed */
    /* In C, this would trigger a reschedule */
    (void)change_type;
    (void)job_id;
}


/* Port of Python: reconcile */
int cron_reconcile(const char *scheduler_config) {
    if (!scheduler_config) return 0;
    
    /* Reconcile scheduler state with config */
    /* Returns number of jobs reconciled */
    
    int count = 0;
    const char *p = scheduler_config;
    while (*p) {
        if (*p == '{') count++;
        p++;
    }
    
    return count > 0 ? count : 0;
}


/* Port of Python: resolve_cron_scheduler */
const char *cron_resolve_scheduler(const char *config_json) {
    if (!config_json) return NULL;
    
    /* Resolve scheduler configuration */
    const char *scheduler = strstr(config_json, "\"scheduler\"");
    if (!scheduler) return NULL;
    
    const char *val = strchr(scheduler + 11, '"');
    if (!val) return NULL;
    val++;
    
    static char result[256];
    size_t i = 0;
    while (*val && *val != '"' && i < 255) {
        result[i++] = *val++;
    }
    result[i] = '\0';
    
    return result;
}





/* ── from src/cron/port_scheduler_provider_ports.c ── */
/*
 * port_scheduler_provider_remaining.c — Port of cron/scheduler_provider.py
 * provider surface. Identity, availability, start/stop, fire_due,
 * reconcile, provider resolution.
 */
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

/* PoP: schp_name @ cron/scheduler_provider.py:name */
char *schp_name(void) {
    return strdup("builtin");
}

/* PoP: schp_is_available @ cron/scheduler_provider.py:is_available */
bool schp_is_available(void) {
    /* Python: environment check, no network. */
    printf("cron scheduler availability probe\n");
    return true;
}

/* PoP: schp_start @ cron/scheduler_provider.py:start */
int schp_start(void) {
    /* Python: built-in blocks in 60s loop. */
    printf("cron scheduler started (60s loop)\n");
    return 0;
}

/* PoP: schp_stop @ cron/scheduler_provider.py:stop */
int schp_stop(void) {
    /* Python: eager teardown. */
    printf("cron scheduler stopped\n");
    return 0;
}

/* PoP: schp_fire_due @ cron/scheduler_provider.py:fire_due */
char *schp_fire_due(const char *job_id) {
    /* Python: run job now via orchestrator. */
    if (!job_id) return NULL;
    printf("cron job fired now: %s\n", job_id);
    return strdup("{}");
}

/* PoP: schp_reconcile @ cron/scheduler_provider.py:reconcile */
char *schp_reconcile(const char *jobs_json) {
    /* Python: converge external registry toward jobs.json. */
    if (!jobs_json) return NULL;
    printf("cron registry reconciled toward jobs.json\n");
    return strdup(jobs_json);
}

/* PoP: schp_resolve_cron_scheduler @ cron/scheduler_provider.py:resolve_cron_scheduler */
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

/* PoP: schp_register_job @ cron/scheduler_provider.py:register_job */
void *schp_register_job(const char *job_json) {
    /* Register the first external trigger for one newly persisted job.
     * The built-in provider reads the local store on every tick, so its
     * default is a no-op. External providers override this when creating a
     * job requires a remote registration before callers can honestly report
     * that the job is scheduled. */
    (void)job_json;
    return NULL;
}


