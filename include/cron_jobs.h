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

/* Current local time as an ISO-8601 string ("YYYY-MM-DD HH:MM:SS"),
 * malloc'd (free()). Mirrors cron/scheduler.py:_hermes_now().isoformat().
 * Implemented in port_cron_jobs.c (now_iso). */
char *now_iso(void);

/* Cron silent-run marker, matching cron/scheduler.py:SILENT_MARKER. A final
 * response equal to this string marks a silent (no-delivery) run. */
#define SCHEDULER_SILENT_MARKER "[SILENT]"

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
/* Save all jobs (array). Returns true on success. Implements the
 * shrink-merge guard (#80624): under the degraded flock-timeout path,
 * jobs created on disk by a concurrent writer are merged back before the
 * atomic replace so they are not clobbered. */
bool cronjobs_save_jobs(const json_t *jobs);

/* ── Workdir / optional text normalization ────────────────────────── */
/* Normalize+validate a workdir. Returns malloc'd abs path, or NULL when
 * disabled/empty. On invalid input returns NULL and sets *err (malloc'd). */
char *cronjobs_normalize_workdir(const char *workdir, char **err);

/* ── Job state predicates ─────────────────────────────────────────── */
/* True when the record carries any operator-facing pause signal
 * (state=="paused" or paused_at set). Port of _has_pause_marker. */
bool cronjobs_has_pause_marker(const json_t *job);
/* True iff the scheduler may fire this job (enabled && !paused). Port of
 * is_job_runnable. */
bool cronjobs_is_job_runnable(const json_t *job);
/* Operator-facing state derived from the scheduler-honoured flag + pause
 * markers. Port of effective_job_state. */
char *cronjobs_effective_job_state(const json_t *job);

/* ── Execution-mode invariant validation ──────────────────────────── */
/* Validate monitor_script/monitor_url/no_agent/script combinations.
 * Returns a malloc'd error message when invalid, else NULL.
 * Port of _validate_job_mode_invariants. */
char *cronjobs_validate_job_mode_invariants(const char *monitor_script,
                                            const char *monitor_url,
                                            bool no_agent,
                                            const char *script);

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
    const char *monitor_script;  /* port of cron/jobs.py create_job monitor_script */
    const char *monitor_url;     /* port of cron/jobs.py create_job monitor_url   */
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
/* Refresh an owned one-shot run_claim (compare-and-refresh on owner). */
bool cronjobs_heartbeat_run_claim(const char *job_id,
                                  const char *expected_owner);
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

/* ── Internal cron/jobs.py helpers (credited via PoP) ────────────── */
/* Reduce one job skill reference to the bare name the curator matches on.
 * Best-effort: strips leading '/'; never fails. Port of _canonical_skill_ref. */
char *cronjobs_canonical_skill_ref(const char *raw);

/* Tolerantly parse jobs.json: utf-8-sig absorbs a BOM; a strict parse
 * failure is retried loose to survive bare control characters. Returns
 * the parsed top-level JSON node (json_free) or NULL (sets *err). Port of
 * _parse_jobs_file. */
json_t *cronjobs_parse_jobs_file(const char *path, bool *used_strict_fallback,
                                 char **err);

/* Best-effort read of on-disk jobs without repair side-effects. Caller must
 * hold the jobs lock. Returns a JSON array (json_free), or NULL when corrupt.
 * Port of _peek_jobs_unlocked. */
json_t *cronjobs_peek_jobs_unlocked(void);

/* Cheap change-detection stamp for jobs.json as a malloc'd string
 * "mtime_ns|size|ino". NULL when missing/unstatable. Port of _jobs_file_stamp. */
char *cronjobs_jobs_file_stamp(void);

/* Merge any on-disk jobs missing from the save payload (concurrent-create
 * clobber guard #80624). Returns a new array when recovery happened, else
 * returns `jobs` (json_free the result either way). Port of
 * _merge_unexpected_disk_jobs. */
json_t *cronjobs_merge_unexpected_disk_jobs(const json_t *jobs,
                                             const json_t *removed_ids);

/* Preflight alert-dedup marker helpers (persist on the job record so the
 * scheduler alerts exactly once per blocked config). Port of
 * _set_preflight_alerted / mark_preflight_alerted / clear_preflight_alerted. */
bool cronjobs_set_preflight_alerted(const char *job_id, bool value);
bool cronjobs_mark_preflight_alerted(const char *job_id);
void cronjobs_clear_preflight_alerted(const char *job_id);

#endif /* CRON_JOBS_H */
