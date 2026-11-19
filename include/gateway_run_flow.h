/*
 * gateway_run_flow.h — message-handling FLOW cores of gateway/run.py.
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception->fallback paths, mutation order) of the live module. All live I/O
 * (store reads, host-lock rendezvous, config/profile reads, agent-cache access,
 * executor lifecycle, i18n, logging, process exit) is injected through seams:
 * callback structs and injected-result parameters declared per flow below.
 * The decision core performs NO I/O itself.
 *
 * Sequences are observable through a grun_trace_t: seam implementations record
 * one JSON event per call (arguments + scripted result) in call order; the
 * oracle compares that ordered event list against recording monkeypatch stubs
 * driving the LIVE Python with the same scripted results.
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 */

#ifndef GATEWAY_RUN_FLOW_H
#define GATEWAY_RUN_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. grun_trace_add takes ownership of
 * ev (frees it when t is NULL). The trace never leaves the caller. */
typedef struct grun_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} grun_trace_t;

void grun_trace_add(grun_trace_t *t, json_t *ev);
void grun_trace_free(grun_trace_t *t);

/* ── 1. Delivery routing: empty/failed agent response normalization ─────────
 * Seams stand in for:
 *   is_context_overflow      -> gateway/run_turn.py:is_context_overflow_failure_result
 *   looks_like_provider_error-> gateway/run.py:_looks_like_gateway_provider_error
 *   hidden_reasoning_incomplete -> gateway/run.py:_is_gateway_hidden_reasoning_incomplete_turn
 *   t                        -> agent/i18n.py:t (scripted templates; kwargs object
 *                               is borrowed, seam must copy what it records)
 *   log_warning              -> gateway logger.warning (pre-formatted message)
 * Returns malloc'd reply text (caller frees). agent_result/response borrowed. */
typedef struct grun_normalize_seams {
    void *ctx;
    bool (*is_context_overflow)(void *ctx, const json_t *agent_result, int history_len);
    bool (*looks_like_provider_error)(void *ctx, const char *text);
    bool (*hidden_reasoning_incomplete)(void *ctx, const json_t *agent_result);
    char *(*t)(void *ctx, const char *key, const json_t *kwargs);
    void (*log_warning)(void *ctx, const char *msg);
} grun_normalize_seams_t;

/* PoP: grun_flow_normalize_empty_agent_response @ gateway/run.py:_normalize_empty_agent_response */
char *grun_flow_normalize_empty_agent_response(const json_t *agent_result,
                                               const char *response,
                                               int history_len,
                                               const grun_normalize_seams_t *s);

/* ── 2. Replay intake: entry construction for non-tool-calling rows ─────────
 * Seams stand in for:
 *   replay_metadata_fields -> agent/message_metadata.py:replay_metadata_fields
 *                             (borrowed scripted result object)
 *   copy_identity_fields   -> agent/message_metadata.py:copy_identity_fields
 *                             (seam applies the scripted identity update to dst;
 *                              src is the flow-built {MESSAGE_UID, ABSORBED_MESSAGE_UIDS}
 *                              subset, borrowed)
 * Returns a NEW json object (caller frees). role/content/msg borrowed. */
typedef struct grun_replay_seams {
    void *ctx;
    json_t *(*replay_metadata_fields)(void *ctx, const json_t *msg);
    void (*copy_identity_fields)(void *ctx, const json_t *src, json_t *dst);
} grun_replay_seams_t;

/* PoP: grun_flow_build_replay_entry @ gateway/run.py:_build_replay_entry */
json_t *grun_flow_build_replay_entry(const char *role, const json_t *content,
                                     const json_t *msg, bool preserve_timestamp,
                                     const grun_replay_seams_t *s);

/* Pure predicate; role/content are any JSON values (Python Any). No seams. */
/* PoP: grun_flow_has_replayable_sidecar @ gateway/run.py:_has_replayable_sidecar */
bool grun_flow_has_replayable_sidecar(const json_t *role, const json_t *content,
                                      const json_t *msg);

/* ── 3. Intake normalization: command delivery origin ───────────────────────
 * source is a JSON model of SessionSource: platform (object with "value",
 * string, null or absent), chat_id, chat_name, thread_id.
 * platform_raises is the injected exception result standing in for a raising
 * source.platform property (the live try/except -> None path).
 * Returns a NEW object {platform, chat_id, chat_name, thread_id} or NULL. */
/* PoP: grun_flow_command_origin_for_source @ gateway/run.py:_command_origin_for_source */
json_t *grun_flow_command_origin_for_source(const json_t *source, bool platform_raises);

/* ── 4. Agent-cache lifecycle: hygiene cached-agent read under lock ─────────
 * gateway is a JSON model: "_agent_cache" (null/absent = no cache),
 * "_agent_cache_lock" (truthy = real lock, falsy = suppress()).
 * Seams stand in for:
 *   lock_acquire / lock_release -> entering/exiting gateway._agent_cache_lock
 *   cache_get                   -> gateway._agent_cache.get(session_key);
 *                                  returns the borrowed entry, sets *is_tuple
 *                                  (Python tuple entries unpack to entry[0])
 *                                  and *raised on exception.
 * Returns a NEW json value (caller frees) or NULL. */
typedef struct grun_cache_seams {
    void *ctx;
    void (*lock_acquire)(void *ctx);
    void (*lock_release)(void *ctx);
    json_t *(*cache_get)(void *ctx, const char *session_key, bool *is_tuple, bool *raised);
} grun_cache_seams_t;

/* PoP: grun_flow_cached_agent_for_hygiene @ gateway/run.py:_cached_agent_for_hygiene */
json_t *grun_flow_cached_agent_for_hygiene(const json_t *gateway, const char *session_key,
                                           const grun_cache_seams_t *s);

/* ── 5. Executor lifecycle: get-or-create under the executor lock ───────────
 * self_state is a MUTABLE JSON model of the runner: "_executor_lock" (truthy =
 * already created; the flow sets it true when it creates the lock),
 * "_executor_closing", and the attr slot holding an executor descriptor
 * ({"_shutdown": bool, ...}) or null.
 * Seam make_pool stands in for the injected pool factory; returns a borrowed
 * descriptor which the flow COPIES into self_state[attr].
 * Returns the BORROWED executor descriptor now in self_state[attr], or NULL
 * with *raised_runtime_error=true (live RuntimeError on the closing flag).
 * Single-threaded harness: lock acquire/release is modeled by the state flag. */
typedef struct grun_pool_seams {
    void *ctx;
    json_t *(*make_pool)(void *ctx);
} grun_pool_seams_t;

/* PoP: grun_flow_get_or_create_pool @ gateway/run.py:GatewayRunner._get_or_create_pool */
json_t *grun_flow_get_or_create_pool(json_t *self_state, const char *attr,
                                     const grun_pool_seams_t *s,
                                     bool *raised_runtime_error);

/* ── 6. Shutdown sequence: stop a pool without waiting ──────────────────────
 * executor is a JSON descriptor or NULL (None -> []).
 * Seam shutdown stands in for executor.shutdown(wait=..., cancel_futures=...);
 * returns true when the call raised TypeError (live fallback retries without
 * cancel_futures). Returns a NEW array of the executor's "_threads" (caller
 * frees); absent/falsy _threads -> empty array. */
typedef struct grun_stop_pool_seams {
    void *ctx;
    bool (*shutdown)(void *ctx, bool wait, bool cancel_futures);
} grun_stop_pool_seams_t;

/* PoP: grun_flow_stop_pool @ gateway/run.py:GatewayRunner._stop_pool */
json_t *grun_flow_stop_pool(const json_t *executor, const grun_stop_pool_seams_t *s);

/* ── 7. Session-key routing: explicit key or derive from source ─────────────
 * Seam session_key_for_source stands in for GatewayRunner._session_key_for_source;
 * returns malloc'd key (flow frees) or NULL, *raised on exception.
 * source NULL models Python None. Returns malloc'd key or NULL (caller frees). */
typedef struct grun_session_key_seams {
    void *ctx;
    char *(*session_key_for_source)(void *ctx, const json_t *source, bool *raised);
} grun_session_key_seams_t;

/* PoP: grun_flow_resolve_session_key_or_none @ gateway/run.py:GatewayRunner._resolve_session_key_or_none */
char *grun_flow_resolve_session_key_or_none(const json_t *source, const char *session_key,
                                            const grun_session_key_seams_t *s);

/* ── 8. Cron tick gating: stand down when another process owns the home ─────
 * Seam resolve_liveness stands in for gateway.status.resolve_gateway_liveness
 * (pid_probe included); returns borrowed {"running": bool, "pid": int|null}
 * or NULL with *raised. self_pid stands in for os.getpid(). */
typedef struct grun_cron_gate_seams {
    void *ctx;
    json_t *(*resolve_liveness)(void *ctx, const char *home, bool *raised);
    long long self_pid;
    const char *probe_error;   /* injected str(exc) for the raised probe path */
    void (*log_debug)(void *ctx, const char *msg);
} grun_cron_gate_seams_t;

/* PoP: grun_flow_cron_profile_gate @ gateway/run.py:_cron_profile_gate */
bool grun_flow_cron_profile_gate(const char *name, const char *home,
                                 const grun_cron_gate_seams_t *s);

/* ── 9. Cron tick scope: served homes plus the process-active profile ───────
 * Seams stand in for:
 *   multiplex_homes -> gateway/run.py:_multiplex_profile_homes (borrowed array
 *                      of [name, home] pairs; Path homes modeled as strings)
 *   active_profile  -> hermes_cli.profiles.get_active_profile_name (malloc'd or NULL)
 *   profile_dir     -> hermes_cli.profiles.get_profile_dir (malloc'd; *raised
 *                      models the except -> homes fallback)
 * Returns a NEW array of [name, home] pairs (caller frees). */
typedef struct grun_cron_tick_seams {
    void *ctx;
    json_t *(*multiplex_homes)(void *ctx, const json_t *config);
    char *(*active_profile)(void *ctx);
    char *(*profile_dir)(void *ctx, const char *name, bool *raised);
} grun_cron_tick_seams_t;

/* PoP: grun_flow_cron_tick_profile_homes @ gateway/run.py:_cron_tick_profile_homes */
json_t *grun_flow_cron_tick_profile_homes(const json_t *config, const grun_cron_tick_seams_t *s);

/* ── 10. Handoff watcher scopes: root poll first, then secondary homes ──────
 * runner is a JSON model {"config": {"multiplex_profiles": bool}|null}.
 * Seam multiplex_homes stands in for _multiplex_profile_homes; *raised models
 * the resolver exception that degrades to the root poll.
 * Returns a NEW array of [name|null, home|null] pairs (caller frees). */
typedef struct grun_handoff_seams {
    void *ctx;
    json_t *(*multiplex_homes)(void *ctx, const json_t *config, bool *raised);
    void (*log_debug)(void *ctx, const char *msg);
} grun_handoff_seams_t;

/* PoP: grun_flow_handoff_watch_scopes @ gateway/run.py:_handoff_watch_scopes */
json_t *grun_flow_handoff_watch_scopes(const json_t *runner, const grun_handoff_seams_t *s);

/* ── 11. Host-gateway claim: lock race arbitration sequence ─────────────────
 * Every rendezvous/attach/profile call is a seam; each records its event.
 *   hermes_home            -> hermes_constants.get_hermes_home (malloc'd)
 *   attach_decision        -> gateway.host_attach.standalone_attach_decision;
 *                             borrowed {"outcome","message"} or NULL; called
 *                             pre-lock with owner=NULL and again on the
 *                             standalone recheck with the live owner
 *   claim_host_lock        -> gateway.host_rendezvous.claim_host_lock; writes
 *                             malloc'd *outcome ("acquired"|"held-by-other"|
 *                             "could-not-open") and *error (or NULL); returns
 *                             true when the call raised
 *   publish_record         -> hr.publish_record (profiles borrowed array)
 *   cleanup_on_exit        -> hr.cleanup_on_exit
 *   read_record            -> hr.read_record(include_stale=True); borrowed
 *                             owner object or NULL; *raised on exception
 *   describe               -> hr.describe (malloc'd)
 *   launched_by_other_tenant -> gateway.host_attach.launched_by_other_tenant
 *   profile_is_standalone  -> hermes_cli.profiles.profile_is_standalone
 *   host_gateway           -> gateway.host_attach.host_gateway(wait_for_channel)
 *                             borrowed owner or NULL
 *   owner_is_standalone    -> gateway/run.py:_owner_is_standalone
 *   migrate_command        -> gateway/run.py:_migrate_command (malloc'd)
 *   refuse_second          -> gateway/run.py:_refuse_second_host_gateway
 *                             (records; the flow then exits 75)
 *   print_line / log       -> builtins.print / gateway logger
 * Returns the process exit verdict: 0 = returned normally, 75 = SystemExit
 * (GATEWAY_SERVICE_RESTART_EXIT_CODE). */
typedef struct grun_claim_seams {
    void *ctx;
    char *(*hermes_home)(void *ctx);
    json_t *(*attach_decision)(void *ctx, const char *home, const json_t *owner);
    bool (*claim_host_lock)(void *ctx, const char *role, char **outcome, char **error);
    void (*publish_record)(void *ctx, const char *role, const json_t *profiles, const char *home);
    void (*cleanup_on_exit)(void *ctx, const char *role);
    json_t *(*read_record)(void *ctx, const char *role, bool include_stale, bool *raised);
    char *(*describe)(void *ctx, const json_t *owner);
    bool (*launched_by_other_tenant)(void *ctx, const char *owner_home, const char *home);
    bool (*profile_is_standalone)(void *ctx, const char *home);
    json_t *(*host_gateway)(void *ctx, double wait_for_channel);
    bool (*owner_is_standalone)(void *ctx);
    char *(*migrate_command)(void *ctx);
    void (*refuse_second)(void *ctx, const json_t *owner);
    void (*print_line)(void *ctx, const char *text);
    void (*log)(void *ctx, const char *level, const char *msg);
} grun_claim_seams_t;

/* PoP: grun_flow_claim_host_gateway_role @ gateway/run.py:_claim_host_gateway_role */
int grun_flow_claim_host_gateway_role(bool force, const grun_claim_seams_t *s);

/* ── 12. Standalone-owner probe on the lock-losing path ─────────────────────
 * Seams stand in for gateway.host_attach.host_gateway / profile_name_for_home /
 * request_serve_profile, hermes_constants.get_hermes_home and os.getpid()
 * (self_pid). Any seam may set *raised (live except -> debug log + False).
 * request_serve_profile returns borrowed {"standalone": bool} or NULL. */
typedef struct grun_owner_seams {
    void *ctx;
    json_t *(*host_gateway)(void *ctx, bool *raised);
    long long self_pid;
    char *(*hermes_home)(void *ctx, bool *raised);
    char *(*profile_name_for_home)(void *ctx, const char *home, bool *raised);
    json_t *(*request_serve_profile)(void *ctx, const char *name, const json_t *owner, bool *raised);
    void (*log_debug)(void *ctx, const char *msg);
} grun_owner_seams_t;

/* PoP: grun_flow_owner_is_standalone @ gateway/run.py:_owner_is_standalone */
bool grun_flow_owner_is_standalone(const grun_owner_seams_t *s);

/* ── 13. Second-gateway refusal: named message + exit 75 ────────────────────
 * Seams: describe (hr.describe, malloc'd), migrate_command (_migrate_command,
 * malloc'd), log_error, print_line. owner borrowed or NULL.
 * Always returns 75 (SystemExit verdict). */
typedef struct grun_refuse_seams {
    void *ctx;
    char *(*describe)(void *ctx, const json_t *owner);
    char *(*migrate_command)(void *ctx);
    void (*log_error)(void *ctx, const char *msg);
    void (*print_line)(void *ctx, const char *text);
} grun_refuse_seams_t;

/* PoP: grun_flow_refuse_second_host_gateway @ gateway/run.py:_refuse_second_host_gateway */
int grun_flow_refuse_second_host_gateway(const json_t *owner, const grun_refuse_seams_t *s);

/* ── 14. Host record refresh with the settled served set ────────────────────
 * runner JSON model: "config" key ABSENT models AttributeError (except path);
 * config null models getattr(None, "multiplex_profiles", False) == False.
 * Seams: owns_host_lock (hr.owns_host_lock), hermes_home, served_profile_names
 * (runner.served_profile_names, borrowed array), profile_name_for_home
 * (host_attach), publish_record, log_debug. Any *raised -> except path. */
typedef struct grun_refresh_seams {
    void *ctx;
    bool (*owns_host_lock)(void *ctx, const char *role, bool *raised);
    char *(*hermes_home)(void *ctx, bool *raised);
    json_t *(*served_profile_names)(void *ctx, bool *raised);
    char *(*profile_name_for_home)(void *ctx, const char *home, bool *raised);
    void (*publish_record)(void *ctx, const char *role, const json_t *profiles, const char *home);
    void (*log_debug)(void *ctx, const char *msg);
} grun_refresh_seams_t;

/* PoP: grun_flow_refresh_host_gateway_record @ gateway/run.py:_refresh_host_gateway_record */
void grun_flow_refresh_host_gateway_record(const json_t *runner, const grun_refresh_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_RUN_FLOW_H */
