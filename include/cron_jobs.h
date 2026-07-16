/*
 * cron_jobs.h — Cron job storage and management (C11 port of cron/jobs.py).
 *
 * Faithful port of the Python cron job store. Jobs live in
 * <SLERMES_HOME>/cron/jobs.json as {"jobs": [...], "updated_at": ISO}. Each
 * job is a JSON object; this module owns creating, loading, mutating, and
 * persisting them, plus schedule parsing and due-job computation.
 *
 * The API is JSON-oriented (json_t *) because a cron job is an open-ended
 * record whose shape must stay byte-compatible with the Python store. Callers
 * own returned json_t* nodes (json_free) unless noted. String returns are
 * malloc'd unless noted (free()).
 *
 * MIT License — Slermes Fork
 */
#ifndef CRON_JOBS_H
#define CRON_JOBS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* ── Constants (mirror cron/jobs.py) ──────────────────────────────── */
#define CRONJOBS_ONESHOT_GRACE_SECONDS 120
#define CRONJOBS_TICKER_INTERVAL_SECONDS 60
#define CRONJOBS_OUTPUT_DEFAULT_KEEP 50

/* ── Paths ────────────────────────────────────────────────────────── */
/* All return a malloc'd absolute path (free()). NULL on OOM. */
char *cronjobs_cron_dir(void);        /* <home>/cron */
char *cronjobs_jobs_file(void);       /* <home>/cron/jobs.json */
char *cronjobs_jobs_lock_file(void);  /* <home>/cron/.jobs.lock */
char *cronjobs_output_dir(void);      /* <home>/cron/output */
char *cronjobs_ticker_heartbeat_file(void);
char *cronjobs_ticker_success_file(void);
/* Resolve a job's output dir, rejecting path-escape. NULL on invalid id
 * (sets *err to a malloc'd message when err != NULL). */
char *cronjobs_job_output_dir(const char *job_id, char **err);

/* Ensure cron dirs exist with 0700 perms. Returns true on success. */
bool cronjobs_ensure_dirs(void);

/* ── Skill list normalization ─────────────────────────────────────── */
/* Merge legacy single `skill` + list `skills` into a unique ordered array.
 * Returns a JSON array of strings (json_free). `skill` may be NULL; `skills`
 * may be NULL, a JSON string, or a JSON array. */
json_t *cronjobs_normalize_skill_list(const char *skill, const json_t *skills);
/* Return a copy of `job` with canonical `skills` + legacy `skill` aligned. */
json_t *cronjobs_apply_skill_fields(const json_t *job);

/* ── Text coercion / display ──────────────────────────────────────── */
/* Coerce a nullable JSON value to a malloc'd string (fallback if null). */
char *cronjobs_coerce_job_text(const json_t *value, const char *fallback);
/* Best-effort human schedule label for a job (malloc'd, never NULL). */
char *cronjobs_schedule_display_for_job(const json_t *job);
/* Return a read-safe normalized copy of a job record (json_free). */
json_t *cronjobs_normalize_job_record(const json_t *job);

/* ── Schedule parsing ─────────────────────────────────────────────── */
/* Parse "30m"/"2h"/"1d" → minutes; -1 on invalid. */
int cronjobs_parse_duration(const char *s);
/* Parse a schedule string into a JSON object {kind, ...display}.
 * Returns NULL on invalid input (sets *err to malloc'd message). */
json_t *cronjobs_parse_schedule(const char *schedule, char **err);
/* Compute next run ISO from a schedule object + optional last_run_at.
 * Returns malloc'd ISO string, or NULL when no more runs. */
char *cronjobs_compute_next_run(const json_t *schedule, const char *last_run_at);
/* Grace window seconds for a schedule (catch-up tolerance). */
int cronjobs_compute_grace_seconds(const json_t *schedule);

/* ── Ticker heartbeat ─────────────────────────────────────────────── */
void cronjobs_record_ticker_heartbeat(bool success);
/* Seconds since heartbeat/success; returns <0 (-1) when unknown. */
double cronjobs_ticker_heartbeat_age(void);
double cronjobs_ticker_success_age(void);

/* ── Persistence ──────────────────────────────────────────────────── */
/* Load all jobs. Returns a JSON array (json_free). NULL on unrepairable
 * corruption (sets *err). Empty array when no store exists. */
json_t *cronjobs_load_jobs(char **err);
/* Save all jobs (array). Returns true on success. */
bool cronjobs_save_jobs(const json_t *jobs);

/* ── Workdir / optional text normalization ────────────────────────── */
/* Normalize+validate a workdir. Returns malloc'd abs path, or NULL when
 * disabled/empty. On invalid input returns NULL and sets *err (malloc'd). */
char *cronjobs_normalize_workdir(const char *workdir, char **err);

/* ── CRUD ─────────────────────────────────────────────────────────── */
/* Options struct for create. All pointers optional (NULL = unset). */
typedef struct {
    const char *prompt;
    const char *schedule;   /* required */
    const char *name;
    int repeat;             /* <=0 treated as "unset/forever" unless has_repeat */
    bool has_repeat;
    const char *deliver;
    const json_t *origin;
    const char *skill;
    const json_t *skills;   /* array or NULL */
    const char *model;
    const char *provider;
    const char *base_url;
    const char *script;
    const json_t *context_from; /* string, array, or NULL */
    const json_t *enabled_toolsets;
    const char *workdir;
    bool no_agent;
    int attach_to_session;  /* -1 unset, 0 false, 1 true */
} cronjobs_create_opts;

/* Create a job; appends to store and returns the created record (json_free).
 * Returns NULL on error (sets *err). */
json_t *cronjobs_create_job(const cronjobs_create_opts *opts, char **err);

/* Get a normalized job by exact id, or NULL. */
json_t *cronjobs_get_job(const char *job_id);
/* Resolve a ref (id or case-insensitive name) → normalized record.
 * On ambiguous name match returns NULL and sets *ambiguous=true. */
json_t *cronjobs_resolve_job_ref(const char *ref, bool *ambiguous);
/* List jobs (normalized). include_disabled=false filters enabled only. */
json_t *cronjobs_list_jobs(bool include_disabled);
/* Update a job by id with a JSON object of updates. Returns normalized
 * updated record, or NULL if not found (sets *err on immutable-field error). */
json_t *cronjobs_update_job(const char *job_id, const json_t *updates, char **err);
json_t *cronjobs_pause_job(const char *job_id, const char *reason);
json_t *cronjobs_resume_job(const char *job_id);
json_t *cronjobs_trigger_job(const char *job_id);
bool    cronjobs_remove_job(const char *job_id);

/* ── Run bookkeeping ──────────────────────────────────────────────── */
void cronjobs_mark_job_run(const char *job_id, bool success,
                           const char *error, const char *delivery_error);
bool cronjobs_claim_dispatch(const char *job_id);
bool cronjobs_advance_next_run(const char *job_id);
bool cronjobs_claim_job_for_fire(const char *job_id, int claim_ttl_seconds);
/* Return jobs due now (array of normalized job dicts, json_free). */
json_t *cronjobs_get_due_jobs(void);

/* ── Output ───────────────────────────────────────────────────────── */
/* Save job output to <output>/<job>/<ts>.md. Returns malloc'd path or NULL. */
char *cronjobs_save_job_output(const char *job_id, const char *output);

/* ── Skill-ref maintenance ────────────────────────────────────────── */
/* Return a JSON array of unique skill names referenced by any job. */
json_t *cronjobs_referenced_skill_names(void);
/* Rewrite skill refs after curator consolidation/prune. `consolidated` is a
 * JSON object old→new; `pruned` a JSON array of names. Returns a report
 * object {rewrites, jobs_updated, jobs_scanned} (json_free). */
json_t *cronjobs_rewrite_skill_refs(const json_t *consolidated, const json_t *pruned);

#endif /* CRON_JOBS_H */
