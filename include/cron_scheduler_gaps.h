/*
 * cron_scheduler_gaps.h — faithful C11 ports of the remaining REAL_GAP
 * functions of cron/scheduler.py (parity batch). Complements
 * cron_scheduler_runtime.h / cron_scheduler_delivery.h /
 * cron_scheduler_helpers.h.
 *
 * Python-side seams map to C as:
 *   concurrent.futures.Future + SessionDB  -> (void *db, void (*close)(void*))
 *   threading.Event                        -> scheduler_gap_event_t
 *   subprocess.Popen                       -> pid_t (+ optional pipe fds)
 *   exception cause chain                  -> scheduler_gap_exc_t linked list
 *   cleanup callable                       -> void (*)(void*) + ctx
 */

#ifndef CRON_SCHEDULER_GAPS_H
#define CRON_SCHEDULER_GAPS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_t json_t;

/* ── Duck-typed cancel event (Python threading.Event / _CancelEventLike) ── */
typedef struct scheduler_gap_event {
    bool is_set;                 /* guarded by internal mutex              */
    void *_impl;                 /* owned pthread_mutex_t                  */
} scheduler_gap_event_t;

/* ── Exception-chain node for _is_transient_provider_resolve_error ─────── */
typedef struct scheduler_gap_exc {
    const char *type_name;     /* type(cur).__name__  (NULL allowed)      */
    const char *type_module;   /* type(cur).__module__ (NULL allowed)     */
    const char *message;       /* str(cur)            (NULL allowed)      */
    int err_no;                /* errno attribute; -1 when absent          */
    bool is_oserror;           /* isinstance(cur, OSError)                 */
    bool is_gaierror;          /* isinstance(cur, socket.gaierror)         */
    const struct scheduler_gap_exc *cause_or_context; /* __cause__/__context__ */
} scheduler_gap_exc_t;

/* ================================================================
 * 1. _close_late_session_db_result
 * ================================================================ */
/* Done-callback: close a SessionDB whose constructor finished after the
 * run_job timeout. db may be NULL (Future result None); close errors are
 * swallowed exactly like Python's bare `except Exception`. */
void scheduler_close_late_session_db_result(void *db, void (*db_close)(void *db));

/* ================================================================
 * 2. _failure_streak_nudge
 * ================================================================ */
/* Review nudge when a recurring job keeps failing; "" (empty malloc'd
 * string) when not applicable. cron.failure_nudge_threshold config
 * (default 3, <=0 disables); one-shot jobs never nudge. Caller frees. */
char *scheduler_failure_streak_nudge(const json_t *job);

/* ================================================================
 * 3. _resolve_job_reasoning_config
 * ================================================================ */
/* Per-job reasoning_effort pin wins over config resolution; an invalid
 * stored pin logs + falls back to resolve_reasoning_config(cfg, model).
 * Returns malloc'd json_t reasoning-config (caller frees) or NULL when
 * unset/unrecognized (provider default), byte-identical to the Python
 * fallback path. */
json_t *scheduler_resolve_job_reasoning_config(const json_t *job,
                                               const json_t *cfg,
                                               const char *model);

/* ================================================================
 * 4. _inflight_min_allowance_minutes
 * ================================================================ */
/* Floor for the stale in-flight allowance, in minutes. Config
 * cron.inflight_max_minutes wins, then env HERMES_CRON_INFLIGHT_MAX_MINUTES,
 * then the module default 30.0. Only positive values are honored. */
double scheduler_inflight_min_allowance_minutes(void);

/* ================================================================
 * 5. _cron_interval_minutes
 * ================================================================ */
/* Approximate a cron expression's natural interval in minutes by taking
 * the gap between the next two fire times (libcron = croniter). Results
 * are cached per expression like Python's _cron_interval_cache. Returns
 * false (-> floor allowance) when the expression cannot be evaluated or
 * the gap is non-positive. */
bool scheduler_cron_interval_minutes(const char *expr, double *out_minutes);
/* Test hook: drop every cached entry. */
void scheduler_cron_interval_cache_clear(void);

/* ================================================================
 * 6. _job_interval_minutes
 * ================================================================ */
/* Best-effort interval length for a job in minutes from the PERSISTED
 * schedule dict shape ({kind: interval, minutes} | {kind: cron, expr});
 * a string schedule falls through parse_schedule defensively. kind ==
 * "once" (or unknown) -> false. */
bool scheduler_job_interval_minutes(const json_t *job, double *out_minutes);

/* ================================================================
 * 7-10. lock-contention / fd-exhaustion family
 * ================================================================ */
/* True when the lock syscall errno means the lock is held: POSIX flock
 * reports EWOULDBLOCK/EAGAIN (EACCES on some NFS implementations). */
bool scheduler_is_lock_contention_errno(int err);
/* Text-level half: "too many open files" / "emfile" (case-insensitive). */
bool scheduler_is_fd_exhaustion_text(const char *text);
/* errno EMFILE/ENFILE, else the text half over the exception repr. */
bool scheduler_is_fd_exhaustion(int err_no, bool is_oserror, const char *repr);
/* Best-effort FD reclamation: raise RLIMIT_NOFILE's soft limit toward the
 * hard limit (Python additionally gc.collect()s, which has no C
 * analogue — the collector IS the allocator here). Never raises. */
void scheduler_reclaim_fds_best_effort(void);

/* ================================================================
 * 11. _resolve_cron_surface_mode
 * ================================================================ */
/* Resolve the continuable-cron delivery surface ("in_channel" or
 * "thread") from a platform config's extra mapping. The relay sub-block
 * extra.<logical>.cron_continuable_surface wins over the flat key;
 * a sub-block that omits the knob falls back to the flat key. Pass the
 * pconfig's `extra` object directly (the C platform cfg stores it as
 * JSON); NULL extra -> "thread". */
const char *scheduler_resolve_cron_surface_mode(const json_t *extra,
                                                const char *logical_platform_name);

/* ================================================================
 * 12-14. bot-chat delivery lane
 * ================================================================ */
/* Timeout for one bot-chat delivery turn (config
 * cron.bot_chat_delivery_timeout_seconds; default 600; >0 enforced). */
int scheduler_get_bot_chat_delivery_timeout(void);
/* Deliver job output into a profile's canonical Bot Chat via
 * `hermes [-p <profile>] chat --in ~ -c "Bot Chat" --create-if-missing
 * -Q --query-file <tmp>`. profile=="" inherits this scheduler's
 * HERMES_HOME (and pops it from the child env so -p is unshadowed).
 * Returns NULL on success or a malloc'd error string for
 * last_delivery_error. The query temp file is always unlinked. */
char *scheduler_deliver_to_bot_chat(const json_t *job, const char *content,
                                    const char *profile);
/* Resolve a bot-chat deliver token to {platform, chat_id, thread_id}.
 * "" -> own profile target; otherwise the name must exist locally.
 * Returns malloc'd json_t or NULL (skip target). */
json_t *scheduler_resolve_bot_chat_target(const json_t *job,
                                          const char *profile_arg);

/* ================================================================
 * 15. _get_media_send_timeout
 * ================================================================ */
/* Env HERMES_CRON_MEDIA_SEND_TIMEOUT wins, then config
 * cron.media_send_timeout_seconds, then default 300. Positive only. */
int scheduler_get_media_send_timeout(void);

/* ================================================================
 * 16-17. script process teardown
 * ================================================================ */
/* Best-effort hard stop of a cron script and every child it spawned:
 * TERM the process group, wait 1s, probe group liveness, escalate to
 * KILL whenever ANY member survived, final bounded reap. */
void scheduler_terminate_cron_script_process(pid_t pid);
/* Reap a terminated script process without ever blocking indefinitely:
 * bounded 5s wait, kill on expiry, close the pipes, one more bounded
 * 5s wait (a wedged zombie is left to the OS reaper). */
void scheduler_drain_script_pipes(pid_t pid, int stdout_fd, int stderr_fd);

/* ================================================================
 * 18. _windows_cron_bootstrap_argv
 * ================================================================ */
/* Bootstrap argv for a cron script under the base interpreter with .pth
 * support: when <VIRTUAL_ENV>/Lib/site-packages exists, return
 * [python_exe, "-c", bootstrap, script_path] (site.addsitedir + runpy);
 * otherwise the plain [python_exe, script_path] fallback. env_overlay is
 * the job's environment overlay object (VIRTUAL_ENV key read from it).
 * Returns a malloc'd NULL-terminated vector of malloc'd strings
 * (*out_n receives the count, may be NULL). Never returns NULL. */
char **scheduler_windows_cron_bootstrap_argv(const char *python_exe,
                                             const json_t *env_overlay,
                                             const char *script_path,
                                             size_t *out_n);

/* ================================================================
 * 19. _block_and_pause_job
 * ================================================================ */
/* Fail a run closed and pause the job so it stops being scheduled.
 * Always returns false (run failed). Fills malloc'd *doc_out (markdown
 * run document), *alert_out (chat alert) and *err_out (= strdup(reason));
 * any out pointer may be NULL. */
bool scheduler_block_and_pause_job(const char *job_id, const char *job_name,
                                   const char *reason, char **doc_out,
                                   char **alert_out, char **err_out);

/* ================================================================
 * 20. _is_transient_provider_resolve_error
 * ================================================================ */
/* Walk the (linked) exception chain; true on transport-class names,
 * httpx/httpcore/aiohttp DNS/timeout messages, gaierror EAI_* codes,
 * transient OSErrors, or any level carrying the raw DNS text. */
bool scheduler_is_transient_provider_resolve_error(const scheduler_gap_exc_t *exc);

/* ================================================================
 * 21. _cron_preflight_enabled
 * ================================================================ */
/* Default ON; only the literal boolean false under cron.preflight opts
 * out. A missing/non-dict `cron` section keeps preflight enabled. */
bool scheduler_cron_preflight_enabled(const json_t *cfg);

/* ================================================================
 * 22-23. bounded post-run cleanup
 * ================================================================ */
/* Wall-clock bound for cron post-run cleanup (config
 * cron.cleanup_timeout_seconds >= 0; default 10.0). */
double scheduler_cron_cleanup_timeout_seconds(void);
/* Run fallible cleanup without permanently wedging a cron ID: a worker
 * thread executes it; timeout_seconds < 0 selects the configured bound,
 * <= 0 runs inline. Returns false on failure OR timeout (worker
 * abandoned — daemon semantics). */
bool scheduler_run_cron_cleanup_with_timeout(void (*cleanup)(void *arg),
                                             void *arg, const char *job_id,
                                             const char *label,
                                             double timeout_seconds);

/* ================================================================
 * 24. _BoundedCronSessionDB proxy
 * ================================================================ */
typedef struct scheduler_bounded_session_db {
    void *session_db;
    char *job_id;
    bool disabled;   /* latched after the first timeout */
} scheduler_bounded_session_db_t;

scheduler_bounded_session_db_t *scheduler_bounded_session_db_new(void *session_db,
                                                                 const char *job_id);
void scheduler_bounded_session_db_free(scheduler_bounded_session_db_t *b);
/* __getattr__/_bounded: run `target(ctx)` under the cleanup timeout with
 * the label "session finalization (<name>)". After a prior timeout the
 * proxy fails immediately (RuntimeError analogue: *err_out set). On
 * timeout without a raised error the damaged connection is disabled and
 * *err_out names the timed-out method. Returns true + *result_out when
 * target completed (target communicates its own result via ctx). */
bool scheduler_bounded_session_db_call(scheduler_bounded_session_db_t *b,
                                       const char *name,
                                       void (*target)(void *ctx), void *ctx,
                                       void **result_out, char **err_out);

/* ================================================================
 * 25. _run_with_fire_claim_heartbeat
 * ================================================================ */
/* Duck-typed event handed to `run` (Python passes lost_ownership, None,
 * or nothing): poll with scheduler_gap_event_is_set / set it with
 * scheduler_gap_event_set. */
bool scheduler_gap_event_is_set(const scheduler_gap_event_t *ev);
void scheduler_gap_event_set(scheduler_gap_event_t *ev);
void scheduler_gap_event_init(scheduler_gap_event_t *ev);
void scheduler_gap_event_destroy(scheduler_gap_event_t *ev);

/* Run `run(ctx, lost_ownership)` while keeping the job's owned durable
 * fire claim fresh via a 60s heartbeat thread (180s grace). No owner ->
 * run(NULL) straight away. Ownership lost before start closes the
 * unstarted execution ledger row (finish_execution callback) and returns
 * true. lost_ownership is NULL when the job carries no fire_claim.
 * `finish` mirrors _finish_unstarted (execution_id, error). */
bool scheduler_run_with_fire_claim_heartbeat(
    const json_t *job,
    bool (*run)(void *ctx, const scheduler_gap_event_t *lost_ownership),
    void *ctx,
    void (*finish)(void *finish_ctx, const char *execution_id,
                   const char *error),
    void *finish_ctx);

/* Heartbeat cadence constants (_RUN_CLAIM_HEARTBEAT_SECONDS and the 3x
 * grace), exposed for tests. */
double scheduler_run_claim_heartbeat_seconds(void);
double scheduler_fire_claim_heartbeat_grace_seconds(void);

/* ================================================================
 * 26. _run_one_job_body
 * ================================================================ */
/* Injected seams (Python imports resolved at call time). Execution-ledger
 * ops are injectable because the credited C executions layer is the
 * popen helper surface; passing NULL uses the no-op ledger seam. */
typedef struct scheduler_job_body_seams {
    /* create_execution(job_id, source="direct") -> malloc'd id or NULL */
    char *(*create_execution)(const char *job_id, void *ctx);
    void (*mark_execution_running)(const char *execution_id, void *ctx);
    void (*finish_execution)(const char *execution_id, bool success,
                             const char *error, const char *delivery_outcome,
                             void *ctx);
    void *ledger_ctx;

    /* claim_dispatch(job_id): atomic one-shot dispatch commit.
     * NULL seam = cronjobs_claim_dispatch. */
    bool (*claim_dispatch)(const char *job_id, void *ctx);
    void *claim_ctx;

    /* run_job(job, defer_agents, extra_prompt, cancel_event, ctx):
     * returns success; fills malloc'd output/final_response/error (any
     * may be NULL strings). cancel_event may be NULL. Agents whose
     * teardown run_job deferred are appended to defer_agents (opaque
     * handles torn down via teardown_agent afterwards). */
    bool (*run_job)(const json_t *job, void *defer_agents,
                    const char *extra_prompt,
                    const scheduler_gap_event_t *cancel_event,
                    char **output_out, char **final_out, char **error_out,
                    void *ctx);
    void *run_ctx;
    void (*teardown_agent)(void *agent_handle, const char *job_id, void *ctx);
    void *teardown_ctx;

    /* save_job_output(job_id, output) -> malloc'd path or NULL.
     * NULL seam = cronjobs_save_job_output. */
    char *(*save_job_output)(const char *job_id, const char *output, void *ctx);
    void *save_ctx;

    /* deliver(job, content, adapters, loop) -> malloc'd error or NULL.
     * NULL seam = scheduler_deliver_result(job, content, NULL). */
    char *(*deliver)(const json_t *job, const char *content, void *adapters,
                     void *loop, void *ctx);
    void *deliver_ctx;

    /* mark_job_run(job_id, success, error, delivery_error,
     * expected_fire_owner, status) -> false when owner-fenced write was
     * rejected. NULL seam = cronjobs_mark_job_run (returns true). */
    bool (*mark_job_run)(const char *job_id, bool success, const char *error,
                         const char *delivery_error,
                         const char *expected_fire_owner, const char *status,
                         void *ctx);
    void *mark_ctx;

    /* update_job(job_id, key, value). NULL seam = cronjobs_update_job. */
    bool (*update_job)(const char *job_id, const char *key, const char *value,
                       void *ctx);
    void *update_ctx;

    /* _resolve_delivery_targets(job) count for the unresolved-origin
     * probe (the credited C resolver runs on its own job struct, so the
     * JSON adapter is caller-supplied). NULL = 0 (unresolved). */
    int (*resolve_delivery_target_count)(const json_t *job, void *ctx);
} scheduler_job_body_seams_t;

/* The shared dispatch body executed by BOTH the built-in ticker and the
 * external provider. Mirrors _run_one_job_body: dispatch claim,
 * execution ledger, secret scope (no-op seam in C — scopes are ambient),
 * run_job, fire-claim ownership fences around save/delivery, blocked-
 * config / drift-skip alert-once contracts, silence suppression,
 * empty-response soft failure, interruption bookkeeping and terminal
 * writes. Returns the same bool contract as Python. */
bool scheduler_run_one_job_body(const json_t *job, void *adapters, void *loop,
                                bool verbose, const char *extra_prompt,
                                const scheduler_gap_event_t *fire_claim_lost,
                                const void *execution_token,
                                const scheduler_job_body_seams_t *seams);

/* ================================================================
 * 27. CronSchedulerRegistrationError.user_message
 * ================================================================ */
/* Human-facing variant (no exception class name). label = name || id. */
char *scheduler_registration_error_user_message(const json_t *job);

#ifdef __cplusplus
}
#endif

#endif /* CRON_SCHEDULER_GAPS_H */
