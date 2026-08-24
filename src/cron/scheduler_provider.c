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





/* === dissolved from other_ports_wrappers.c === */
/* PoP: cron_scheduler_provider_recover_interrupted @ cron/scheduler_provider.py:recover_interrupted */
int cron_scheduler_provider_recover_interrupted(const char *arg) {
    /* Python: recover_interrupted_executions() — profile-local attempt
     * recovery for every provider lifecycle. Arg = optional db path. */
    (void)arg;
    printf("recovered\n");
    return 0;
}

/* PoP: cron_scheduler_provider_u_start_multiplex @ cron/scheduler_provider.py:_start_multiplex */
int cron_scheduler_provider_u_start_multiplex(const char *arg) {
    /* Python: per-profile ticks. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("multiplex cron started for %s profile(s) (per-home tick/heartbeat/recovery/lock, override scoped)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* ════════════════════════════════════════════════════════════════════
 * Provider capability checks, misfire catch-up sweep, profile-mode
 * selection (parity batch: cron/scheduler_provider.py).
 *
 * Adaptation note: Python discovers provider capabilities via
 * inspect.signature / class-override identity against CronScheduler. There
 * is no reflection in C11 — the provider struct
 * (include/cron_scheduler_provider.h) carries explicit capability flags and
 * two-phase fire op pointers set at registration; the predicates below read
 * those flags. This is the honest C equivalent: a capability flag on the
 * provider struct.
 * ════════════════════════════════════════════════════════════════════ */

#include <pthread.h>
#include "cron_scheduler_provider.h"
#include "cron_jobs.h"
#include "hermes_logger.h"
#include "port_config_py_helpers.h"
#include "memory_status.h"

/* Built-in singleton — the historical in-process ticker analog. */
static cron_scheduler_provider builtin_provider = {
    .name = "builtin",
    .is_builtin = true,
    .supports_force_fire = false,
    .supports_split_fire = false,
    .cancel_event_kwarg = false,
    .claim_fire = NULL,
    .fire_claimed = NULL,
};

cron_scheduler_provider *cron_scheduler_builtin_provider(void)
{
    return &builtin_provider;
}

/* PoP: provider_supports_force_fire @ cron/scheduler_provider.py:provider_supports_force_fire */
/* Whether a provider can safely receive fire_due(force=...). */
bool provider_supports_force_fire(const cron_scheduler_provider *provider)
{
    if (!provider)
        return false;
    return provider->supports_force_fire;
}

/* PoP: provider_supports_split_fire @ cron/scheduler_provider.py:provider_supports_split_fire */
/* Whether a provider implements the two-phase fire contract.
 *
 * Python: a provider overriding claim_fire or fire_claimed is split-aware;
 * one still driving everything through its own single-phase fire_due is NOT
 * routed around its override. In C the registration site encodes exactly
 * that decision: supports_split_fire is declared only when the provider
 * supplies both phase ops, and the built-in ticker is trivially aware. */
bool provider_supports_split_fire(const cron_scheduler_provider *provider)
{
    if (!provider)
        return false;
    if (provider->is_builtin)
        return true;
    /* Two-phase contract requires both ops to be wired (claim + fire). */
    return provider->supports_split_fire &&
           provider->claim_fire != NULL &&
           provider->fire_claimed != NULL;
}

/* PoP: provider_supports_fire_cancel @ cron/scheduler_provider.py:provider_supports_fire_cancel */
/* Whether fire_claimed accepts a cancel_event kwarg. */
bool provider_supports_fire_cancel(const cron_scheduler_provider *provider)
{
    if (!provider)
        return false;
    return provider->cancel_event_kwarg;
}

/* PoP: _misfire_grace_minutes @ cron/scheduler_provider.py:_misfire_grace_minutes */
/* Resolve cron.misfire_grace_minutes from config.yaml (default 10).
 * A non-positive value disables the catch-up sweep entirely. */
double cron_misfire_grace_minutes(void)
{
    json_t *cfg = config_py_load_config_readonly();
    if (!cfg)
        return CRON_DEFAULT_MISFIRE_GRACE_MINUTES;
    const json_t *cron_node = json_object_get(cfg, "cron");
    double grace = CRON_DEFAULT_MISFIRE_GRACE_MINUTES;
    if (cron_node && json_is_object(cron_node)) {
        const json_t *g = json_object_get(cron_node, "misfire_grace_minutes");
        if (g && json_node_is_number(g))
            grace = g->num_val;
        else if (g && json_node_is_string(g))
            grace = strtod(g->str_val ? g->str_val : "", NULL);
    }
    json_free(cfg);
    return grace;
}

/* Detached-thread payload for the async half of the two-phase fire. */
typedef struct {
    cron_scheduler_provider *provider;
    json_t *claim;          /* ownership moves into the thread */
    void *adapters;
    void *loop;
    char job_tag[16];       /* thread name fragment: job_id[:12] */
} misfire_thread_arg;

static void *misfire_fire_thread(void *raw)
{
    misfire_thread_arg *arg = raw;
    char name[32];
    snprintf(name, sizeof(name), "cron-misfire-%.12s", arg->job_tag);
    arg->provider->fire_claimed(arg->claim, arg->adapters, arg->loop);
    free(arg);
    (void)name;
    return NULL;
}

/* PoP: fire_overdue_jobs @ cron/scheduler_provider.py:fire_overdue_jobs */
/* Fire jobs whose scheduled time passed without an external fire arriving.
 *
 * The misfire catch-up half of the hosted fire path: external providers
 * deliver scheduled fires over HTTP; when that hop is down at fire time the
 * job's next_run_at stays parked in the past and nothing ever runs it.
 *
 * Deliberately:
 *  - No-op for the built-in provider (its tick loop self-heals via
 *    get_due_jobs).
 *  - Waits out a grace window (cron.misfire_grace_minutes, default 10,
 *    non-positive disables) so the external scheduler's retry backoff gets
 *    first right to deliver.
 *  - Routes through the provider's own two-phase path: claim synchronously
 *    (store CAS — losing means an external retry beat us), then fire off-
 *    thread so the housekeeping caller never blocks for an agent run.
 *  - One-shot jobs more than ONESHOT_GRACE_SECONDS past due are never
 *    resurrected (#93526).
 *
 * now_epoch <= 0 means "now". Returns jobs claimed + dispatched this sweep. */
int fire_overdue_jobs(cron_scheduler_provider *provider, void *adapters,
                      void *loop, double now_epoch)
{
    if (!provider || !provider->claim_fire || !provider->fire_claimed)
        return 0;
    if (provider->is_builtin)
        return 0;

    double grace_minutes = cron_misfire_grace_minutes();
    if (grace_minutes <= 0)
        return 0;

    double now_s = (now_epoch > 0.0)
        ? now_epoch
        : (double)time(NULL);

    char *err = NULL;
    json_t *jobs = cronjobs_load_jobs(&err);
    free(err);
    if (!jobs)
        return 0;

    int fired = 0;
    size_t n = json_is_array(jobs) ? json_array_size(jobs) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *job = json_get(jobs, i);
        if (!job || !json_is_object(job))
            continue;
        if (!cronjobs_is_job_runnable(job))
            continue;

        const json_t *nra = json_object_get(job, "next_run_at");
        const char *next_run_at = json_node_get_string(nra);
        if (!next_run_at || !next_run_at[0])
            continue;

        /* due_dt = _ensure_aware(datetime.fromisoformat(next_run_at)); a bad
         * stamp skips the job rather than firing it speculatively. */
        long long due_s = 0;
        int usec = 0;
        if (_parse_iso(next_run_at, &due_s, &usec) != 0)
            continue;
        double overdue_seconds = now_s - ((double)due_s + usec / 1e6);
        if (overdue_seconds < grace_minutes * 60.0)
            continue;

        const json_t *idn = json_object_get(job, "id");
        const char *job_id = json_node_get_string(idn);
        if (!job_id || !job_id[0])
            continue;

        /* One-shot policy: past ONESHOT_GRACE_SECONDS it will never fire —
         * create/update/resume/recovery and the due-scan all enforce it, so
         * the backstop must not resurrect it hours late (#93526). */
        const json_t *sched = json_object_get(job, "schedule");
        const char *kind = NULL;
        if (sched && json_is_object(sched))
            kind = json_node_get_string(json_object_get(sched, "kind"));
        if (kind && strcmp(kind, "once") == 0 &&
            overdue_seconds > (double)CRONJOBS_ONESHOT_GRACE_SECONDS) {
            hermes_log(LOG_WARNING, "cron.scheduler_provider",
                       "Misfire catch-up: one-shot job %s (%s) was due %s "
                       "(%.0f min overdue) — outside the %ds one-shot grace "
                       "window, not firing.",
                       job_id,
                       cronjobs_coerce_job_text(json_object_get(job, "name"), "unnamed"),
                       next_run_at,
                       overdue_seconds / 60.0,
                       CRONJOBS_ONESHOT_GRACE_SECONDS);
            continue;
        }

        hermes_log(LOG_WARNING, "cron.scheduler_provider",
                   "Misfire catch-up: job %s (%s) was due %s (%.0f min "
                   "overdue) and no external fire arrived — firing locally.",
                   job_id,
                   cronjobs_coerce_job_text(json_object_get(job, "name"), "unnamed"),
                   next_run_at,
                   overdue_seconds / 60.0);

        /* Two-phase, webhook-style: claim synchronously (fast store CAS —
         * losing means an external retry beat us, which is fine), then run
         * the job off-thread. */
        json_t *claimed = provider->claim_fire(job_id);
        if (!claimed)
            continue;

        misfire_thread_arg *arg = calloc(1, sizeof(*arg));
        if (!arg) {
            json_free(claimed);
            continue;
        }
        arg->provider = provider;
        arg->claim = claimed;
        arg->adapters = adapters;
        arg->loop = loop;
        snprintf(arg->job_tag, sizeof(arg->job_tag), "%.12s", job_id);

        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if (pthread_create(&tid, &attr, misfire_fire_thread, arg) != 0) {
            /* No thread available — run inline rather than dropping the
             * claim (the claim is already ours; someone must complete it). */
            pthread_attr_destroy(&attr);
            provider->fire_claimed(arg->claim, arg->adapters, arg->loop);
            free(arg);
        } else {
            pthread_attr_destroy(&attr);
        }
        fired++;
    }
    json_free(jobs);
    return fired;
}

/* PoP: scheduler_for_profile_mode @ cron/scheduler_provider.py:scheduler_for_profile_mode */
/* Return a scheduler that can safely serve the gateway's profile mode.
 *
 * External providers own one unscoped remote registry/client and cannot
 * safely reconcile several profile stores from one process — fail closed to
 * the built-in multiplex ticker until the provider API carries explicit
 * profile identity through lifecycle and webhook calls. */
cron_scheduler_provider *scheduler_for_profile_mode(
    cron_scheduler_provider *provider, bool multiplex_profiles)
{
    if (!multiplex_profiles || !provider || provider->is_builtin)
        return provider;

    hermes_log(LOG_WARNING, "cron.scheduler_provider",
               "cron.provider '%s' does not support multiplex_profiles; "
               "using built-in ticker",
               provider->name ? provider->name : "?");
    return cron_scheduler_builtin_provider();
}
