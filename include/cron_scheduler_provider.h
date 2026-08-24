/*
 * cron_scheduler_provider.h — Cron scheduler provider ports
 * (cron/scheduler_provider.py).
 *
 * Provider capability predicates (honest adaptation of Python's
 * inspect.signature probes: C providers carry explicit capability flags on
 * their struct instead), the misfire catch-up sweep, and profile-mode
 * selection.
 *
 * MIT License — Slermes Fork
 */
#ifndef SLERMES_CRON_SCHEDULER_PROVIDER_H
#define SLERMES_CRON_SCHEDULER_PROVIDER_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CRON_DEFAULT_MISFIRE_GRACE_MINUTES 10.0

/* C adaptation of the CronScheduler provider object. Python discovers
 * capabilities by inspecting method signatures / class overrides; here the
 * registration site declares them explicitly. */
typedef struct cron_scheduler_provider {
    const char *name;          /* "builtin" for the in-process ticker */
    bool is_builtin;           /* InProcessCronScheduler analog */

    /* Capability flags — set at registration:
     *   supports_force_fire   fire_due() safely accepts force=
     *   supports_split_fire   implements claim_fire/fire_claimed two-phase
     *   cancel_event_kwarg    fire_claimed() accepts a cancel_event argument */
    bool supports_force_fire;
    bool supports_split_fire;
    bool cancel_event_kwarg;

    /* Two-phase fire ops (NULL when the provider is single-phase). claim_fire
     * returns a malloc'd claim node (ownership passes to fire_claimed). */
    json_t *(*claim_fire)(const char *job_id);
    void (*fire_claimed)(json_t *claim, void *adapters, void *loop);
} cron_scheduler_provider;

/* The built-in in-process ticker instance (shared singleton; its fields are
 * never mutated). */
cron_scheduler_provider *cron_scheduler_builtin_provider(void);

/* PoP: provider_supports_force_fire @ cron/scheduler_provider.py:provider_supports_force_fire
 * Whether a provider can safely receive fire_due(force=...). */
bool provider_supports_force_fire(const cron_scheduler_provider *provider);

/* PoP: provider_supports_split_fire @ cron/scheduler_provider.py:provider_supports_split_fire
 * Whether a provider implements the two-phase fire contract. */
bool provider_supports_split_fire(const cron_scheduler_provider *provider);

/* PoP: provider_supports_fire_cancel @ cron/scheduler_provider.py:provider_supports_fire_cancel
 * Whether fire_claimed accepts a cancel_event kwarg. */
bool provider_supports_fire_cancel(const cron_scheduler_provider *provider);

/* Resolve the misfire catch-up grace window from config:
 * cron.misfire_grace_minutes (default 10; non-positive disables). */
double cron_misfire_grace_minutes(void);

/* PoP: fire_overdue_jobs @ cron/scheduler_provider.py:fire_overdue_jobs
 * Fire jobs whose scheduled time passed without an external fire arriving.
 * now_epoch <= 0 → current time. adapters/loop are forwarded to the
 * provider's fire_claimed op. Returns jobs claimed + dispatched this sweep.
 * No-op for the built-in provider (its tick loop self-heals). */
int fire_overdue_jobs(cron_scheduler_provider *provider, void *adapters,
                      void *loop, double now_epoch);

/* PoP: scheduler_for_profile_mode @ cron/scheduler_provider.py:scheduler_for_profile_mode
 * Return a scheduler that can safely serve the gateway's profile mode.
 * External providers own one unscoped remote registry, so multiplex mode
 * fails closed to the built-in multiplex ticker. */
cron_scheduler_provider *scheduler_for_profile_mode(
    cron_scheduler_provider *provider, bool multiplex_profiles);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_CRON_SCHEDULER_PROVIDER_H */
