/*
 * moa_loop_flow.h — fan-out / aggregation FLOW cores + leaves of
 * agent/moa_loop.py (quarry: C:/Users/eman5/hermes-agent).
 *
 * Ports the DECISION sequences of the Mixture-of-Agents turn path: the
 * advisor fan-out cache cadence (_fanout_cache_key), the cache-MISS fan-out
 * orchestration and emission ORDER (_run_fanout), the guidance rendering
 * branches (_build_guidance / _guidance_inputs / _join_reference_outputs),
 * usage/cost aggregation (_sum_reference_accounting / _fold_pending_accounting),
 * the user-interrupt cancellation path (_settle_interrupted / _record_late),
 * the reference metrics / context-window decision chains
 * (_price_reference_response / _reference_context_length) and the supporting
 * leaves (_field / _slot_labels / _agent_cache_opts / _with_cache_disabled /
 * _hash_messages / _tool_activity_since_last_user / _placeholder_output).
 * Every live model-call / sibling / clock effect enters through ONE universal
 * seam (mlf_io_t.call); the cores perform no I/O.
 *
 * Already claimed elsewhere (NOT re-ported here; they appear only as scripted
 * seam names): _slot_label, _slot_runtime, _slot_reasoning_config,
 * _aggregator_reasoning_config, _merge_slot_extra_body, _run_reference,
 * _run_references_parallel, _trim_messages_for_reference, _reference_messages,
 * _extract_text, _preset_temperature, _is_failed_reference, _degraded_notice,
 * _redact_reference_text, _redact_reference_outputs, _redact_trace_messages,
 * _redact_trace_accounting, _maybe_apply_moa_cache_control, _truncate_tool_result,
 * _render_tool_calls, _emit, _record_late_reference_accounting,
 * consume_reference_usage, consume_and_save_trace, last_reference_metrics,
 * prepare, rebase_prepared_request, create, _call_prepared_aggregator,
 * aggregate_moa_context, build_moa_facade, peel_reference_guidance,
 * _attach_reference_guidance, _completed_response_as_stream_chunk
 * (src/agent/moa_loop_ports.c, moa_loop_wrappers.c, moa_loop_gaps3.c).
 *
 * ── Universal seam contract ────────────────────────────────────────────────
 *   json_t *call(void *ctx, const char *name, const json_t *args)
 * args is a BORROWED object (None-valued keys dropped). The result is a NEW
 * json_t owned by the caller: NULL == Python None; an object carrying a string
 * "__raise__" key ({"__raise__": cls, "msg": str}) == an exception of that
 * class with str(exc) == msg, which the core propagates or catches exactly
 * where the live code does. The harness stub records one {ev, args, ret}
 * event per call (ret = the RAW scripted value) and the oracle's recording
 * monkeypatches emit the identical shape.
 *
 * Script shapes (fixture "io" object, per seam name):
 *   absent / null        -> None on every call
 *   {"seq": [v1, v2..]}  -> vi on the i-th call (0-based); None when exhausted
 *   any other value      -> reused on every call
 *
 * ── Seam name table (name -> live call) ────────────────────────────────────
 *   _slot_label                 module sibling (claimed port)
 *   _is_failed_reference        module sibling (claimed port)
 *   _redact_reference_outputs   module sibling (claimed port)
 *   _redact_reference_text      module sibling (claimed port)
 *   _redact_trace_accounting    module sibling (claimed port)
 *   _degraded_notice            module sibling (claimed port)
 *   _preset_temperature         module sibling (claimed port)
 *   _run_references_parallel    module sibling (claimed port); the scripted
 *                               value is {"outputs": [...], "progress": [...]}
 *                               — the stub fires one progress_callback per
 *                               "progress" entry (each invokes the _emit seam)
 *                               BEFORE returning materialized "outputs".
 *   _sum_reference_accounting   module sibling target (seamed in composites)
 *   _fold_pending_accounting    facade method target (seamed in composites)
 *   _guidance_inputs            module sibling target (seamed in composites)
 *   _join_reference_outputs     module sibling target (seamed in composites)
 *   _placeholder_output         module sibling target (seamed in composites)
 *   _hash_messages              module sibling target (seamed in composites)
 *   _emit                       facade method (claimed port); never raises
 *   slot_metrics                agent.moa_trace.slot_metrics
 *   get_model_context_length    agent.model_metadata.get_model_context_length
 *   normalize_usage             agent.usage_pricing.normalize_usage
 *   estimate_usage_cost         agent.usage_pricing.estimate_usage_cost
 *   future.cancel               concurrent.futures.Future.cancel
 *   future.done                 concurrent.futures.Future.done
 *   future.result               concurrent.futures.Future.result
 *   future.add_done_callback    concurrent.futures.Future.add_done_callback
 *   sink                        the late_accounting_sink callable (_record_late)
 *
 * ── Value model ────────────────────────────────────────────────────────────
 * CanonicalUsage  -> {"_type":"CanonicalUsage", input_tokens, output_tokens,
 *                     cache_read_tokens, cache_write_tokens, reasoning_tokens,
 *                     request_count} (the six summed fields; raw_usage and
 *                     cache_write_by_ttl are per-response detail dropped by
 *                     __add__ and never carried by a returned sum).
 * _RefAccounting  -> {"_type":"_RefAccounting", usage, cost_usd, cost_status,
 *                     cost_source, messages, output, model, provider,
 *                     temperature}. A value WITHOUT the matching _type tag is
 *                     "not isinstance" of that class (the isinstance gates).
 * attribute object-> {"__attr__": {name: value}} (getattr double); a plain
 *                     object is a dict. None == JSON null.
 * reference output-> [label, text, acct] (the live 3-tuple).
 * cache key tuple -> [preset_name, hash, [labels...]].
 * (provider,model) cache key -> "provider\tmodel" object key.
 *
 * ── Facade state window (borrowed mutable "state" object) ──────────────────
 *   preset_name, _privacy_mode, _agent,
 *   _fanout_turn_sig, _fanout_iteration_count, _fanout_last_state_sig,
 *   _ref_cache_key, _ref_cache_outputs,
 *   _pending_reference_usage, _pending_reference_cost,
 *   _pending_trace, _last_reference_metrics,
 *   ctx_cache (the (provider,model)->window memo for _reference_context_length),
 *   results   (the fan-out results list for _settle_interrupted).
 * Each core reads/writes only the subset its live def touches; the oracle
 * dumps the same window back. Seam handles are scripted I/O, not state.
 *
 * ── Calling convention ─────────────────────────────────────────────────────
 * Every core: json_t *mlf_<fn>(const json_t *in, const mlf_io_t *io,
 *                              json_t *state, json_t **exc)
 *   in    borrowed case input object.
 *   state borrowed mutable facade-state object.
 *   exc   out-param: on a propagating exception, set to the NEW raise-spec
 *         object and the core returns NULL. *exc is NULL on entry.
 *   ret   NEW json_t: the Python return value (tuple -> array, None ->
 *         json_null(), str -> string, bool -> bool, float -> number).
 *
 * Fixture scope: _hash_messages role/content are str|None|number (the advisory
 * view always carries string content); str.strip()/lower() model ASCII
 * whitespace + U+0085/U+00A0 and ASCII case; integral floats normalize to int.
 */
#ifndef MOA_LOOP_FLOW_H
#define MOA_LOOP_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mlf_io {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} mlf_io_t;

/* Leaves. */
json_t *mlf_field(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_slot_labels(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_agent_cache_opts(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_with_cache_disabled(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_hash_messages(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_tool_activity_since_last_user(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_placeholder_output(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);

/* Aggregation / rendering. */
json_t *mlf_join_reference_outputs(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_guidance_inputs(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_sum_reference_accounting(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_fold_pending_accounting(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);

/* Reference metrics / window decision chains. */
json_t *mlf_reference_context_length(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_price_reference_response(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);

/* Cancellation / error paths. */
json_t *mlf_record_late(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_settle_interrupted(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);

/* Fan-out / guidance flows (facade methods). */
json_t *mlf_fanout_cache_key(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_build_guidance(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);
json_t *mlf_run_fanout(const json_t *in, const mlf_io_t *io, json_t *state, json_t **exc);

#ifdef __cplusplus
}
#endif

#endif /* MOA_LOOP_FLOW_H */