/*
 * terminal_tool_lifecycle_flow.h — command-execution lifecycle FLOW cores of
 * tools/terminal_tool.py and its live splits (terminal_tool_background.py,
 * terminal_tool_result.py, terminal_tool_config.py).
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception->fallback paths, mutation order, dict insertion order) of the live
 * modules. All live I/O (env.execute, process-registry spawn, approval scans,
 * config/scope reads, cwd resolution, redaction, truncation, logging, sleeps,
 * metrics) enters through the seam structs below; the cores perform no I/O.
 *
 * Sequences are observable through a ttlf_trace_t: seam implementations record
 * one JSON event per call (arguments + scripted result) in call order; the
 * oracle compares that ordered event list against recording monkeypatch stubs
 * driving the LIVE Python with the same scripted results.
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 * Python truthiness / or-chain / short-circuit order is preserved exactly.
 */

#ifndef TERMINAL_TOOL_LIFECYCLE_FLOW_H
#define TERMINAL_TOOL_LIFECYCLE_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Hard cap on foreground timeout (live FOREGROUND_MAX_TIMEOUT default). */
#define TTLF_FOREGROUND_MAX_TIMEOUT 600

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. ttlf_trace_add takes ownership of
 * ev (frees it when t is NULL). The trace never leaves the caller. */
typedef struct ttlf_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} ttlf_trace_t;

void ttlf_trace_add(ttlf_trace_t *t, json_t *ev);
void ttlf_trace_free(ttlf_trace_t *t);

/* ── 1. Error envelope: tools/terminal_tool.py:_error_json ──────────────────
 * error is a borrowed JSON value (string, or NULL for Python None -> null).
 * status is a JSON_STRING node or NULL (Python None -> omit). extra is a
 * borrowed object of additional keys (may be NULL); body.update(extra)
 * semantics: existing keys keep their position, values are replaced.
 * Returns malloc'd JSON string (caller frees). */
/* PoP: ttlf_error_json_flow @ tools/terminal_tool.py:_error_json */
char *ttlf_error_json_flow(const json_t *error, int exit_code,
                           const json_t *status, const json_t *extra);

/* ── 2. Approval decision chain: tools/terminal_tool.py:_run_approval_guards ─
 * Seams stand in for:
 *   docker_has_host_access -> tools/terminal_tool.py:_docker_has_host_access
 *   check_all_guards       -> tools/terminal_tool.py:_check_all_guards
 *                             (returns the BORROWED scripted approval dict)
 *   preapproval_note       -> tools/approval_provenance.py:preapproval_note
 *                             (malloc'd note; called only on the pre_approved
 *                             branch, with approval["pre_approved"] borrowed)
 * Returns a NEW verdict object {note: string|null, approved_run: bool}
 * (caller frees), or NULL with *rejected set to the malloc'd _Rejected
 * envelope JSON. force=true skips every seam. */
typedef struct ttlf_approval_seams {
    void *ctx;
    bool (*docker_has_host_access)(void *ctx, const json_t *config);
    const json_t *(*check_all_guards)(void *ctx, const char *command,
                                      const char *env_type, bool has_host_access);
    char *(*preapproval_note)(void *ctx, const json_t *pre_approved);
} ttlf_approval_seams_t;

/* PoP: ttlf_run_approval_guards_flow @ tools/terminal_tool.py:_run_approval_guards */
json_t *ttlf_run_approval_guards_flow(const char *command, const char *env_type,
                                      const json_t *config, bool force,
                                      const ttlf_approval_seams_t *s,
                                      char **rejected);

/* ── 3. Pre-exec guard order: tools/terminal_tool.py:_pre_exec_block ────────
 * Order matters: gateway lifecycle first, then workdir validation, then the
 * missing-local-workdir refusal, then (local only) self_repo OR locked_skill
 * (short-circuit or). Seams return malloc'd JSON envelopes or NULL (pass).
 *   gateway_lifecycle_block / self_repo_block / locked_skill_block
 *       -> tools/terminal_tool_guards.py (kwargs objects borrowed)
 *   validate_workdir -> tools/terminal_tool_guards.py:_validate_workdir
 *   refuse_missing_local -> tools/terminal_tool.py:_refuse_missing_local_workdir
 *       (returns the rejection envelope the live helper raises, or NULL)
 *   log -> logger.warning (pre-formatted message)
 * Returns NULL when the command may run, else the malloc'd rejected JSON. */
typedef struct ttlf_preexec_seams {
    void *ctx;
    char *(*gateway_lifecycle_block)(void *ctx, const json_t *kwargs);
    char *(*validate_workdir)(void *ctx, const char *workdir);
    char *(*refuse_missing_local)(void *ctx, const char *workdir, const json_t *env);
    char *(*self_repo_block)(void *ctx, const json_t *kwargs);
    char *(*locked_skill_block)(void *ctx, const json_t *kwargs);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_preexec_seams_t;

/* PoP: ttlf_pre_exec_block_flow @ tools/terminal_tool.py:_pre_exec_block */
char *ttlf_pre_exec_block_flow(const char *command, const json_t *env,
                               const char *env_type, const char *cwd,
                               const char *workdir, const char *session_key,
                               const ttlf_preexec_seams_t *s);

/* ── 4. Execution planning: tools/terminal_tool.py:_plan_execution ──────────
 * command is any JSON value (Python Any); command_type_name is the Python
 * type(command).__name__ used in the invalid-command messages ("str" for a
 * JSON string). timeout is a JSON number or NULL (None).
 * Seams (call order preserved, including the or-chain short-circuits):
 *   get_env_config            -> _get_env_config (borrowed config dict)
 *   enforce_no_refusal        -> tools.terminal_scope.enforce_no_refusal;
 *                                returns true when it raises, *exc_msg malloc'd
 *   resolve_container_task_id -> _resolve_container_task_id (malloc'd)
 *   resolve_task_overrides    -> resolve_task_overrides (borrowed dict)
 *   select_image              -> _select_image (malloc'd)
 *   get_session_cwd           -> get_session_cwd (malloc'd or NULL); called
 *                                only when overrides["cwd"] is falsy
 *   coerce_ssh_remote_cwd     -> coerce_ssh_remote_cwd (NEW string/null node)
 *   resolve_task_host_cwd     -> _resolve_task_host_cwd (NEW string/null node)
 *   is_container_backend      -> _is_container_backend
 *   is_unusable_container_cwd -> _is_unusable_container_cwd (only when the
 *                                backend is a container)
 *   trailing_amp              -> trailing_amp_background_command (malloc'd/NULL)
 *   guidance                  -> _foreground_background_guidance (malloc'd/NULL)
 *   log                       -> logger.warning/info (pre-formatted)
 * Returns a NEW _ExecPlan field object in dataclass field order (caller
 * frees), or NULL with *rejected (malloc'd envelope) or *exception (malloc'd
 * message) set. */
typedef struct ttlf_plan_seams {
    void *ctx;
    const json_t *(*get_env_config)(void *ctx);
    bool (*enforce_no_refusal)(void *ctx, char **exc_msg);
    char *(*resolve_container_task_id)(void *ctx, const char *task_id);
    const json_t *(*resolve_task_overrides)(void *ctx, const char *task_id);
    char *(*select_image)(void *ctx, const char *env_type,
                          const json_t *overrides, const json_t *config);
    char *(*get_session_cwd)(void *ctx, const char *session_key);
    json_t *(*coerce_ssh_remote_cwd)(void *ctx, const json_t *cwd, const char *env_type);
    json_t *(*resolve_task_host_cwd)(void *ctx, const json_t *config, const char *task_id);
    bool (*is_container_backend)(void *ctx, const char *env_type);
    bool (*is_unusable_container_cwd)(void *ctx, const char *cwd, const json_t *mounted_host);
    char *(*trailing_amp)(void *ctx, const char *command);
    char *(*guidance)(void *ctx, const char *command);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_plan_seams_t;

/* PoP: ttlf_plan_execution_flow @ tools/terminal_tool.py:_plan_execution */
json_t *ttlf_plan_execution_flow(const json_t *command, const char *command_type_name,
                                 const char *task_id, const json_t *timeout,
                                 bool background, bool host_local,
                                 const ttlf_plan_seams_t *s,
                                 char **rejected, char **exception);

/* ── 5. Promotion note: tools/terminal_tool.py:_with_promoted_note ──────────
 * Pure JSON round-trip decision: parse failure / non-dict / truthy "error"
 * returns the input UNCHANGED; otherwise appends promoted_from_foreground
 * (template chosen by notify_on_complete truthiness). Returns malloc'd. */
/* PoP: ttlf_with_promoted_note_flow @ tools/terminal_tool.py:_with_promoted_note */
char *ttlf_with_promoted_note_flow(const char *result_json, int requested_timeout);

/* ── 6. Foreground retry/timeout/yield: tools/terminal_tool.py:_run_foreground ─
 * plan fields are passed flat (env_type, effective_task_id, effective_timeout,
 * plan_cwd, plan_host_cwd). env is a borrowed JSON attribute-double.
 * Seams:
 *   clear_interrupt  -> tools.interrupt.clear_current_thread_interrupt
 *   resolve_cwd      -> _resolve_command_cwd (kwargs borrowed; malloc'd cwd)
 *   yield_handler    -> yield_to_background_handler (kwargs borrowed; returns
 *                       whether a handler was produced)
 *   env_execute      -> env.execute; kwargs borrowed, attempt is retry_count;
 *                       returns the BORROWED scripted result or NULL with
 *                       *exc_type/*exc_msg malloc'd (the raised exception)
 *   sleep_fn         -> time.sleep (recorded, never sleeps)
 *   redact           -> _redact_terminal_error_text (malloc'd)
 *   record_outcome   -> record_terminal_outcome (metered path)
 *   finalize         -> finalize_foreground_result (kwargs borrowed; malloc'd)
 *   log              -> logger.warning/error (pre-formatted)
 * Returns malloc'd result JSON (envelope, yielded envelope, or finalize ret). */
typedef struct ttlf_foreground_seams {
    void *ctx;
    void (*clear_interrupt)(void *ctx);
    char *(*resolve_cwd)(void *ctx, const json_t *kwargs);
    bool (*yield_handler)(void *ctx, const json_t *kwargs);
    const json_t *(*env_execute)(void *ctx, const json_t *kwargs, int attempt,
                                 char **exc_type, char **exc_msg);
    void (*sleep_fn)(void *ctx, double seconds);
    char *(*redact)(void *ctx, const char *text);
    void (*record_outcome)(void *ctx, const char *command, const char *env_type,
                           const json_t *result);
    char *(*finalize)(void *ctx, const json_t *kwargs);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_foreground_seams_t;

/* PoP: ttlf_run_foreground_flow @ tools/terminal_tool.py:_run_foreground */
char *ttlf_run_foreground_flow(const char *command, const json_t *env,
                               const char *env_type, const char *effective_task_id,
                               int effective_timeout, const char *plan_cwd,
                               const char *plan_host_cwd,
                               const char *task_id, const char *session_id,
                               const char *session_key, const char *cwd_key,
                               const char *workdir, const char *approval_note,
                               bool clear_interrupt, bool metered,
                               const ttlf_foreground_seams_t *s);

/* ── 7. Deadline floor: tools/terminal_tool.py:_terminal_deadline_floor ─────
 * args is a borrowed dict. parse_env_var seam stands in for _parse_env_var
 * ("TERMINAL_TIMEOUT", "180"): returns a NEW number node or NULL with
 * *raised=true (ValueError). Returns true with *out set, false for None. */
typedef struct ttlf_deadline_seams {
    void *ctx;
    json_t *(*parse_env_var)(void *ctx, const char *name, const char *def, bool *raised);
} ttlf_deadline_seams_t;

/* PoP: ttlf_terminal_deadline_floor_flow @ tools/terminal_tool.py:_terminal_deadline_floor */
bool ttlf_terminal_deadline_floor_flow(const json_t *args,
                                       const ttlf_deadline_seams_t *s, double *out);

/* ── 8. Degraded backend: tools/terminal_tool.py:_degraded_result ───────────
 * e is a borrowed {reason, retry_hint} object. Seams:
 *   tenv             -> _tenv (malloc'd raw value)
 *   fatal_error_json -> _fatal_error_json (malloc'd envelope)
 *   evict            -> _evict_environment_for_task; true when it raises
 *                       (swallowed by _quiet, debug-logged)
 *   log              -> logger.warning/debug
 * Returns malloc'd result JSON. */
typedef struct ttlf_degraded_seams {
    void *ctx;
    char *(*tenv)(void *ctx, const char *name, const char *def);
    char *(*fatal_error_json)(void *ctx, const json_t *e);
    bool (*evict)(void *ctx, const char *task_id);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_degraded_seams_t;

/* PoP: ttlf_degraded_result_flow @ tools/terminal_tool.py:_degraded_result */
char *ttlf_degraded_result_flow(const json_t *e, const char *task_id,
                                const ttlf_degraded_seams_t *s);

/* ── 9. Dispatch normalization: tools/terminal_tool.py:_handle_terminal ─────
 * args borrowed; heartbeat_py_type marks a JSON number that Python received
 * as a float ("float") vs int (NULL/"int") — isinstance(heartbeat, int).
 * Seams: validate_prepared (agent.terminal_approval_batch), terminal_tool
 * (kwargs borrowed, malloc'd result). Returns malloc'd result JSON. */
typedef struct ttlf_handle_seams {
    void *ctx;
    void (*validate_prepared)(void *ctx, const json_t *args);
    char *(*terminal_tool)(void *ctx, const json_t *kwargs);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_handle_seams_t;

/* PoP: ttlf_handle_terminal_flow @ tools/terminal_tool.py:_handle_terminal */
char *ttlf_handle_terminal_flow(const json_t *args, const char *heartbeat_py_type,
                                const char *task_id, const char *session_id,
                                const ttlf_handle_seams_t *s);

/* ── 10. Background spawn: tools/terminal_tool_background.py:spawn_background_process ─
 * params is a borrowed object carrying the keyword arguments: command, env
 * (attribute-double or null), env_type, effective_task_id, task_id,
 * session_key, workdir, cwd, effective_pty, cwd_key, notify_on_complete,
 * watch_patterns (array|null), approval_note, completion_output_chars,
 * pty_disabled_reason, heartbeat_seconds, persist_on_release, mounted_host
 * (string|null; null means "use env.host_cwd"), auto_backgrounded.
 * Seams:
 *   resolve_cwd      -> _resolve_command_cwd (kwargs borrowed; malloc'd)
 *   spawn            -> _spawn's process_registry.spawn_local/spawn_via_env;
 *                       branch "local"/"env", kwargs borrowed; returns a NEW
 *                       session attribute-double or NULL with *exc_msg malloc'd
 *   msys_path        -> tools.environments.local._msys_to_windows_path
 *   apply_async_support -> _apply_async_support: mutates session/result_data
 *                       per the script, writes *out_notify / *out_watch
 *                       (NEW array or NULL)
 *   register_watcher -> _register_completion_watcher (sets watcher_interval=5)
 *   is_delegated_child -> agent.delegation_context.is_delegated_child_context
 *   arm_heartbeat    -> process_registry.arm_heartbeat
 *   redact           -> _redact_terminal_error_text (malloc'd)
 *   log              -> logger.warning
 * Returns malloc'd result JSON. *session_out receives the session object
 * (ownership transfers; NULL when spawn itself raised) — mutations persist
 * even when a later step raises, exactly like the live proc_session. */
typedef struct ttlf_spawn_seams {
    void *ctx;
    char *(*resolve_cwd)(void *ctx, const json_t *kwargs);
    json_t *(*spawn)(void *ctx, const char *branch, const json_t *kwargs, char **exc_msg);
    char *(*msys_path)(void *ctx, const char *path);
    void (*apply_async_support)(void *ctx, json_t *session, json_t *result_data,
                                bool notify, const json_t *watch,
                                bool *out_notify, json_t **out_watch);
    void (*register_watcher)(void *ctx, json_t *session, const char *session_key);
    bool (*is_delegated_child)(void *ctx);
    int (*arm_heartbeat)(void *ctx, json_t *session, int seconds);
    char *(*redact)(void *ctx, const char *text);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_spawn_seams_t;

/* PoP: ttlf_spawn_background_flow @ tools/terminal_tool_background.py:spawn_background_process */
char *ttlf_spawn_background_flow(const json_t *params, const ttlf_spawn_seams_t *s,
                                 json_t **session_out);

/* PoP: ttlf_looks_like_homebrew_ci_poller @ tools/terminal_tool_background.py:_looks_like_homebrew_ci_poller */
bool ttlf_looks_like_homebrew_ci_poller(const char *command);

/* ── 11. Foreground finalize: tools/terminal_tool_result.py:finalize_foreground_result ─
 * result/env borrowed (env may be NULL). Seams model the whole live helper
 * including its internal _quiet wrapper: raised=true means the helper
 * swallowed an exception and returned its fallback (the flow debug-logs the
 * _quiet label). sudo_annotations returns a NEW {output, auth_failed,
 * cache_cleared} object. redact_spill returns a NEW array of [key, value]
 * pairs. verification returns a NEW object or NULL. realpath raised=true
 * models os.path.realpath raising inside the flow's own _quiet block.
 * Returns malloc'd result JSON with optional fields in live insertion order. */
typedef struct ttlf_finalize_seams {
    void *ctx;
    void (*record_session_cwd)(void *ctx, const char *session_key, const char *cwd);
    json_t *(*sudo_annotations)(void *ctx, const char *command, const char *output,
                                const char *env_type);
    char *(*transform_hook)(void *ctx, const json_t *kwargs, bool *raised);
    char *(*truncate)(void *ctx, const char *output);
    char *(*strip_ansi)(void *ctx, const char *output);
    char *(*redact_output)(void *ctx, const char *output, const char *command);
    char *(*interpret_exit_code)(void *ctx, const char *command, int returncode);
    char *(*failure_hint)(void *ctx, const json_t *kwargs, bool *raised);
    char *(*realpath)(void *ctx, const char *path, bool *raised);
    json_t *(*redact_spill)(void *ctx, const json_t *path, const json_t *total_chars,
                            const char *command);
    json_t *(*verification)(void *ctx, const json_t *kwargs, bool *raised);
    void (*log)(void *ctx, const char *level, const char *msg);
} ttlf_finalize_seams_t;

/* PoP: ttlf_finalize_foreground_result_flow @ tools/terminal_tool_result.py:finalize_foreground_result */
char *ttlf_finalize_foreground_result_flow(const char *command, const json_t *result,
                                           const json_t *env, const char *env_type,
                                           const char *effective_task_id,
                                           const char *task_id, const char *session_id,
                                           const char *session_key, const char *workdir,
                                           const char *command_cwd, const char *approval_note,
                                           const ttlf_finalize_seams_t *s);

/* PoP: ttlf_cwd_fallback_note @ tools/terminal_tool_result.py:_cwd_fallback_note */
char *ttlf_cwd_fallback_note(const json_t *fallback);

/* ── 12. Exit-code interpretation: tools/terminal_tool_result.py:_interpret_exit_code ─
 * signal_note seam stands in for _interpret_signal_exit (malloc'd or NULL).
 * The pipeline/chain split, base-command selection and _EXIT_CODE_SEMANTICS
 * table are ported verbatim. Returns malloc'd note or NULL. */
typedef struct ttlf_iec_seams {
    void *ctx;
    char *(*signal_note)(void *ctx, int exit_code);
} ttlf_iec_seams_t;

/* PoP: ttlf_interpret_exit_code_flow @ tools/terminal_tool_result.py:_interpret_exit_code */
char *ttlf_interpret_exit_code_flow(const char *command, int exit_code,
                                    const ttlf_iec_seams_t *s);

/* ── 13. Host-path normalization: tools/terminal_tool_config.py:_host_path_key ─
 * Slash style + drive-letter case folding; trailing slashes dropped except
 * drive roots. Returns malloc'd key ("" for NULL/empty input). */
/* PoP: ttlf_host_path_key @ tools/terminal_tool_config.py:_host_path_key */
char *ttlf_host_path_key(const char *path);

/* PoP: ttlf_translate_mounted_host_path @ tools/terminal_tool_config.py:translate_mounted_host_path */
char *ttlf_translate_mounted_host_path(const char *path, const char *host_root,
                                       const char *container_root);

/* ── 14. SSH remote cwd coercion: tools/terminal_tool_config.py:coerce_ssh_remote_cwd ─
 * cwd is any JSON value (non-string passes through unchanged). The POSIX
 * lexical normpath/relpath math is ported; the host facts enter via seams:
 *   is_container_launch_dir -> _is_container_launch_dir
 *   get_subprocess_home     -> hermes_constants.get_subprocess_home (malloc'd/NULL)
 *   get_real_home           -> hermes_constants.get_real_home (malloc'd/NULL;
 *                              called only when subprocess home is truthy)
 * Returns a NEW JSON node (string or null). */
typedef struct ttlf_ssh_seams {
    void *ctx;
    bool (*is_container_launch_dir)(void *ctx, const char *path);
    char *(*get_subprocess_home)(void *ctx);
    char *(*get_real_home)(void *ctx);
} ttlf_ssh_seams_t;

/* PoP: ttlf_coerce_ssh_remote_cwd_flow @ tools/terminal_tool_config.py:coerce_ssh_remote_cwd */
json_t *ttlf_coerce_ssh_remote_cwd_flow(const json_t *cwd, const char *env_type,
                                        const ttlf_ssh_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* TERMINAL_TOOL_LIFECYCLE_FLOW_H */
