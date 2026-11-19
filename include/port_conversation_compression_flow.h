/*
 * port_conversation_compression_flow.h -- compression-pipeline FLOW cores
 * ported from agent/conversation_compression.py.
 *
 * MISSION (flow parity doctrine): port the ORCHESTRATION logic of the
 * compression pipeline -- when to compress, what to keep/evict, summary
 * construction order, token-budget branches, failure/retry paths, state
 * transitions -- not just pure leaves. All live I/O (LLM summary call,
 * clock, tokenizer measurements, SessionDB writes, telemetry, warnings,
 * uuid minting, thread pools) is injected through SEAMS: scripted json_t
 * results in `in`. The decision cores never perform I/O themselves.
 *
 * Shapes:
 *  - Pure helpers return their value directly (json_t of any type).
 *  - State helpers return {"seq":[...], "state":{...}, "ret":...}: the
 *    ordered transition events, the final mutated state, the return value.
 *  - Flow cores return a fresh json_t ARRAY: the ordered DECISION SEQUENCE.
 *    Seam calls appear as {"a":"<seam>", ...args/ret}; the last element is
 *    {"a":"ret", ...} carrying the function's return (and final state where
 *    the live function mutates its arguments).
 *
 * `in` is borrowed (never freed/mutated). Returned json_t is caller-owned.
 * Extends port_conversation_compression.c (oracle-proven pure slice); the
 * context_compressor*.c files belong to a concurrent port and are untouched.
 */
#ifndef PORT_CONVERSATION_COMPRESSION_FLOW_H
#define PORT_CONVERSATION_COMPRESSION_FLOW_H

#include "../lib/libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── pure helpers ─────────────────────────────────────────────────────── */

/* compaction_status_kind: in {kind, text, marked} -> "compacting"|str(kind) */
json_t *ccflow_compaction_status_kind(const json_t *in);
/* _steer_markers: in {open, close, import_error} -> [open, close].
 * SEAM: agent.prompt_builder STEER_MARKER_OPEN/CLOSE import (scripted). */
json_t *ccflow_steer_markers(const json_t *in);
/* _message_contains_busy_steer: in {message, markers:[open,close]} -> bool.
 * SEAM: _steer_markers result injected as `markers`. */
json_t *ccflow_message_contains_busy_steer(const json_t *in);
/* _extract_steer_text_from_message: same in -> str|null */
json_t *ccflow_extract_steer_text_from_message(const json_t *in);
/* _compressed_has_busy_steer: in {messages, markers} -> bool */
json_t *ccflow_compressed_has_busy_steer(const json_t *in);
/* _messages_match_scoped_identity: in {left, right} -> bool */
json_t *ccflow_messages_match_scoped_identity(const json_t *in);
/* _stamp_scoped_twins: in {targets, source, exact_counts_stamped} -> targets */
json_t *ccflow_stamp_scoped_twins(const json_t *in);
/* _todo_snapshot_is_only_content: in {content, stripped} -> bool */
json_t *ccflow_todo_snapshot_is_only_content(const json_t *in);
/* _direct_messages_for_pre_compress_memory: in {messages} -> list */
json_t *ccflow_direct_messages_for_pre_compress_memory(const json_t *in);
/* _prior_timeout_failures: in {count, kind} -> int (type-pinned) */
json_t *ccflow_prior_timeout_failures(const json_t *in);
/* request_exceeds_model_window: in {window, window_kind, tokens, tokens_kind}
 * -> bool|null (type-pinned both sides) */
json_t *ccflow_request_exceeds_model_window(const json_t *in);
/* _stall_source_fingerprint: in {summary_model, agent_model, messages,
 * approx_tokens, tokens_kind} -> "msgs=N:tokens=T:model=M" */
json_t *ccflow_stall_source_fingerprint(const json_t *in);
/* _restore_prune_rearm_tokens: in {state, snapshot} -> mutated state */
json_t *ccflow_restore_prune_rearm_tokens(const json_t *in);
/* _existing_system_prompt: in {cached, system_message, build_result} -> str.
 * SEAM: agent._build_system_prompt (scripted result, called iff cached falsy) */
json_t *ccflow_existing_system_prompt(const json_t *in);
/* _configured_compression_context_length: in {config_error, config_raw,
 * fallback} -> int|null. SEAM: _get_auxiliary_task_config read. */
json_t *ccflow_configured_compression_context_length(const json_t *in);
/* context_engine_persistence: in {accepts, persistence} -> dict.
 * SEAM: accepts_keyword_argument(hook, "persistence"). */
json_t *ccflow_context_engine_persistence(const json_t *in);
/* _restore_messages_snapshot: in {messages, snapshot} -> {restored, messages} */
json_t *ccflow_restore_messages_snapshot(const json_t *in);
/* _pruned_skill_reload_notice: in {compressed, extracted:[[names...]...],
 * max_markers} -> str. SEAM: _extract_pruned_skill_names per message text. */
json_t *ccflow_pruned_skill_reload_notice(const json_t *in);

/* ── attempt-ownership / timeout state machines ───────────────────────── */

json_t *ccflow_compressor_attempt_serial_lock(const json_t *in);
json_t *ccflow_claim_compressor_attempt(const json_t *in);
json_t *ccflow_compressor_attempt_is_current(const json_t *in);
json_t *ccflow_mark_compressor_working_attempt(const json_t *in);
json_t *ccflow_working_attempt_is_current(const json_t *in);
json_t *ccflow_caller_attempt_is_current(const json_t *in);
json_t *ccflow_raise_if_stale_attempt(const json_t *in);
json_t *ccflow_install_compression_cancelled_check(const json_t *in);
json_t *ccflow_clear_compression_cancelled_check_if_owner(const json_t *in);
json_t *ccflow_get_context_compression_timeout_state(const json_t *in);
json_t *ccflow_set_context_compression_timeout_outcome(const json_t *in);
json_t *ccflow_reset_context_compression_timeout_outcome(const json_t *in);
json_t *ccflow_mark_context_compression_timed_out(const json_t *in);
json_t *ccflow_mark_compression_blocked_transient(const json_t *in);

/* ── flow cores (ordered decision sequences) ──────────────────────────── */

json_t *ccflow_swallow(const json_t *in);
json_t *ccflow_emit_feasibility_notice(const json_t *in);
json_t *ccflow_warn_summary_or_aux_fallback(const json_t *in);
json_t *ccflow_refresh_main_credential(const json_t *in);
json_t *ccflow_hand_off_metrics_segment(const json_t *in);
json_t *ccflow_reset_read_dedup_caches(const json_t *in);
json_t *ccflow_rebind_session_context(const json_t *in);
json_t *ccflow_aux_inherits_main_route(const json_t *in);
json_t *ccflow_compression_child_source(const json_t *in);
json_t *ccflow_held_watermark(const json_t *in);
json_t *ccflow_automatic_compression_gate_blocks(const json_t *in);
json_t *ccflow_begin_compression_attempt(const json_t *in);
json_t *ccflow_announce_compression_start(const json_t *in);
json_t *ccflow_resolve_lock_api(const json_t *in);
json_t *ccflow_release_compression_lock(const json_t *in);
json_t *ccflow_try_acquire_durable_lock(const json_t *in);
json_t *ccflow_abort_lease(const json_t *in);
json_t *ccflow_sit_out_lock_contention(const json_t *in);
json_t *ccflow_acquire_compression_lease(const json_t *in);
json_t *ccflow_adopt_if_parent_rotated(const json_t *in);
json_t *ccflow_adopt_grown_durable_parent(const json_t *in);
json_t *ccflow_pre_compress_memory_context(const json_t *in);
json_t *ccflow_resolve_compress_call(const json_t *in);
json_t *ccflow_run_summary_dispatch(const json_t *in);
json_t *ccflow_run_summary_phase(const json_t *in);
json_t *ccflow_candidate_rejected(const json_t *in);
json_t *ccflow_salvage_or_refuse_grown_transcript(const json_t *in);
json_t *ccflow_fold_todo_snapshot(const json_t *in);
json_t *ccflow_rebuild_system_prompt_at_boundary(const json_t *in);
json_t *ccflow_commit_compaction(const json_t *in);
json_t *ccflow_finish_compaction_boundary(const json_t *in);
json_t *ccflow_publish_rotated_compaction(const json_t *in);
json_t *ccflow_carry_session_state_to_child(const json_t *in);
json_t *ccflow_route_codex_compaction(const json_t *in);
json_t *ccflow_stall_retry_routes(const json_t *in);
json_t *ccflow_retry_compression_on_fallback_chain(const json_t *in);
json_t *ccflow_run_pinned_compression_retry(const json_t *in);
json_t *ccflow_join_cancelled_worker(const json_t *in);
json_t *ccflow_await_worker_within_budget(const json_t *in);
json_t *ccflow_await_in_flight_commit(const json_t *in);
json_t *ccflow_cancel_or_join_worker(const json_t *in);
json_t *ccflow_release_cancelled_worker(const json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CONVERSATION_COMPRESSION_FLOW_H */