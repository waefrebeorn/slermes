/*
 * async_delegation.h — public API for the background (async) delegation registry.
 * Faithful C port of tools/async_delegation.py.
 *
 * Backs delegate_task(background=true): a parent agent dispatches a child that
 * runs on a daemon worker thread and returns a handle immediately. When the
 * child finishes, a completion event is delivered via an injected sink so the
 * caller can route it back into the conversation (mirrors Python's
 * process_registry.completion_queue).
 *
 * Design notes (faithful to Python):
 *  - The runner is INJECTED (zero-arg callable returning a json result). All
 *    child build/run logic lives in the caller, keeping this module a pure
 *    lifecycle owner.
 *  - The completion sink is INJECTED (type async_delegation_sink_t). The C
 *    caller wires it to its process_registry completion queue. No link-time
 *    dependency on process_registry here (self-contained).
 *  - Capacity is enforced under the registry lock so two concurrent dispatches
 *    cannot both exceed the cap.
 *  - interrupt_all() runs every live record's interrupt_fn then marks them
 *    cancelled.
 *
 * Minimal includes, no hermes.h. Opaque record type; callers only see json.
 */

#ifndef HERMES_ASYNC_DELEGATION_H
#define HERMES_ASYNC_DELEGATION_H

#include <stddef.h>
#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Result a runner returns. Caller owns the returned json (freed by the
 * registry after finalize). */
typedef json_node_t *(*async_delegation_runner_t)(void);

/* Optional interrupt callback (signals the child to stop). */
typedef void (*async_delegation_interrupt_t)(void);

/* Progress sampler for the stale-delegation monitor. Returns a non-negative
 * count of "interesting activity tokens" and sets *in_tool. The contract is
 * intentionally opaque to the registry (mirrors Python's opaque `token`):
 * a changed return value refreshes the record's progress timestamp. */
typedef int (*async_delegation_progress_t)(void *progress_ctx, int *in_tool);

/* Completion sink: receives the finished event record. The registry owns
 * `event` and frees it after the sink returns. */
typedef void (*async_delegation_sink_t)(json_node_t *event);

#define ASYNC_DELEGATION_DEFAULT_MAX_CHILDREN 3
#define ASYNC_DELEGATION_MAX_RETAINED_COMPLETED 50

/* Initialize the async delegation subsystem (idempotent). */
void async_delegation_init(void);

/* Tear down: cancel all running, free all records. */
void async_delegation_shutdown(void);

/* Number of delegations currently running. */
int async_delegation_active_count(void);

/* Spawn `runner` on a daemon worker and return a handle immediately.
 * Returns a json object: {"status":"dispatched","delegation_id":...} or
 * {"status":"rejected","error":...} when at capacity.
 * `goal`,`context`,`toolsets` (NULL-terminated string array or NULL),
 * `role`,`model`,`session_key` are captured verbatim for the completion block.
 * `origin_ui_session_id`/`origin_session_id`/`parent_session_id` route the
 * completion event back to the originating session (mirrors Python's
 * session-key/origin_ui_session_id/origin_session_id/parent_session_id).
 * `runner` is the child; `interrupt_fn` signals it to stop.
 * `progress_fn(progress_ctx, &in_tool)` is sampled by the stale-delegation
 * monitor; it returns an activity-token integer (changes refresh the
 * timestamp) and sets *in_tool so the monitor can apply the right idle/grace
 * threshold. `sink` receives the completion event when the child finishes. */
json_node_t *async_delegation_dispatch(
    const char *goal, const char *context, const char *const *toolsets,
    const char *role, const char *model, const char *session_key,
    const char *origin_ui_session_id, const char *origin_session_id,
    const char *parent_session_id,
    async_delegation_runner_t runner, async_delegation_interrupt_t interrupt_fn,
    async_delegation_progress_t progress_fn, void *progress_ctx,
    async_delegation_sink_t sink, int max_async_children);

/* Fan-out batch: `runner` runs the WHOLE batch as ONE background unit and
 * returns {"results":[...],"total_duration_seconds":N}. Occupies ONE slot. */
json_node_t *async_delegation_dispatch_batch(
    const char *const *goals, int n_goals, const char *context,
    const char *const *toolsets, const char *role, const char *model,
    const char *session_key, const char *origin_ui_session_id,
    const char *origin_session_id, const char *parent_session_id,
    async_delegation_runner_t runner, async_delegation_interrupt_t interrupt_fn,
    async_delegation_progress_t progress_fn, void *progress_ctx,
    async_delegation_sink_t sink, int max_async_children);

/* List all delegations (running + recently completed) as a json array of
 * records. Caller owns the returned array. */
json_node_t *async_delegation_list(void);

/* Number of active (running/stalling/finalizing) delegations owned by a
 * single UI session, or total if session_key is NULL/empty. */
int async_delegation_active_for_session(const char *session_key,
                                        const char *origin_ui_session_id,
                                        const char *parent_session_id);

/* Number of active TASKS (child subagents): expands a batch to its child
 * count. For the in-memory registry each batch occupies one slot, so the
 * faithful count is the number of active delegations (matches Python's
 * len(goals) fallback of 1 per batch when the goal list is present). */
int async_delegation_active_task_count(void);

/* Whether a session still owns any live async delegation. */
bool async_delegation_has_live_for_session(const char *session_key,
                                           const char *origin_ui_session_id,
                                           const char *parent_session_id);

/* Interrupt all active delegations owned by one UI session. Returns count. */
int async_delegation_interrupt_for_session(const char *session_key,
                                           const char *origin_ui_session_id,
                                           const char *parent_session_id);

/* Signal all running delegations to stop; mark them cancelled. Returns the
 * number of running delegations that were interrupted. */
int async_delegation_interrupt_all(const char *reason);

/* Test helper: drop all records (running + completed) without running threads. */
void async_delegation_reset_for_tests(void);

/* Start (once) the stale-delegation monitor thread for the given record.
 * Idempotent; the monitor exits on its own when no monitorable records
 * remain. Call after dispatching a delegation that carries progress_fn. */
/* PoP: _ensure_stale_monitor @ tools/async_delegation.py:_ensure_stale_monitor */
void async_delegation_ensure_stale_monitor(void);

/* PoP: _children_activity_from_token @ tools/async_delegation.py:_children_activity_from_token */
/* Best-effort parse of a progress token into per-child activity fields.
 * Returns api_calls count; fills optional out params. Returns 0 on failure. */
int async_delegation_children_activity_from_token(const char *token_json,
                                                  int *out_api_calls,
                                                  char *out_tool_buf,
                                                  size_t out_tool_cap,
                                                  double *out_activity_ts);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ASYNC_DELEGATION_H */
