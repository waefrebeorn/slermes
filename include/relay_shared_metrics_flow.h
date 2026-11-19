/*
 * relay_shared_metrics_flow.h — shared-metrics normalization/aggregation
 * FLOW cores of hermes_cli/observability/relay_shared_metrics.py (LIVE
 * authority).
 *
 * Ports the DECISION sequences (guard chains, branch conditions, emission
 * ORDER, aggregation state machines, error paths) of the _Runtime event
 * projection. All live I/O (Relay scope emission, the monotonic clock, the
 * contract/fields/model-state callees owned by sibling shared_metrics_*
 * modules) is injected through seams; the decision cores perform no I/O.
 *
 * Sequences are observable through an rsm_flow_trace_t: seam implementations
 * record one JSON event per call (arguments + scripted result) in call order;
 * the oracle compares that ordered event list against recording monkeypatch
 * stubs driving the LIVE Python with the same scripted results.
 *
 * State models (JSON):
 *   session   {session_id, closing, start_fields, turns, failed_turns,
 *              last_outcome, first_turn_ns, last_turn_ns, api_calls,
 *              tool_call_total, replies, tasks: {task_id: task},
 *              model_calls: {"<task>\t<request>": model_call}}
 *   task      {task_id, ref, started_ns, start_fields, model_call_ids: [],
 *              tool_call_ids: [], turn_ids: [], retired_turn_ids: [],
 *              unidentified_tool_calls, cost: {user_turn}}
 *   model_call {task_id, fields, error_class, ttft_bucket}
 * `ref` markers model Python object identity for the `is` checks (the live
 * task stored in session.tasks vs the task argument).
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 */

#ifndef RELAY_SHARED_METRICS_FLOW_H
#define RELAY_SHARED_METRICS_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ── */
typedef struct rsm_flow_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} rsm_flow_trace_t;

void rsm_flow_trace_add(rsm_flow_trace_t *t, json_t *ev);
void rsm_flow_trace_free(rsm_flow_trace_t *t);

/* ── 1. Terminal flags for a task's return or raise ─────────────────────────
 * error: NULL (no exception) or {"name": "<exception class name>"} describing
 * a BUILTIN exception (KeyboardInterrupt / InterruptedError / TimeoutError /
 * ValueError / ...) or asyncio.CancelledError (matched by type NAME live).
 * result: the task's return value (any JSON; non-dict reads as {}).
 * `is True` identity gates: JSON true only, never 1/"true".
 * Returns a NEW flags object; caller frees. */
/* PoP: rsm_flow_terminal_flags @ hermes_cli/observability/relay_shared_metrics.py:_terminal_flags */
json_t *rsm_flow_terminal_flags(const json_t *result, const json_t *error);

/* ── 2. (session_id, event[key]) pair when both are non-empty ───────────────
 * _text semantics: str(event.get(key) or "") — falsy collapses to "" BEFORE
 * str(). Returns a NEW 2-element array or NULL; caller frees. */
/* PoP: rsm_flow_session_pair @ hermes_cli/observability/relay_shared_metrics.py:_session_pair */
json_t *rsm_flow_session_pair(const json_t *event, const char *key);

/* ── 3. Partial hook-context identity compatibility ─────────────────────────
 * candidate/observed: 3-element string arrays (api_request_id, turn_id,
 * tool_call_id). observed[2] falsy or differing -> false; the first two slots
 * match when either side is empty or both equal. Pure. */
/* PoP: rsm_flow_identities_compatible @ hermes_cli/observability/relay_shared_metrics.py:_identities_compatible */
bool rsm_flow_identities_compatible(const json_t *candidate, const json_t *observed);

/* ── 4. Open tool-call keys compatible with an observed identity ────────────
 * tool_call_keys: array of 4-element arrays (task_id, api_request_id, turn_id,
 * tool_call_id) in session.tool_calls insertion order. Returns a NEW array of
 * the matching keys (copies, order preserved); caller frees. */
/* PoP: rsm_flow_compatible_tool_call_keys @ hermes_cli/observability/relay_shared_metrics.py:_compatible_tool_call_keys */
json_t *rsm_flow_compatible_tool_call_keys(const json_t *tool_call_keys,
                                           const char *task_id,
                                           const json_t *identity);

/* ── 5. Merge one closed segment's context peak (fullest fill wins) ─────────
 * into/segment: {peak_route: str|null, peak_tokens: num, peak_window: num|null,
 * limit_hit: bool}. Mutates `into` in place; segment borrowed. A limit hit
 * anywhere sticks; the fuller ratio (tokens/window) adopts route/tokens/window.
 * Fixtures keep peak_window non-zero whenever peak_route is set (live divides). */
/* PoP: rsm_flow_absorb_peak @ hermes_cli/observability/relay_shared_metrics.py:_absorb_peak */
void rsm_flow_absorb_peak(json_t *into, const json_t *segment);

/* ── 6. Admission gate: may this event act on this task? ────────────────────
 * session/task/state per the header models; `current` additionally requires
 * the task to still be the session's live run for its id (ref identity).
 * Admitted events have their turn remembered (task.turn_ids set-append +
 * state.turn_sessions["<sid>\t<turn>"] = session_id). Mutates task/state.
 * Mirrors _event_matches_task_turn / _remember_turn (already-ported helpers;
 * the oracle drives the LIVE defs). */
/* PoP: rsm_flow_admits @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._admits */
bool rsm_flow_admits(json_t *state, json_t *session, json_t *task,
                     const json_t *event, bool current);

/* ── 7/8. Cold-cache expectation state machine (bounded FIFO, cap 256) ──────
 * expected_keys: JSON array of keys in OrderedDict insertion order (values
 * are all None live). announce: False when already expected, else append +
 * evict oldest past 256, True. consume: True when the key was expected
 * (pop(key, False) is None), removing it; False otherwise. Mutate in place. */
/* PoP: rsm_flow_announce_cold @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._announce_cold */
bool rsm_flow_announce_cold(json_t *expected_keys, const char *key);

/* PoP: rsm_flow_consume_cold @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._consume_cold */
bool rsm_flow_consume_cold(json_t *expected_keys, const char *key);

/* ── 9. Session-level turn aggregation (emitted once at session close) ──────
 * Clock seam monotonic_ns stands in for time.monotonic_ns (last_turn_ns).
 * First counted turn adopts task.start_fields (copy) + first_turn_ns;
 * failed_turns counts ONLY outcome == "failed". Mutates session in place. */
typedef struct rsm_clock_seams {
    void *ctx;
    long long (*monotonic_ns)(void *ctx);
} rsm_clock_seams_t;

/* PoP: rsm_flow_count_session_turn @ hermes_cli/observability/relay_shared_metrics.py:_Runtime._count_session_turn */
void rsm_flow_count_session_turn(json_t *session, const json_t *task,
                                 const char *outcome, const rsm_clock_seams_t *s);

/* ── 10. Model-call refresh/close flow (update_model_call) ──────────────────
 * session NULL models _any_session resolving None (silent early return).
 * Seams stand in for the sibling-module callees (contract.model_call_fields /
 * model_error_class, model_state.observe_call / observe_error,
 * fields_.ttft_bucket / model_token_fields, _observe_turn_call,
 * _finish_model_call, _mark) — all already ported or contract-owned; the
 * oracle scripts them identically. finish_model_call POPS the located key
 * from session.model_calls (its live state effect). The token mark runs under
 * _guarded: a raising mark logs WARNING and is swallowed.
 * Mutates session (fields/error_class/ttft_bucket/replies/model_calls). */
typedef struct rsm_update_seams {
    void *ctx;
    const json_t *(*model_call_fields)(void *ctx, const json_t *event);
    const char *(*model_error_class)(void *ctx, const json_t *event);
    void (*observe_call)(void *ctx, const json_t *fields, const json_t *usage,
                         const json_t *context_length);
    void (*observe_error)(void *ctx, const json_t *fields, const char *error_class);
    void (*observe_turn_call)(void *ctx, const json_t *model_call, const json_t *event);
    const char *(*ttft_bucket)(void *ctx, const json_t *event);
    void (*finish_model_call)(void *ctx, json_t *session, const char *key,
                              const char *outcome);
    const json_t *(*model_token_fields)(void *ctx, const json_t *usage,
                                        const json_t *fields);
    void (*mark)(void *ctx, const json_t *task, const char *name,
                 const json_t *data, bool *raised);
    void (*log_warning)(void *ctx, const char *msg);
} rsm_update_seams_t;

/* PoP: rsm_flow_update_model_call @ hermes_cli/observability/relay_shared_metrics.py:_Runtime.update_model_call */
void rsm_flow_update_model_call(json_t *session, const json_t *event, bool finish,
                                const rsm_update_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_SHARED_METRICS_FLOW_H */