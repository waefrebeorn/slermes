/*
 * cron_scheduler_runtime.h — faithful C11 port of the cron/scheduler.py
 * RUNTIME surface: job execution machinery, thread pools, interruption
 * bookkeeping, the workdir readers-writer lock, toolset resolution, the
 * script runner + wake gate, prompt assembly, and delivery orchestration.
 *
 * Complements cron_scheduler_delivery.h (pure routing transforms) and
 * cron_scheduler_helpers.h (silence/summarize). No god headers: json via
 * the project json_t forward decl, session store via opaque db_t.
 *
 * Implemented across:
 *   src/cron/port_cron_scheduler_runtime.c   (locks/pools/interruption/title)
 *   src/cron/port_cron_scheduler_toolsets.c  (cron toolset resolution)
 *   src/cron/port_cron_scheduler_script.c    (script runner, heartbeat, gate)
 *   src/cron/port_cron_scheduler_prompt.c    (prompt assembly + scanners)
 *   src/cron/port_cron_scheduler_deliver.c   (mirror/seed/media/deliver/run)
 */

#ifndef CRON_SCHEDULER_RUNTIME_H
#define CRON_SCHEDULER_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_t json_t;
typedef struct db_t db_t;   /* opaque, lib/libdb session store */

/* ================================================================
 * Session titling (cron finally-block write)
 * ================================================================ */

/* PoP-mirrored port of _set_cron_session_title. Persists a non-blank,
 * unique title before the session closes. On a unique-title conflict the
 * next title in the lineage is used ("base" -> "base #2"). Returns the
 * malloc'd title actually persisted, or NULL when nothing could be set
 * (missing db/session/blank title, or an unrecoverable conflict — the
 * Python re-raise path — in which case *conflict_out is set true). */
char *scheduler_set_cron_session_title(db_t *db, const char *session_id,
                                       const char *base_title,
                                       bool *conflict_out);

/* ================================================================
 * Interruption bookkeeping (running / interrupted job-id sets)
 * ================================================================ */

/* Internal registration used by the dispatch path (Python: _running_job_ids
 * add/discard inside _submit_with_guard / _process_job). */
void scheduler_running_jobs_add(const char *job_id);
void scheduler_running_jobs_remove(const char *job_id);

/* Snapshot of in-flight cron job IDs. Returns a malloc'd NULL-terminated
 * array of malloc'd strings (caller frees both); *out_n gets the count. */
char **scheduler_get_running_job_ids(size_t *out_n);

/* Mark every in-flight job interrupted (gateway shutdown path). Records
 * the IDs in the interrupted set BEFORE writing last_status via
 * cronjobs_mark_job_run(job, false, reason). Returns malloc'd
 * NULL-terminated array of the IDs actually marked (caller frees). */
char **scheduler_mark_running_jobs_interrupted(const char *reason,
                                               size_t *out_n);

/* Non-destructive peek at the interrupted flag. */
bool scheduler_is_interrupted(const char *job_id);

/* Check-and-clear the interrupted flag (recurring jobs reuse IDs). */
bool scheduler_consume_interrupted_flag(const char *job_id);

/* ================================================================
 * Writer-preferring readers-writer lock (_ReadWriteLock)
 * ================================================================ */

typedef struct scheduler_rwlock scheduler_rwlock_t;

scheduler_rwlock_t *scheduler_rwlock_new(void);
void scheduler_rwlock_free(scheduler_rwlock_t *lk);
void scheduler_rwlock_acquire_read(scheduler_rwlock_t *lk);
void scheduler_rwlock_release_read(scheduler_rwlock_t *lk);
void scheduler_rwlock_acquire_write(scheduler_rwlock_t *lk);
void scheduler_rwlock_release_write(scheduler_rwlock_t *lk);

/* The process-global TERMINAL_CWD serialization lock (Python
 * _terminal_cwd_lock module global). */
scheduler_rwlock_t *scheduler_terminal_cwd_lock(void);

/* ================================================================
 * Persistent thread pools (parallel + sequential)
 * ================================================================ */

typedef struct scheduler_pool scheduler_pool_t;
typedef void (*scheduler_task_fn)(void *arg);

/* Return (or create) the persistent parallel pool. A change in
 * max_workers replaces the pool (old one is shut down without waiting),
 * exactly like the Python global. max_workers <= 0 means the CPython
 * ThreadPoolExecutor default (min(32, ncpu+4)). */
scheduler_pool_t *scheduler_get_parallel_pool(int max_workers);

/* Return (or create) the persistent single-worker sequential pool. */
scheduler_pool_t *scheduler_get_sequential_pool(void);

/* Submit fire-and-forget work. Returns false when the pool refuses new
 * work (shutting down) — the C analogue of the RuntimeError. */
bool scheduler_pool_submit(scheduler_pool_t *pool,
                           scheduler_task_fn fn, void *arg);

/* Shut down both persistent pools, waiting for queued work (atexit). */
void scheduler_shutdown_parallel_pool(void);

/* True when process teardown began (_interpreter_shutting_down). The
 * optional error text is matched for the "cannot schedule new futures"
 * prefix, mirroring the Python race fallback. */
bool scheduler_interpreter_shutting_down(const char *error_text);

/* Flag teardown started (called from the shutdown/atexit path). */
void scheduler_note_interpreter_shutdown(void);

/* ================================================================
 * Lock paths + plugin env accessor
 * ================================================================ */

/* Resolve cron lock paths at call time (profile/env honored).
 * Fills malloc'd strings *lock_dir_out and *tick_lock_out (caller frees). */
void scheduler_get_lock_paths(char **lock_dir_out, char **tick_lock_out);

/* Cron home-channel env var registered by a plugin platform, or ""
 * (static storage, do not free). Port of _plugin_cron_env_var. */
const char *scheduler_plugin_cron_env_var(const char *platform_name);

/* ================================================================
 * Toolset resolution (port_cron_scheduler_toolsets.c)
 * ================================================================ */

/* Toolsets a cron-spawned agent must never receive: the 3 protected ones
 * layered with agent.disabled_toolsets from config. Returns malloc'd
 * NULL-terminated array of malloc'd strings. */
char **scheduler_resolve_cron_disabled_toolsets(const json_t *cfg,
                                                size_t *out_n);

/* Layer enabled MCP servers onto a per-job enabled_toolsets allowlist.
 * per_job is a NULL-terminated string array. Mirrors the no_mcp sentinel,
 * the named-server allowlist rule, and the sorted union. */
char **scheduler_merge_mcp_into_per_job_toolsets(const char *const *per_job,
                                                 size_t n_per_job,
                                                 const json_t *cfg,
                                                 size_t *out_n);

/* Resolve the toolset list for a cron job. Per-job enabled_toolsets wins
 * (with MCP layering); otherwise the "cron" platform config; NULL on
 * failure (caller loads the full default set). */
char **scheduler_resolve_cron_enabled_toolsets(const json_t *job,
                                               const json_t *cfg,
                                               size_t *out_n);

void scheduler_free_string_list(char **list, size_t n);

/* ================================================================
 * Script runner + wake gate (port_cron_scheduler_script.c)
 * ================================================================ */

/* Cron pre-run script timeout: patched module value -> env
 * HERMES_CRON_SCRIPT_TIMEOUT -> config cron.script_timeout_seconds ->
 * default 3600. */
int scheduler_get_script_timeout(void);
void scheduler_set_script_timeout_override(int seconds); /* tests */

/* Parse <venv>/pyvenv.cfg into a JSON object of lowercased keys.
 * Returns empty object on read failure (caller json_free). */
json_t *scheduler_read_pyvenv_cfg(const char *venv_dir);

/* Filter Hermes-managed secrets from a subprocess environment. Input is
 * a NULL-terminated "KEY=VALUE" vector (pass environ); returns a fresh
 * malloc'd NULL-terminated vector (caller frees deep). Port of
 * tools/environments/local.py:_sanitize_subprocess_env static core. */
char **scheduler_sanitize_subprocess_env(char *const *base_env);

/* Execute a cron job's data-collection script. Scripts must live under
 * HERMES_HOME/scripts (traversal/symlink guarded). .sh/.bash run via
 * bash, everything else via python3. Output is secret-redacted. Returns
 * success flag; *output_out gets malloc'd stdout or error message. */
bool scheduler_run_job_script(const char *script_path, const char *workdir,
                              char **output_out);

/* Same, keeping an owned one-shot run_claim fresh via a heartbeat thread
 * (60s period) while the script runs. */
bool scheduler_run_job_script_with_claim_heartbeat(const json_t *job,
                                                   const char *script_path,
                                                   const char *workdir,
                                                   char **output_out);

/* Wake gate: last non-empty stdout line parsed as JSON;
 * {"wakeAgent": false} => false (skip agent); anything else => true. */
bool scheduler_parse_wake_gate(const char *script_output);

/* ================================================================
 * Prompt assembly (port_cron_scheduler_prompt.c)
 * ================================================================ */

/* Scan the fully-assembled cron prompt. Returns the malloc'd (possibly
 * sanitized) prompt on pass; NULL on block with *error_out set to the
 * malloc'd scanner error (CronPromptInjectionBlocked). */
char *scheduler_scan_assembled_cron_prompt(const char *assembled,
                                           const json_t *job,
                                           bool has_skills,
                                           bool has_injected_data,
                                           const char *user_prompt,
                                           char **error_out);

/* Build the effective prompt for a cron job. prerun_ok/prerun_output
 * carry an already-executed script result (wake-gate path); pass
 * prerun_output=NULL to run the script inline. Returns malloc'd prompt,
 * or NULL with *silent_out=true when the script produced no output
 * (skip AI call), or NULL with *error_out set when blocked. */
char *scheduler_build_job_prompt(const json_t *job,
                                 bool prerun_ok, const char *prerun_output,
                                 bool *silent_out, char **error_out);

/* Fail closed if the job's stored provider/base_url pair would
 * exfiltrate a credential. Returns NULL when safe, malloc'd error when
 * blocked (RuntimeError path). */
char *scheduler_guard_job_credential_exfil(const json_t *job);

/* Safe provenance suffix for security logs (" origin_platform='x' ..."
 * or ""). Returns malloc'd string. */
char *scheduler_cron_job_origin_log_suffix(const json_t *job);

/* ================================================================
 * Delivery + mirror + run (port_cron_scheduler_runtime_impl.c)
 * ================================================================ */

/* Media routing decision for _send_media_via_adapter. */
typedef enum {
    SCHEDULER_MEDIA_VOICE = 0,
    SCHEDULER_MEDIA_VIDEO,
    SCHEDULER_MEDIA_IMAGE,
    SCHEDULER_MEDIA_DOCUMENT,
} scheduler_media_kind_t;

/* Pure routing: extension + platform + voice flag -> adapter method. */
scheduler_media_kind_t scheduler_route_media(const char *platform,
                                             const char *media_path,
                                             bool is_voice);

/* Failure text builder for a failed cron delivery. malloc'd, redacted. */
char *scheduler_summarize_cron_failure_for_delivery(const char *job_id,
                                                    const char *error);

/* SendResult contract guard: true when a live adapter send_result indicates
 * success (status=="delivered" or success==true). */
int scheduler_confirm_adapter_delivery(const json_t *send_result);

/* Adapter surface for live sends (C analogue of the duck-typed Python
 * adapter). Any member may be NULL (capability missing). */
typedef struct {
    bool (*send_text)(const char *chat_id, const char *text,
                      const char *thread_id, void *ctx);
    bool (*send_voice)(const char *chat_id, const char *path, void *ctx);
    bool (*send_video)(const char *chat_id, const char *path, void *ctx);
    bool (*send_image)(const char *chat_id, const char *path, void *ctx);
    bool (*send_document)(const char *chat_id, const char *path, void *ctx);
    /* get_chat_info probe: fills type_out ("channel"/"group"/...) and
     * returns true on success. */
    bool (*get_chat_info)(const char *chat_id, char *type_out, size_t sz,
                          void *ctx);
    /* create_handoff_thread: returns malloc'd new thread id or NULL. */
    char *(*create_thread)(const char *chat_id, const char *name, void *ctx);
    void *ctx;
} scheduler_adapter_t;

/* Send extracted MEDIA files as native attachments. media_paths is a
 * NULL-terminated array; voice_flags parallel (may be NULL = all false).
 * Returns number of successful sends. */
int scheduler_send_media_via_adapter(const scheduler_adapter_t *adapter,
                                     const char *platform,
                                     const char *chat_id,
                                     const char *const *media_paths,
                                     const bool *voice_flags,
                                     const json_t *job);

/* Telegram ambiguous-topic disambiguation: probe chat type via the
 * adapter; only a "channel" chat routes via direct_messages_topic_id.
 * Fails safe to false. */
bool scheduler_is_channel_dm_topic(const scheduler_adapter_t *adapter,
                                   const char *chat_id, const char *job_id);

/* Best-effort mirror of a cron delivery into the origin chat's session.
 * No-op unless enabled. Prefixes "[Cron delivery: <name>]". */
void scheduler_maybe_mirror_cron_delivery(const json_t *job,
                                          const char *platform,
                                          const char *chat_id,
                                          const char *mirror_text,
                                          const char *thread_id,
                                          const char *user_id,
                                          bool enabled);

/* Open a dedicated thread for a continuable cron job. Returns malloc'd
 * thread id or NULL (fall back to DM mirror). */
char *scheduler_open_continuable_cron_thread(const json_t *job,
                                             const scheduler_adapter_t *adapter,
                                             const char *chat_id);

/* Seed the freshly-opened cron thread's session with the brief. */
void scheduler_seed_cron_thread_session(const json_t *job,
                                        const char *platform,
                                        const char *chat_id,
                                        const char *thread_id,
                                        const char *mirror_text);

/* Seed the FLAT (thread-less) session for an in_channel delivery.
 * Returns true when a seed row was created and the brief mirrored. */
bool scheduler_seed_cron_channel_session(const json_t *job,
                                         const char *platform,
                                         const char *chat_id,
                                         const char *mirror_text,
                                         bool is_dm, const char *user_id);

/* Deliver job output to the configured target(s). Uses the adapter when
 * given, else the registered platform send path. Returns NULL on
 * success (or a legitimately silent run) or a malloc'd error string. */
char *scheduler_deliver_result(const json_t *job, const char *content,
                               const scheduler_adapter_t *adapter);

/* Full no_agent run path of run_job: the script IS the job. Returns
 * success; fills malloc'd *doc_out, *final_out, *error_out (any may be
 * set NULL). final_out=="[SILENT]" marks a silent run. */
bool scheduler_run_job_no_agent(const json_t *job, char **doc_out,
                                char **final_out, char **error_out);

/* Agent runner callback for the LLM path: given the assembled prompt,
 * produce the malloc'd final response (NULL = failure w/ *err set). */
typedef char *(*scheduler_agent_run_fn)(const char *prompt,
                                        const json_t *job,
                                        char **err_out, void *ctx);

/* run_job: execute a single cron job. no_agent short-circuit, wake
 * gate, prompt assembly (injection-scanned), credential-exfil guard,
 * then the agent callback; interruption flags honored before status is
 * reported. Returns success; fills the 4-tuple. */
bool scheduler_run_job(const json_t *job,
                       scheduler_agent_run_fn agent_run, void *agent_ctx,
                       char **doc_out, char **final_out, char **error_out);

#ifdef __cplusplus
}
#endif

#endif /* CRON_SCHEDULER_RUNTIME_H */
