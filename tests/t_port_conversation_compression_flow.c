/*
 * t_port_conversation_compression_flow.c -- oracle harness for
 * port_conversation_compression_flow.c (compression-pipeline FLOW).
 *
 * Producer side of the differential gate: emits one JSON line per scripted
 * case, {"func":..., "in":{...}, "out":...}, where "out" is the value (pure
 * helpers), the {seq,state,ret} record (state machines), or the ordered
 * DECISION SEQUENCE (flow cores) produced by the C core under the scripted
 * I/O in "in". tests/oracle_conversation_compression_flow.py replays the same
 * scripted inputs through the LIVE Python and compares.
 *
 * Link: src/agent/port_conversation_compression_flow.o
 *       src/agent/port_conversation_compression.o lib/libjson/json.o
 */
#include "port_conversation_compression_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ccflow_case {
    const char *func;
    const char *in;
};

static json_t *dispatch(const char *func, const json_t *in)
{
    if (strcmp(func, "compaction_status_kind") == 0) return ccflow_compaction_status_kind(in);
    if (strcmp(func, "steer_markers") == 0) return ccflow_steer_markers(in);
    if (strcmp(func, "message_contains_busy_steer") == 0) return ccflow_message_contains_busy_steer(in);
    if (strcmp(func, "extract_steer_text_from_message") == 0) return ccflow_extract_steer_text_from_message(in);
    if (strcmp(func, "compressed_has_busy_steer") == 0) return ccflow_compressed_has_busy_steer(in);
    if (strcmp(func, "messages_match_scoped_identity") == 0) return ccflow_messages_match_scoped_identity(in);
    if (strcmp(func, "stamp_scoped_twins") == 0) return ccflow_stamp_scoped_twins(in);
    if (strcmp(func, "todo_snapshot_is_only_content") == 0) return ccflow_todo_snapshot_is_only_content(in);
    if (strcmp(func, "direct_messages_for_pre_compress_memory") == 0) return ccflow_direct_messages_for_pre_compress_memory(in);
    if (strcmp(func, "prior_timeout_failures") == 0) return ccflow_prior_timeout_failures(in);
    if (strcmp(func, "request_exceeds_model_window") == 0) return ccflow_request_exceeds_model_window(in);
    if (strcmp(func, "stall_source_fingerprint") == 0) return ccflow_stall_source_fingerprint(in);
    if (strcmp(func, "restore_prune_rearm_tokens") == 0) return ccflow_restore_prune_rearm_tokens(in);
    if (strcmp(func, "existing_system_prompt") == 0) return ccflow_existing_system_prompt(in);
    if (strcmp(func, "configured_compression_context_length") == 0) return ccflow_configured_compression_context_length(in);
    if (strcmp(func, "context_engine_persistence") == 0) return ccflow_context_engine_persistence(in);
    if (strcmp(func, "restore_messages_snapshot") == 0) return ccflow_restore_messages_snapshot(in);
    if (strcmp(func, "pruned_skill_reload_notice") == 0) return ccflow_pruned_skill_reload_notice(in);
    if (strcmp(func, "compressor_attempt_serial_lock") == 0) return ccflow_compressor_attempt_serial_lock(in);
    if (strcmp(func, "claim_compressor_attempt") == 0) return ccflow_claim_compressor_attempt(in);
    if (strcmp(func, "compressor_attempt_is_current") == 0) return ccflow_compressor_attempt_is_current(in);
    if (strcmp(func, "mark_compressor_working_attempt") == 0) return ccflow_mark_compressor_working_attempt(in);
    if (strcmp(func, "working_attempt_is_current") == 0) return ccflow_working_attempt_is_current(in);
    if (strcmp(func, "caller_attempt_is_current") == 0) return ccflow_caller_attempt_is_current(in);
    if (strcmp(func, "raise_if_stale_attempt") == 0) return ccflow_raise_if_stale_attempt(in);
    if (strcmp(func, "install_compression_cancelled_check") == 0) return ccflow_install_compression_cancelled_check(in);
    if (strcmp(func, "clear_compression_cancelled_check_if_owner") == 0) return ccflow_clear_compression_cancelled_check_if_owner(in);
    if (strcmp(func, "get_context_compression_timeout_state") == 0) return ccflow_get_context_compression_timeout_state(in);
    if (strcmp(func, "set_context_compression_timeout_outcome") == 0) return ccflow_set_context_compression_timeout_outcome(in);
    if (strcmp(func, "reset_context_compression_timeout_outcome") == 0) return ccflow_reset_context_compression_timeout_outcome(in);
    if (strcmp(func, "mark_context_compression_timed_out") == 0) return ccflow_mark_context_compression_timed_out(in);
    if (strcmp(func, "mark_compression_blocked_transient") == 0) return ccflow_mark_compression_blocked_transient(in);
    if (strcmp(func, "swallow") == 0) return ccflow_swallow(in);
    if (strcmp(func, "emit_feasibility_notice") == 0) return ccflow_emit_feasibility_notice(in);
    if (strcmp(func, "warn_summary_or_aux_fallback") == 0) return ccflow_warn_summary_or_aux_fallback(in);
    if (strcmp(func, "refresh_main_credential") == 0) return ccflow_refresh_main_credential(in);
    if (strcmp(func, "hand_off_metrics_segment") == 0) return ccflow_hand_off_metrics_segment(in);
    if (strcmp(func, "reset_read_dedup_caches") == 0) return ccflow_reset_read_dedup_caches(in);
    if (strcmp(func, "rebind_session_context") == 0) return ccflow_rebind_session_context(in);
    if (strcmp(func, "aux_inherits_main_route") == 0) return ccflow_aux_inherits_main_route(in);
    if (strcmp(func, "compression_child_source") == 0) return ccflow_compression_child_source(in);
    if (strcmp(func, "held_watermark") == 0) return ccflow_held_watermark(in);
    if (strcmp(func, "automatic_compression_gate_blocks") == 0) return ccflow_automatic_compression_gate_blocks(in);
    if (strcmp(func, "begin_compression_attempt") == 0) return ccflow_begin_compression_attempt(in);
    if (strcmp(func, "announce_compression_start") == 0) return ccflow_announce_compression_start(in);
    if (strcmp(func, "resolve_lock_api") == 0) return ccflow_resolve_lock_api(in);
    if (strcmp(func, "release_compression_lock") == 0) return ccflow_release_compression_lock(in);
    if (strcmp(func, "try_acquire_durable_lock") == 0) return ccflow_try_acquire_durable_lock(in);
    if (strcmp(func, "abort_lease") == 0) return ccflow_abort_lease(in);
    if (strcmp(func, "sit_out_lock_contention") == 0) return ccflow_sit_out_lock_contention(in);
    if (strcmp(func, "acquire_compression_lease") == 0) return ccflow_acquire_compression_lease(in);
    if (strcmp(func, "adopt_if_parent_rotated") == 0) return ccflow_adopt_if_parent_rotated(in);
    if (strcmp(func, "adopt_grown_durable_parent") == 0) return ccflow_adopt_grown_durable_parent(in);
    if (strcmp(func, "pre_compress_memory_context") == 0) return ccflow_pre_compress_memory_context(in);
    if (strcmp(func, "resolve_compress_call") == 0) return ccflow_resolve_compress_call(in);
    if (strcmp(func, "run_summary_dispatch") == 0) return ccflow_run_summary_dispatch(in);
    if (strcmp(func, "run_summary_phase") == 0) return ccflow_run_summary_phase(in);
    if (strcmp(func, "candidate_rejected") == 0) return ccflow_candidate_rejected(in);
    if (strcmp(func, "salvage_or_refuse_grown_transcript") == 0) return ccflow_salvage_or_refuse_grown_transcript(in);
    if (strcmp(func, "fold_todo_snapshot") == 0) return ccflow_fold_todo_snapshot(in);
    if (strcmp(func, "rebuild_system_prompt_at_boundary") == 0) return ccflow_rebuild_system_prompt_at_boundary(in);
    if (strcmp(func, "commit_compaction") == 0) return ccflow_commit_compaction(in);
    if (strcmp(func, "finish_compaction_boundary") == 0) return ccflow_finish_compaction_boundary(in);
    if (strcmp(func, "publish_rotated_compaction") == 0) return ccflow_publish_rotated_compaction(in);
    if (strcmp(func, "carry_session_state_to_child") == 0) return ccflow_carry_session_state_to_child(in);
    if (strcmp(func, "route_codex_compaction") == 0) return ccflow_route_codex_compaction(in);
    if (strcmp(func, "stall_retry_routes") == 0) return ccflow_stall_retry_routes(in);
    if (strcmp(func, "retry_compression_on_fallback_chain") == 0) return ccflow_retry_compression_on_fallback_chain(in);
    if (strcmp(func, "run_pinned_compression_retry") == 0) return ccflow_run_pinned_compression_retry(in);
    if (strcmp(func, "join_cancelled_worker") == 0) return ccflow_join_cancelled_worker(in);
    if (strcmp(func, "await_worker_within_budget") == 0) return ccflow_await_worker_within_budget(in);
    if (strcmp(func, "await_in_flight_commit") == 0) return ccflow_await_in_flight_commit(in);
    if (strcmp(func, "cancel_or_join_worker") == 0) return ccflow_cancel_or_join_worker(in);
    if (strcmp(func, "release_cancelled_worker") == 0) return ccflow_release_cancelled_worker(in);
    return json_null();
}

static const struct ccflow_case cases[] = {
/* ---- pure helpers ---- */
{"compaction_status_kind", "{\"kind\":\"lifecycle\",\"text\":\"Compacting context\",\"marked\":true}"},
{"compaction_status_kind", "{\"kind\":\"lifecycle\",\"text\":\"plain status\",\"marked\":false}"},
{"compaction_status_kind", "{\"kind\":\"lifecycle\",\"text\":\"   \",\"marked\":true}"},
{"compaction_status_kind", "{\"kind\":\"tool\",\"text\":\"Compacting context\",\"marked\":true}"},
{"steer_markers", "{\"open\":\"<STEER>\",\"close\":\"</STEER>\",\"import_error\":false}"},
{"steer_markers", "{\"import_error\":true}"},
{"steer_markers", "{\"open\":\"\",\"close\":\"\",\"import_error\":false}"},
{"message_contains_busy_steer", "{\"message\":{\"role\":\"tool\",\"content\":\"x [OUT-OF-BAND USER MESSAGE y [/OUT-OF-BAND USER MESSAGE] z\"}}"},
{"message_contains_busy_steer", "{\"message\":{\"role\":\"tool\",\"content\":\"only [OUT-OF-BAND USER MESSAGE open\"}}"},
{"message_contains_busy_steer", "{\"message\":{\"role\":\"tool\",\"content\":\"\"}}"},
{"message_contains_busy_steer", "{\"message\":{\"role\":\"tool\",\"content\":\"a <S> b </S> c\"},\"markers\":[\"<S>\",\"</S>\"]}"},
{"message_contains_busy_steer", "{\"message\":{\"role\":\"tool\",\"content\":[{\"type\":\"text\",\"text\":\"[OUT-OF-BAND USER MESSAGE\"},{\"type\":\"text\",\"text\":\"[/OUT-OF-BAND USER MESSAGE]\"}]}}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"[OUT-OF-BAND USER MESSAGE\\nhello world\\n[/OUT-OF-BAND USER MESSAGE]\"}}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"pre <S> inner </S> post\"},\"markers\":[\"<S>\",\"</S>\"]}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"[OUT-OF-BAND USER MESSAGE tail[/OUT-OF-BAND USER MESSAGE]\"}}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"[OUT-OF-BAND USER MESSAGE\\nno close\"}}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"<S>   </S>\"},\"markers\":[\"<S>\",\"</S>\"]}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"<S>abc[/OUT-OF-BAND USER MESSAGE]\"},\"markers\":[\"<S>\",\"</S>\"]}"},
{"extract_steer_text_from_message", "{\"message\":{\"content\":\"no markers at all\"}}"},
{"compressed_has_busy_steer", "{\"messages\":[{\"role\":\"user\",\"content\":\"[OUT-OF-BAND USER MESSAGE [/OUT-OF-BAND USER MESSAGE]\"},{\"role\":\"tool\",\"content\":\"[OUT-OF-BAND USER MESSAGE [/OUT-OF-BAND USER MESSAGE]\"}]}"},
{"compressed_has_busy_steer", "{\"messages\":[{\"role\":\"user\",\"content\":\"[OUT-OF-BAND USER MESSAGE [/OUT-OF-BAND USER MESSAGE]\"}]}"},
{"compressed_has_busy_steer", "{\"messages\":[]}"},
{"compressed_has_busy_steer", "{\"messages\":[\"x\",{\"role\":\"tool\",\"content\":\"<S></S>\"}],\"markers\":[\"<S>\",\"</S>\"]}"},
{"messages_match_scoped_identity", "{\"left\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5},\"right\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5}}"},
{"messages_match_scoped_identity", "{\"left\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5},\"right\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":9}}"},
{"messages_match_scoped_identity", "{\"left\":{\"role\":\"user\",\"content\":\"hi\"},\"right\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":9}}"},
{"messages_match_scoped_identity", "{\"left\":{\"role\":\"user\",\"content\":\"hi\"},\"right\":{\"role\":\"assistant\",\"content\":\"hi\"}}"},
{"messages_match_scoped_identity", "{\"left\":\"x\",\"right\":{\"role\":\"user\"}}"},
{"stamp_scoped_twins", "{\"targets\":[{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5},{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":9}],\"source\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5},\"exact_counts_stamped\":false}"},
{"stamp_scoped_twins", "{\"targets\":[{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5,\"_db_persisted\":true},{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":9}],\"source\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5},\"exact_counts_stamped\":true}"},
{"stamp_scoped_twins", "{\"targets\":[{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5,\"_db_persisted\":true},{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":9}],\"source\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5},\"exact_counts_stamped\":false}"},
{"stamp_scoped_twins", "{\"targets\":[{\"role\":\"user\",\"content\":\"hi\"},{\"role\":\"user\",\"content\":\"other\"}],\"source\":{\"role\":\"user\",\"content\":\"hi\"}}"},
{"stamp_scoped_twins", "{\"targets\":[{\"role\":\"assistant\",\"content\":\"hi\",\"timestamp\":5}],\"source\":{\"role\":\"user\",\"content\":\"hi\",\"timestamp\":5}}"},
{"todo_snapshot_is_only_content", "{\"content\":\"abc\",\"stripped\":\"\"}"},
{"todo_snapshot_is_only_content", "{\"content\":\"abc\",\"stripped\":\"  \"}"},
{"todo_snapshot_is_only_content", "{\"content\":\"abc\",\"stripped\":\"x\"}"},
{"todo_snapshot_is_only_content", "{\"content\":[{\"type\":\"text\",\"text\":\"s\"}],\"stripped\":[]}"},
{"todo_snapshot_is_only_content", "{\"content\":[{\"type\":\"text\",\"text\":\"s\"}],\"stripped\":[{\"type\":\"image\"}]}"},
{"todo_snapshot_is_only_content", "{\"content\":\"abc\",\"stripped\":[]}"},
{"direct_messages_for_pre_compress_memory", "{\"messages\":[{\"role\":\"user\",\"content\":\"a\"},{\"role\":\"assistant\",\"content\":\"b\",\"tool_calls\":[{}]},{\"role\":\"assistant\",\"content\":\"  \",\"tool_calls\":[{}]},{\"role\":\"assistant\",\"content\":\"prose\",\"tool_calls\":[{}]},{\"role\":\"tool\",\"content\":\"t\"},{\"role\":\"user\",\"content\":\"s\",\"_compressed_summary\":true},\"notdict\",{\"role\":\"system\",\"content\":\"x\"}]}"},
{"direct_messages_for_pre_compress_memory", "{\"messages\":null}"},
{"direct_messages_for_pre_compress_memory", "{\"messages\":[{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"p\"}],\"tool_calls\":[{}]}]}"},
{"prior_timeout_failures", "{\"count\":3,\"kind\":\"int\"}"},
{"prior_timeout_failures", "{\"count\":true,\"kind\":\"bool\"}"},
{"prior_timeout_failures", "{\"count\":2.5,\"kind\":\"float\"}"},
{"prior_timeout_failures", "{\"count\":\"2\",\"kind\":\"str\"}"},
{"prior_timeout_failures", "{\"kind\":\"missing\"}"},
{"request_exceeds_model_window", "{\"window\":100,\"window_kind\":\"int\",\"tokens\":150,\"tokens_kind\":\"int\"}"},
{"request_exceeds_model_window", "{\"window\":100,\"window_kind\":\"int\",\"tokens\":100,\"tokens_kind\":\"int\"}"},
{"request_exceeds_model_window", "{\"window\":true,\"window_kind\":\"bool\",\"tokens\":150,\"tokens_kind\":\"int\"}"},
{"request_exceeds_model_window", "{\"window\":100,\"window_kind\":\"int\",\"tokens\":1.5,\"tokens_kind\":\"float\"}"},
{"request_exceeds_model_window", "{\"window\":0,\"window_kind\":\"int\",\"tokens\":150,\"tokens_kind\":\"int\"}"},
{"stall_source_fingerprint", "{\"summary_model\":\"aux\",\"agent_model\":\"main\",\"messages\":[1,2],\"approx_tokens\":\"12\",\"tokens_kind\":\"str\"}"},
{"stall_source_fingerprint", "{\"summary_model\":\"\",\"agent_model\":\"main\",\"messages\":[],\"approx_tokens\":12.9,\"tokens_kind\":\"float\"}"},
{"stall_source_fingerprint", "{\"summary_model\":null,\"agent_model\":null,\"messages\":\"x\",\"approx_tokens\":\"bad\",\"tokens_kind\":\"str\"}"},
{"stall_source_fingerprint", "{\"summary_model\":\"m\",\"agent_model\":\"\",\"messages\":[1],\"approx_tokens\":null,\"tokens_kind\":\"null\"}"},
{"stall_source_fingerprint", "{\"summary_model\":\"m\",\"messages\":[],\"approx_tokens\":true,\"tokens_kind\":\"bool\"}"},
{"restore_prune_rearm_tokens", "{\"state\":{\"a\":1,\"_proactive_prune_rearm_tokens\":9},\"snapshot\":{\"_proactive_prune_rearm_tokens\":5,\"_proactive_prune_cold_credit\":2}}"},
{"restore_prune_rearm_tokens", "{\"state\":{\"a\":1},\"snapshot\":{}}"},
{"restore_prune_rearm_tokens", "{\"state\":{},\"snapshot\":{\"_proactive_prune_cold_credit\":7}}"},
{"existing_system_prompt", "{\"cached\":\"P\",\"system_message\":\"s\",\"build_result\":\"B\"}"},
{"existing_system_prompt", "{\"cached\":\"\",\"system_message\":\"s\",\"build_result\":\"B\"}"},
{"existing_system_prompt", "{\"cached\":null,\"system_message\":\"s\",\"build_result\":\"B\"}"},
{"configured_compression_context_length", "{\"config_raw\":32000,\"raw_kind\":\"int\"}"},
{"configured_compression_context_length", "{\"config_raw\":\"32000\",\"raw_kind\":\"str\"}"},
{"configured_compression_context_length", "{\"config_raw\":\"x\",\"raw_kind\":\"str\"}"},
{"configured_compression_context_length", "{\"config_raw\":null}"},
{"configured_compression_context_length", "{\"config_error\":true,\"fallback\":16000,\"fallback_kind\":\"int\"}"},
{"configured_compression_context_length", "{\"config_error\":true,\"fallback\":null}"},
{"context_engine_persistence", "{\"accepts\":true,\"persistence\":{\"x\":1}}"},
{"context_engine_persistence", "{\"accepts\":false,\"persistence\":{\"x\":1}}"},
{"restore_messages_snapshot", "{\"messages\":[1,2],\"snapshot\":[1,3]}"},
{"restore_messages_snapshot", "{\"messages\":[1,2],\"snapshot\":[1,2]}"},
{"restore_messages_snapshot", "{\"messages\":[1,2],\"snapshot\":null}"},
{"pruned_skill_reload_notice", "{\"compressed\":[{\"role\":\"user\"},{\"role\":\"assistant\"},\"x\"],\"extracted\":[[\"a\",\"b\"],[\"a\"],[\"c\"]],\"max_markers\":20}"},
{"pruned_skill_reload_notice", "{\"compressed\":[{\"role\":\"user\"},{\"role\":\"user\"}],\"extracted\":[[\"a\",\"b\",\"c\"],[\"d\"]],\"max_markers\":2}"},
{"pruned_skill_reload_notice", "{\"compressed\":[{\"role\":\"user\"}],\"extracted\":[[]],\"max_markers\":20}"},
{"pruned_skill_reload_notice", "{\"compressed\":[],\"extracted\":[],\"max_markers\":20}"},
/* ---- attempt ownership state machines ---- */
{"compressor_attempt_serial_lock", "{\"state\":{},\"frozen\":false}"},
{"compressor_attempt_serial_lock", "{\"state\":{},\"frozen\":true}"},
{"compressor_attempt_serial_lock", "{\"state\":{\"_compression_attempt_serial_lock\":true},\"frozen\":false}"},
{"claim_compressor_attempt", "{\"state\":{},\"frozen\":false}"},
{"claim_compressor_attempt", "{\"state\":{\"_compression_attempt_generation\":4},\"frozen\":false}"},
{"claim_compressor_attempt", "{\"state\":{},\"frozen\":true}"},
{"claim_compressor_attempt", "{\"state\":{\"_compression_attempt_generation\":\"\"},\"frozen\":false}"},
{"compressor_attempt_is_current", "{\"state\":{\"_compression_attempt_generation\":3},\"generation\":3}"},
{"compressor_attempt_is_current", "{\"state\":{\"_compression_attempt_generation\":4},\"generation\":3}"},
{"compressor_attempt_is_current", "{\"state\":{\"_compression_attempt_generation\":4},\"generation\":0}"},
{"mark_compressor_working_attempt", "{\"state\":{},\"generation\":5,\"frozen\":false}"},
{"mark_compressor_working_attempt", "{\"state\":{},\"generation\":5,\"frozen\":true}"},
{"working_attempt_is_current", "{\"state\":{},\"generation\":3}"},
{"working_attempt_is_current", "{\"state\":{\"_compression_attempt_generation\":3},\"generation\":3}"},
{"working_attempt_is_current", "{\"state\":{\"_compression_attempt_generation\":4},\"generation\":3}"},
{"working_attempt_is_current", "{\"state\":{\"_compression_working_attempt_generation\":5},\"generation\":5}"},
{"working_attempt_is_current", "{\"state\":{\"_compression_working_attempt_generation\":5,\"_compression_attempt_generation\":3},\"generation\":3}"},
{"working_attempt_is_current", "{\"state\":{\"_compression_working_attempt_generation\":5},\"generation\":0}"},
{"caller_attempt_is_current", "{\"state\":{\"_compression_working_attempt_generation\":5},\"ctx_generation\":5}"},
{"caller_attempt_is_current", "{\"state\":{\"_compression_working_attempt_generation\":5},\"ctx_generation\":null}"},
{"caller_attempt_is_current", "{\"state\":{\"_compression_working_attempt_generation\":5},\"ctx_generation\":2}"},
{"raise_if_stale_attempt", "{\"state\":{\"_compression_working_attempt_generation\":5},\"ctx_generation\":5}"},
{"raise_if_stale_attempt", "{\"state\":{\"_compression_working_attempt_generation\":5},\"ctx_generation\":2}"},
{"install_compression_cancelled_check", "{\"state\":{},\"check\":\"cb1\",\"generation\":3,\"frozen\":false}"},
{"install_compression_cancelled_check", "{\"state\":{},\"check\":\"cb1\",\"generation\":3,\"frozen\":true}"},
{"clear_compression_cancelled_check_if_owner", "{\"state\":{\"_compression_cancelled_check\":\"cb\",\"_compression_cancelled_check_owner\":3},\"generation\":3}"},
{"clear_compression_cancelled_check_if_owner", "{\"state\":{\"_compression_cancelled_check\":\"cb\",\"_compression_cancelled_check_owner\":4},\"generation\":3}"},
{"clear_compression_cancelled_check_if_owner", "{\"state\":{\"_compression_cancelled_check\":\"cb\",\"_compression_cancelled_check_owner\":4},\"generation\":0}"},
{"clear_compression_cancelled_check_if_owner", "{\"state\":{},\"generation\":3}"},
{"get_context_compression_timeout_state", "{\"agent\":{},\"create\":true}"},
{"get_context_compression_timeout_state", "{\"agent\":{},\"create\":false}"},
{"get_context_compression_timeout_state", "{\"agent\":{},\"create\":true,\"no_vars\":true}"},
{"get_context_compression_timeout_state", "{\"agent\":{\"_context_compression_timeout_state\":{\"__local__\":true,\"timed_out\":true}},\"create\":false}"},
{"set_context_compression_timeout_outcome", "{\"agent\":{},\"timed_out\":true}"},
{"set_context_compression_timeout_outcome", "{\"agent\":{\"_context_compression_timeout_state\":{\"__local__\":true}},\"timed_out\":false}"},
{"set_context_compression_timeout_outcome", "{\"agent\":{},\"timed_out\":true,\"no_vars\":true}"},
{"reset_context_compression_timeout_outcome", "{\"agent\":{\"_context_compression_timeout_state\":{\"__local__\":true,\"timed_out\":true},\"_last_compression_timed_out\":true}}"},
{"mark_context_compression_timed_out", "{\"agent\":{}}"},
{"mark_compression_blocked_transient", "{\"agent\":{},\"reason\":\"cooldown:summary_failure\"}"},
{"mark_compression_blocked_transient", "{\"agent\":{},\"reason\":\"structural_backoff:no_progress\"}"},
{"mark_compression_blocked_transient", "{\"agent\":{},\"reason\":\"ineffective\"}"},
{"mark_compression_blocked_transient", "{\"agent\":{},\"reason\":\"cooldown:x\",\"frozen\":true}"},
/* ---- small flow cores ---- */
{"swallow", "{\"message\":\"best effort\",\"raises\":false,\"exc_info\":false}"},
{"swallow", "{\"message\":\"best effort\",\"raises\":true,\"exc_info\":false}"},
{"swallow", "{\"message\":\"best effort\",\"raises\":true,\"exc_info\":true}"},
{"emit_feasibility_notice", "{\"agent\":{},\"msg\":\"warn one\"}"},
{"emit_feasibility_notice", "{\"agent\":{\"_last_feasibility_notice\":\"warn one\"},\"msg\":\"warn one\"}"},
{"emit_feasibility_notice", "{\"agent\":{\"_last_feasibility_notice\":\"old\"},\"msg\":\"new\"}"},
{"warn_summary_or_aux_fallback", "{\"agent\":{},\"compressor\":{\"_last_summary_error\":\"boom\"}}"},
{"warn_summary_or_aux_fallback", "{\"agent\":{\"_last_compression_summary_warning\":\"boom\"},\"compressor\":{\"_last_summary_error\":\"boom\"}}"},
{"warn_summary_or_aux_fallback", "{\"agent\":{},\"compressor\":{\"_last_aux_model_failure_model\":\"aux-m\",\"_last_aux_model_failure_error\":\"401\"}}"},
{"warn_summary_or_aux_fallback", "{\"agent\":{\"_last_aux_fallback_warning_key\":[\"aux-m\",\"401\"]},\"compressor\":{\"_last_aux_model_failure_model\":\"aux-m\",\"_last_aux_model_failure_error\":\"401\"}}"},
{"warn_summary_or_aux_fallback", "{\"agent\":{},\"compressor\":{}}"},
{"refresh_main_credential", "{\"api_mode\":\"anthropic_messages\",\"refresh_present\":true,\"refresh_raises\":false}"},
{"refresh_main_credential", "{\"api_mode\":\"chat_completions\",\"refresh_present\":true}"},
{"refresh_main_credential", "{\"api_mode\":\"anthropic_messages\",\"refresh_present\":false}"},
{"refresh_main_credential", "{\"api_mode\":\"anthropic_messages\",\"refresh_present\":true,\"refresh_raises\":true}"},
{"hand_off_metrics_segment", "{\"old\":\"s1\",\"new\":\"s2\",\"raises\":false}"},
{"hand_off_metrics_segment", "{\"old\":\"s1\",\"new\":\"s2\",\"raises\":true}"},
{"reset_read_dedup_caches", "{\"task_id\":\"t1\",\"session_id\":\"s1\"}"},
{"reset_read_dedup_caches", "{\"task_id\":\"t1\",\"session_id\":\"\"}"},
{"reset_read_dedup_caches", "{\"task_id\":\"t1\",\"session_id\":\"s1\",\"file_raises\":true,\"skill_raises\":true}"},
{"rebind_session_context", "{\"session_id\":\"s2\",\"set_raises\":false,\"logging_raises\":false}"},
{"rebind_session_context", "{\"session_id\":\"s2\",\"set_raises\":true,\"logging_raises\":false}"},
{"rebind_session_context", "{\"session_id\":\"s2\",\"set_raises\":false,\"logging_raises\":true}"},
{"aux_inherits_main_route", "{\"agent_model\":\"Main\",\"aux_model\":\"other\",\"agent_base_url\":\"http://a\",\"aux_base_url\":\"http://a\",\"normalize_map\":{\"http://a\":\"a\"}}"},
{"aux_inherits_main_route", "{\"agent_model\":\"main\",\"aux_model\":\" MAIN \",\"agent_base_url\":\"\",\"aux_base_url\":\"http://x\",\"normalize_map\":{\"\":\"\",\"http://x\":\"x\"}}"},
{"aux_inherits_main_route", "{\"agent_model\":\"main\",\"aux_model\":\"main\",\"agent_base_url\":\"http://a/v1\",\"aux_base_url\":\"http://a/v1/\",\"normalize_map\":{\"http://a/v1\":\"a/v1\",\"http://a/v1/\":\"a/v1\"}}"},
{"aux_inherits_main_route", "{\"agent_model\":\"main\",\"aux_model\":\"main\",\"agent_base_url\":\"http://a\",\"aux_base_url\":\"http://b\",\"normalize_map\":{\"http://a\":\"a\",\"http://b\":\"b\"}}"},
{"compression_child_source", "{\"store_present\":true,\"record_source\":\"tool\",\"platform\":\"cli\",\"source_for\":\"cli\"}"},
{"compression_child_source", "{\"store_present\":false,\"db_source\":\"kanban\",\"platform\":\"cli\",\"source_for\":\"cli\"}"},
{"compression_child_source", "{\"store_present\":true,\"record_source\":null,\"platform\":\"telegram\",\"source_for\":\"telegram\"}"},
{"compression_child_source", "{\"store_present\":false,\"read_raises\":true,\"platform\":\"cli\",\"source_for\":\"cli\"}"},
{"held_watermark", "{\"watermark\":10,\"messages\":[1,2,3],\"verbatim_tail\":null,\"seam_result\":7}"},
{"held_watermark", "{\"watermark\":null,\"messages\":[],\"verbatim_tail\":[{\"role\":\"user\"}],\"seam_result\":null}"},
/* ---- breaker gate / attempt entry / announce ---- */
{"automatic_compression_gate_blocks", "{\"agent\":{},\"bypass_cooldown\":false,\"include_cooldown\":true,\"guards\":{\"get_active_compression_failure_cooldown\":{\"present\":true,\"raises\":false},\"_load_fallback_compression_streak\":{\"present\":true,\"raises\":true},\"_load_ineffective_compression_count\":{\"present\":false}},\"blocked\":{\"present\":false}}"},
{"automatic_compression_gate_blocks", "{\"agent\":{},\"bypass_cooldown\":false,\"guards\":{},\"blocked\":{\"present\":true,\"result\":false,\"accepts_ignore\":true}}"},
{"automatic_compression_gate_blocks", "{\"agent\":{},\"bypass_cooldown\":true,\"guards\":{},\"blocked\":{\"present\":true,\"result\":true,\"accepts_ignore\":true},\"reason_fn\":{\"present\":true,\"accepts_ignore\":true,\"result\":\"cooldown:summary_failure\",\"raises\":false}}"},
{"automatic_compression_gate_blocks", "{\"agent\":{},\"bypass_cooldown\":true,\"guards\":{},\"blocked\":{\"present\":true,\"result\":true,\"accepts_ignore\":false},\"reason_fn\":{\"present\":true,\"accepts_ignore\":true,\"result\":\"ineffective\",\"raises\":false}}"},
{"automatic_compression_gate_blocks", "{\"agent\":{},\"bypass_cooldown\":false,\"guards\":{},\"blocked\":{\"present\":true,\"result\":true,\"accepts_ignore\":false},\"reason_fn\":{\"present\":true,\"accepts_ignore\":false,\"raises\":true}}"},
{"automatic_compression_gate_blocks", "{\"agent\":{},\"bypass_cooldown\":false,\"include_cooldown\":false,\"guards\":{\"get_active_compression_failure_cooldown\":{\"present\":true},\"_load_fallback_compression_streak\":{\"present\":true}},\"blocked\":{\"present\":true,\"result\":true,\"accepts_ignore\":false},\"reason_fn\":{\"present\":false}}"},
{"begin_compression_attempt", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"force\":false,\"defer_notification\":false,\"approx_tokens\":500,\"now\":100.0,\"attempt_id\":\"a1\",\"generation\":1,\"snapshot\":{\"compression_count\":2},\"seed\":{}}"},
{"begin_compression_attempt", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{\"last_prompt_tokens\":900},\"force\":true,\"defer_notification\":false,\"approx_tokens\":null,\"now\":101.5,\"attempt_id\":\"a2\",\"generation\":4,\"snapshot\":{}}"},
{"begin_compression_attempt", "{\"agent\":{},\"compressor\":{},\"force\":false,\"trigger\":\"overflow\",\"overflow_reason\":\"context_overflow\",\"defer_notification\":false,\"now\":102.0,\"attempt_id\":\"a3\",\"generation\":1,\"snapshot\":{}}"},
{"begin_compression_attempt", "{\"agent\":{},\"compressor\":{},\"force\":false,\"defer_notification\":true,\"pending_present\":true,\"now\":103.0,\"attempt_id\":\"a4\",\"generation\":1,\"snapshot\":{}}"},
{"begin_compression_attempt", "{\"agent\":{},\"compressor\":{},\"force\":false,\"defer_notification\":false,\"now\":104.0,\"attempt_id\":\"a5\",\"generation\":1,\"snapshot\":{},\"metrics_raises\":true}"},
{"announce_compression_start", "{\"force\":true,\"message_count\":10,\"approx_tokens\":5000,\"focus_topic\":null,\"model\":\"m\",\"session_id\":\"s1\"}"},
{"announce_compression_start", "{\"force\":false,\"auto_status\":\"custom compacting line\",\"message_count\":10,\"approx_tokens\":5000,\"model\":\"m\"}"},
{"announce_compression_start", "{\"force\":false,\"auto_status\":null,\"message_count\":0,\"approx_tokens\":null,\"model\":\"m\"}"},
{"announce_compression_start", "{\"force\":false,\"auto_status\":\"\",\"message_count\":3,\"model\":\"m\"}"},
/* ---- lock API / lease flows ---- */
{"resolve_lock_api", "{\"db_present\":false,\"store_present\":false}"},
{"resolve_lock_api", "{\"db_present\":true,\"store_present\":true}"},
{"resolve_lock_api", "{\"db_present\":true,\"store_present\":false,\"lookup_raises\":true}"},
{"resolve_lock_api", "{\"db_present\":true,\"store_present\":false,\"not_callable\":true}"},
{"resolve_lock_api", "{\"db_present\":true,\"store_present\":false}"},
{"release_compression_lock", "{\"store_present\":true,\"sid\":\"s1\",\"holder\":\"h1\"}"},
{"release_compression_lock", "{\"store_present\":false,\"sid\":\"s1\",\"holder\":\"h1\",\"raises\":true}"},
{"try_acquire_durable_lock", "{\"sid\":\"s1\",\"holder\":\"h1\",\"ttl\":300,\"fence_present\":true,\"acquire\":{\"result\":true},\"watermark\":{\"result\":42},\"store_present\":true}"},
{"try_acquire_durable_lock", "{\"sid\":\"s1\",\"holder\":\"h1\",\"ttl\":300,\"fence_present\":true,\"acquire\":{\"result\":true},\"watermark\":{\"raises\":true},\"store_present\":false}"},
{"try_acquire_durable_lock", "{\"sid\":\"s1\",\"holder\":\"h1\",\"ttl\":300,\"fence_present\":false,\"acquire\":{\"raises\":true},\"release_raises\":false,\"store_present\":false,\"watermark_before\":null}"},
{"try_acquire_durable_lock", "{\"sid\":\"s1\",\"holder\":\"h1\",\"ttl\":300,\"fence_present\":true,\"acquire\":{\"result\":false},\"watermark_before\":null}"},
{"try_acquire_durable_lock", "{\"sid\":\"s1\",\"holder\":\"h1\",\"ttl\":300,\"fence_present\":true,\"fence_has_method\":false,\"acquire\":{\"result\":true},\"watermark\":{\"result\":5}}"},
{"abort_lease", "{\"prompt\":\"given\",\"started_at\":10.0,\"failure_class\":\"lock_contended\",\"seed\":{\"attempt_id\":\"a\"},\"lifecycle\":{\"status_emitted\":true,\"commit_status\":\"aborted\",\"done_emitted\":false},\"force_terminal\":true,\"status_callback_present\":true}"},
{"abort_lease", "{\"prompt\":null,\"cached\":\"\",\"build_result\":\"built\",\"started_at\":10.0,\"failure_class\":\"commit_fence_cancelled\",\"seed\":{},\"lifecycle\":{\"status_emitted\":false,\"commit_status\":\"aborted\",\"done_emitted\":false},\"force_terminal\":true,\"status_callback_present\":true}"},
{"abort_lease", "{\"prompt\":\"p\",\"started_at\":10.0,\"failure_class\":\"x\",\"seed\":{},\"lifecycle\":{\"status_emitted\":true,\"commit_status\":\"aborted\",\"done_emitted\":true},\"force_terminal\":true,\"status_callback_present\":true}"},
{"abort_lease", "{\"prompt\":\"p\",\"started_at\":10.0,\"failure_class\":\"x\",\"seed\":{},\"lifecycle\":{\"status_emitted\":true,\"commit_status\":\"committed\",\"done_emitted\":false},\"force_terminal\":false,\"status_callback_present\":false}"},
{"sit_out_lock_contention", "{\"agent\":{},\"sid\":\"s1\",\"store_present\":true,\"holder_result\":\"other-holder\",\"started_at\":10.0,\"seed\":{},\"cached\":\"P\",\"build_result\":\"B\"}"},
{"sit_out_lock_contention", "{\"agent\":{\"_last_compression_lock_warning_sid\":\"s1\"},\"sid\":\"s1\",\"store_present\":false,\"holder_result\":null,\"started_at\":10.0,\"seed\":{},\"cached\":null,\"build_result\":\"B\"}"},
{"sit_out_lock_contention", "{\"agent\":{},\"sid\":\"s2\",\"store_present\":false,\"holder_raises\":true,\"started_at\":10.0,\"seed\":{},\"cached\":\"P\",\"emit_raises\":true}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":false,\"store_present\":false,\"fence_present\":false,\"lock_api\":{\"api_present\":false,\"error\":null}}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":false,\"fence_present\":false,\"lock_api\":{\"api_present\":false,\"error\":null},\"holder_id\":\"h1\"}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\",\"_last_compression_lock_error_sid\":\"s1\"},\"db_present\":true,\"store_present\":false,\"fence_present\":false,\"lock_api\":{\"api_present\":false,\"error\":null},\"holder_id\":\"h1\"}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":false,\"fence_present\":true,\"lock_api\":{\"api_present\":true,\"error\":\"lookup\"},\"holder_id\":\"h1\",\"sit_out\":{\"prompt\":\"SP\"}}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":true,\"fence_present\":true,\"lock_api\":{\"api_present\":true,\"error\":null},\"holder_id\":\"h1\",\"begin_lock_setup\":false,\"abort\":{\"prompt\":\"AP\"}}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":true,\"fence_present\":true,\"lock_api\":{\"api_present\":true,\"error\":null},\"holder_id\":\"h1\",\"begin_lock_setup\":true,\"acquire\":{\"acquired\":false,\"watermark\":null,\"holder_cleared\":false},\"sit_out\":{\"prompt\":\"SP\"}}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":true,\"fence_present\":true,\"lock_api\":{\"api_present\":true,\"error\":null},\"holder_id\":\"h1\",\"begin_lock_setup\":true,\"acquire\":{\"acquired\":true,\"watermark\":42,\"holder_cleared\":false},\"register_requested\":true,\"cached\":\"\",\"build_result\":\"B\",\"started_at\":10.0,\"seed\":{}}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":true,\"fence_present\":true,\"lock_api\":{\"api_present\":true,\"error\":null},\"holder_id\":\"h1\",\"begin_lock_setup\":true,\"acquire\":{\"acquired\":true,\"watermark\":42,\"holder_cleared\":false},\"register_requested\":false,\"ttl_raw\":120,\"ttl_kind\":\"int\"}"},
{"acquire_compression_lease", "{\"agent\":{\"session_id\":\"s1\"},\"db_present\":true,\"store_present\":true,\"fence_present\":false,\"lock_api\":{\"api_present\":true,\"error\":null},\"holder_id\":\"h1\",\"begin_lock_setup\":true,\"acquire\":{\"acquired\":true,\"watermark\":null,\"holder_cleared\":true}}"},
/* ---- adoption + summary prep ---- */
{"adopt_if_parent_rotated", "{\"db_present\":false,\"sid\":\"s1\",\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"cached\":\"P\",\"build_result\":\"B\"}"},
{"adopt_if_parent_rotated", "{\"db_present\":true,\"sid\":\"s1\",\"rotated\":{\"result\":false},\"messages\":[],\"cached\":\"P\"}"},
{"adopt_if_parent_rotated", "{\"db_present\":true,\"sid\":\"s1\",\"rotated\":{\"result\":true},\"recovered\":[{\"role\":\"user\",\"content\":\"child\"}],\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"cached\":\"\",\"build_result\":\"B\",\"approx_tokens\":500,\"seed\":{\"attempt_id\":\"a\"}}"},
{"adopt_if_parent_rotated", "{\"db_present\":true,\"sid\":\"s1\",\"rotated\":{\"result\":true},\"recovered\":null,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"cached\":\"P\",\"approx_tokens\":null,\"seed\":{}}"},
{"adopt_if_parent_rotated", "{\"db_present\":true,\"sid\":\"s1\",\"rotated\":{\"raises\":true},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"cached\":\"P\",\"approx_tokens\":100,\"seed\":{}}"},
{"adopt_grown_durable_parent", "{\"db_present\":false,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":true}"},
{"adopt_grown_durable_parent", "{\"db_present\":true,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":false,\"loader_present\":false}"},
{"adopt_grown_durable_parent", "{\"db_present\":true,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":true,\"durable1\":[1,2],\"watermark\":null}"},
{"adopt_grown_durable_parent", "{\"db_present\":true,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":true,\"durable1\":[1,2,3],\"durable2\":[{\"role\":\"user\",\"_row_id\":11},{\"role\":\"assistant\",\"_row_id\":12},{\"role\":\"user\",\"_row_id\":13}],\"preflush_idx\":null,\"watermark\":10}"},
{"adopt_grown_durable_parent", "{\"db_present\":true,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":true,\"durable1\":[1,2,3],\"preflush_idx\":1,\"flush\":{\"raises\":true},\"watermark\":10}"},
{"adopt_grown_durable_parent", "{\"db_present\":true,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":true,\"durable1\":[1,2,3],\"preflush_idx\":1,\"flush\":{\"result\":true},\"durable2\":[{\"role\":\"user\",\"_row_id\":11},{\"role\":\"assistant\",\"_row_id\":12},{\"role\":\"user\",\"_row_id\":13}],\"watermark\":10}"},
{"adopt_grown_durable_parent", "{\"db_present\":true,\"sid\":\"s1\",\"messages\":[1,2],\"store_present\":true,\"durable1\":[1,2,3],\"preflush_idx\":null,\"durable2\":[{\"role\":\"user\"},{\"role\":\"assistant\"}],\"watermark\":10}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":false,\"mm_present\":false,\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":false,\"mm_present\":true,\"on_pre_compress\":{\"result\":\"insights\",\"raises\":false},\"sanitize\":{\"result\":\"clean\"},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":false,\"mm_present\":true,\"on_pre_compress\":{\"raises\":true},\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":true,\"mm_present\":false,\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":true,\"mm_present\":true,\"supports_present\":true,\"supports\":{\"raises\":true},\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":true,\"mm_present\":true,\"supports_present\":true,\"supports\":{\"result\":false},\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":true,\"mm_present\":true,\"supports_present\":true,\"supports\":{\"result\":true},\"on_pre_compress\":{\"result\":\"ctx\",\"raises\":false},\"sanitize\":{\"result\":\"sanitized\"},\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":true,\"mm_present\":true,\"supports_present\":true,\"supports\":{\"result\":true},\"on_pre_compress\":{\"raises\":true},\"messages\":[]}"},
{"pre_compress_memory_context", "{\"checkpoint_required\":true,\"persist_disabled\":true,\"mm_present\":false,\"db_present\":false,\"messages\":[]}"},
{"resolve_compress_call", "{\"agent\":{},\"kwargs\":{\"current_tokens\":500,\"force\":false},\"memory_context\":\"\",\"engine_name\":\"ContextCompressor\"}"},
{"resolve_compress_call", "{\"agent\":{},\"kwargs\":{\"current_tokens\":500},\"memory_context\":\"insights\",\"engine_name\":\"PluginEngine\"}"},
{"resolve_compress_call", "{\"agent\":{\"_last_memory_context_unsupported_engine\":\"PluginEngine\"},\"kwargs\":{\"current_tokens\":500},\"memory_context\":\"insights\",\"engine_name\":\"PluginEngine\"}"},
{"resolve_compress_call", "{\"agent\":{},\"kwargs\":{\"current_tokens\":500,\"memory_context\":\"insights\"},\"memory_context\":\"insights\",\"engine_name\":\"ContextCompressor\"}"},
{"run_summary_dispatch", "{\"compressor\":{},\"fence_present\":true,\"fence_cancelled\":true,\"generation\":3,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"kwargs\":{}}"},
{"run_summary_dispatch", "{\"compressor\":{},\"fence_present\":true,\"fence_cancelled\":false,\"hard_cancel_set\":false,\"generation\":3,\"messages\":[],\"kwargs\":{\"force\":false},\"compress\":{\"result\":[{\"role\":\"user\",\"content\":\"summary\"}]}}"},
{"run_summary_dispatch", "{\"compressor\":{},\"fence_present\":true,\"fence_cancelled\":false,\"generation\":3,\"messages\":[],\"kwargs\":{},\"compress\":{\"raises\":\"AuxiliaryExplicitCancellation\"}}"},
{"run_summary_dispatch", "{\"compressor\":{},\"fence_present\":true,\"fence_cancelled\":false,\"hard_cancel_set\":true,\"generation\":3,\"messages\":[],\"kwargs\":{},\"compress\":{\"result\":[]}}"},
{"run_summary_dispatch", "{\"compressor\":{},\"fence_present\":false,\"generation\":0,\"messages\":[],\"kwargs\":{},\"compress\":{\"result\":[1]}}"},
{"run_summary_dispatch", "{\"compressor\":{},\"fence_present\":true,\"fence_cancelled\":false,\"generation\":3,\"messages\":[],\"kwargs\":{},\"compress\":{\"result\":[]},\"frozen\":true}"},
{"run_summary_phase", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"in_place\":true,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":true,\"seed\":{\"attempt_id\":\"a\"},\"memory\":{\"memory_context\":\"\",\"raised\":null},\"resolve\":{\"kwargs\":{}},\"dispatch\":{\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"raises\":null},\"fence_present\":true,\"fence_cancelled\":false,\"hard_cancel_set\":false}"},
{"run_summary_phase", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"in_place\":false,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":false,\"seed\":{},\"adopt\":{\"result\":[{\"role\":\"user\",\"content\":\"a\"},{\"role\":\"assistant\",\"content\":\"b\"}]},\"memory\":{\"memory_context\":\"\",\"raised\":null},\"resolve\":{\"kwargs\":{}},\"dispatch\":{\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"raises\":null}}"},
{"run_summary_phase", "{\"agent\":{},\"compressor\":{},\"in_place\":true,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":true,\"seed\":{},\"memory\":{\"memory_context\":\"\",\"raised\":null},\"resolve\":{\"kwargs\":{}},\"dispatch\":{\"raises\":\"AuxiliaryExplicitCancellation\"},\"restore\":{\"raises\":null},\"stall_backoff\":false,\"fence_present\":true,\"fence_cancelled\":false,\"hard_cancel_set\":true,\"cached\":\"P\",\"build_result\":\"B\"}"},
{"run_summary_phase", "{\"agent\":{},\"compressor\":{},\"in_place\":true,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":true,\"seed\":{},\"memory\":{\"memory_context\":\"\",\"raised\":null},\"resolve\":{\"kwargs\":{}},\"dispatch\":{\"raises\":\"AuxiliaryExplicitCancellation\"},\"restore\":{\"raises\":null},\"stall_backoff\":true,\"fence_present\":true,\"fence_cancelled\":true,\"hard_cancel_set\":false,\"cached\":\"\",\"build_result\":\"B\"}"},
{"run_summary_phase", "{\"agent\":{},\"compressor\":{},\"in_place\":true,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":true,\"seed\":{},\"memory\":{\"memory_context\":\"\",\"raised\":null},\"resolve\":{\"kwargs\":{}},\"dispatch\":{\"raises\":\"AuxiliaryExplicitCancellation\"},\"restore\":{\"raises\":\"RuntimeError\"},\"cached\":\"P\"}"},
{"run_summary_phase", "{\"agent\":{},\"compressor\":{},\"in_place\":true,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":true,\"seed\":{},\"memory\":{\"memory_context\":\"\",\"raised\":null},\"resolve\":{\"kwargs\":{}},\"dispatch\":{\"raises\":\"ValueError\"},\"cached\":\"P\"}"},
{"run_summary_phase", "{\"agent\":{},\"compressor\":{},\"in_place\":true,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"approx_tokens\":500,\"status_emitted\":false,\"seed\":{},\"memory\":{\"memory_context\":\"\",\"raised\":\"CompressionCheckpointUnavailable\"},\"cached\":\"P\"}"},
{"candidate_rejected", "{\"agent\":{},\"compressor\":{\"_last_compress_aborted\":true,\"_last_summary_error\":\"boom\"},\"compressed\":[{\"role\":\"user\",\"content\":\"x\"}],\"before\":[{\"role\":\"user\",\"content\":\"a\"}],\"generation\":1,\"seed\":{}}"},
{"candidate_rejected", "{\"agent\":{\"_last_compression_summary_warning\":\"unknown error\"},\"compressor\":{\"_last_compress_aborted\":true,\"_last_summary_error\":null},\"compressed\":[1],\"before\":[2],\"generation\":1,\"seed\":{}}"},
{"candidate_rejected", "{\"agent\":{},\"compressor\":{},\"compressed\":[{\"role\":\"user\",\"content\":\"a\"}],\"before\":[{\"role\":\"user\",\"content\":\"a\"}],\"generation\":1,\"seed\":{},\"structural_recorder\":{\"present\":true,\"raises\":false}}"},
{"candidate_rejected", "{\"agent\":{},\"compressor\":{},\"compressed\":[{\"role\":\"user\",\"content\":\"a\"}],\"before\":[{\"role\":\"user\",\"content\":\"a\",\"_db_persisted\":true}],\"generation\":1,\"seed\":{},\"structural_recorder\":{\"present\":false}}"},
{"candidate_rejected", "{\"agent\":{},\"compressor\":{},\"compressed\":[],\"before\":[{\"role\":\"user\",\"content\":\"a\"}],\"generation\":1,\"seed\":{}}"},
{"candidate_rejected", "{\"agent\":{},\"compressor\":{\"_compression_working_attempt_generation\":5},\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"before\":[{\"role\":\"user\",\"content\":\"a\"}],\"generation\":3,\"seed\":{}}"},
{"candidate_rejected", "{\"agent\":{},\"compressor\":{\"_compression_working_attempt_generation\":3},\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"before\":[{\"role\":\"user\",\"content\":\"a\"}],\"generation\":3,\"seed\":{}}"},
{"salvage_or_refuse_grown_transcript", "{\"compressor\":{},\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"rough_in\":1000,\"rough_out\":500,\"cached\":\"P\",\"seed\":{},\"snapshot\":{}}"},
{"salvage_or_refuse_grown_transcript", "{\"compressor\":{},\"compressed\":[{\"role\":\"user\",\"content\":\"big\"}],\"rough_in\":1000,\"rough_out\":1500,\"salvage\":{\"result\":[{\"role\":\"user\",\"content\":\"small\"}]},\"salv_est\":800,\"cached\":\"P\",\"seed\":{},\"snapshot\":{}}"},
{"salvage_or_refuse_grown_transcript", "{\"compressor\":{},\"compressed\":[{\"role\":\"user\",\"content\":\"big\"}],\"rough_in\":1000,\"rough_out\":1500,\"salvage\":{\"result\":null},\"cached\":\"\",\"build_result\":\"B\",\"seed\":{},\"snapshot\":{\"_proactive_prune_rearm_tokens\":7},\"record_rejected\":{\"present\":true,\"raises\":false}}"},
{"salvage_or_refuse_grown_transcript", "{\"compressor\":{},\"compressed\":[{\"role\":\"user\",\"content\":\"big\"}],\"rough_in\":1000,\"rough_out\":1500,\"salvage\":{\"result\":[{\"role\":\"user\",\"content\":\"still big\"}]},\"salv_est\":1200,\"cached\":\"P\",\"seed\":{},\"snapshot\":{},\"record_rejected\":{\"present\":false}}"},
/* ---- todo fold / prompt rebuild / commit / boundary ---- */
{"fold_todo_snapshot", "{\"compressed\":[{\"role\":\"user\",\"content\":\"real task\"}],\"todo_snapshot\":\"[Your active task list was preserved across context compression]\\n- [ ] x\",\"has_items\":{\"present\":true,\"result\":true},\"reload_notice\":\"\",\"tail_is_real\":true}"},
{"fold_todo_snapshot", "{\"compressed\":[{\"role\":\"user\",\"content\":\"old [Your active task list was preserved across context compression]\\n- [ ] old\",\"_todo_snapshot_synthetic\":true}],\"todo_snapshot\":\"fresh\",\"has_items\":{\"present\":true,\"result\":true},\"reload_notice\":\"\",\"tail_is_real\":false}"},
{"fold_todo_snapshot", "{\"compressed\":[{\"role\":\"assistant\",\"content\":\"a\"},{\"role\":\"user\",\"content\":\"keep [Your active task list was preserved across context compression]\\nold\",\"_todo_snapshot_synthetic\":true}],\"todo_snapshot\":\"fresh\",\"has_items\":{\"present\":true,\"result\":true},\"reload_notice\":\"\",\"tail_is_real\":false}"},
{"fold_todo_snapshot", "{\"compressed\":[{\"role\":\"user\",\"content\":\"tail\"}],\"todo_snapshot\":\"fresh\",\"has_items\":{\"present\":false},\"reload_notice\":\"RELOAD\",\"tail_is_real\":true}"},
{"fold_todo_snapshot", "{\"compressed\":[{\"role\":\"user\",\"content\":\"[Your active task list was preserved across context compression]\\nold\"}],\"todo_snapshot\":\"fresh\",\"has_items\":{\"present\":true,\"result\":false},\"reload_notice\":\"\",\"tail_is_real\":false}"},
{"fold_todo_snapshot", "{\"compressed\":[],\"todo_snapshot\":\"\",\"has_items\":{\"present\":true,\"result\":true,\"raises\":true},\"reload_notice\":\"\"}"},
{"rebuild_system_prompt_at_boundary", "{\"agent\":{\"_cached_system_prompt\":\"seeded\",\"_retain_seeded_system_prompt\":true},\"build_result\":\"new\"}"},
{"rebuild_system_prompt_at_boundary", "{\"agent\":{\"_cached_system_prompt\":\"same\"},\"build_result\":\"same\",\"refresh_raises\":false}"},
{"rebuild_system_prompt_at_boundary", "{\"agent\":{\"_cached_system_prompt\":\"old\"},\"build_result\":\"new\",\"refresh_raises\":true}"},
{"rebuild_system_prompt_at_boundary", "{\"agent\":{},\"build_result\":\"new\",\"refresh_raises\":false}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"in_place\":true,\"session_db_present\":false,\"now\":50.0,\"made_progress\":true,\"messages_before\":null,\"seed\":{},\"snapshot\":{}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"big\"}],\"in_place\":true,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\"}],\"salvage\":{\"refused\":true,\"prompt\":\"RP\"},\"seed\":{},\"snapshot\":{}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{\"_last_compression_telemetry\":{\"x\":1}},\"messages\":[{\"role\":\"user\",\"content\":\"a\"},{\"role\":\"assistant\",\"content\":\"b\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"},{\"role\":\"assistant\",\"content\":\"t\",\"_compaction_tail\":true}],\"in_place\":true,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\"},{\"role\":\"assistant\",\"content\":\"b\"}],\"salvage\":{\"refused\":false},\"memory_call\":{\"raises\":false},\"absorbed_roles\":{},\"coverage\":{\"covered_ids\":[1],\"unresolved_held\":[]},\"held_watermark\":{\"result\":9},\"archive_commit\":{\"raises\":false},\"lease\":{\"holder\":\"h1\",\"watermark\":9},\"new_system_prompt\":\"NP\",\"seed\":{},\"snapshot\":{}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"in_place\":true,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\",\"_db_persisted\":true}],\"salvage\":{\"refused\":false},\"memory_call\":{\"raises\":false},\"absorbed_roles\":{},\"coverage\":{\"covered_ids\":[],\"unresolved_held\":[]},\"held_watermark\":{\"result\":null},\"archive_commit\":{\"raises\":true},\"lease\":{\"holder\":\"h1\"},\"error_msg\":\"db locked\",\"split_cooldown\":{\"present\":true,\"raises\":false},\"seed\":{},\"snapshot\":{\"_proactive_prune_rearm_tokens\":3}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"in_place\":false,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\"}],\"salvage\":{\"refused\":false},\"memory_call\":{\"raises\":false},\"publish\":{\"raises\":false,\"new_session_id\":\"s2\"},\"lease\":{\"holder\":\"h1\",\"watermark\":5},\"new_system_prompt\":\"NP\",\"seed\":{},\"snapshot\":{}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"in_place\":false,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\",\"_db_persisted\":true}],\"salvage\":{\"refused\":false},\"memory_call\":{\"raises\":false},\"publish\":{\"raises\":true},\"error_msg\":\"lease lost\",\"split_cooldown\":{\"present\":true,\"raises\":true},\"seed\":{},\"snapshot\":{}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\",\"_active_session_turn_lease_holder\":\"turn\",\"_persist_user_message_idx\":1},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\",\"_db_persisted\":true,\"_row_id\":1},{\"role\":\"user\",\"content\":\"fresh\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"},{\"role\":\"assistant\",\"content\":\"t\",\"_compaction_tail\":true}],\"in_place\":true,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\"}],\"salvage\":{\"refused\":false},\"memory_call\":{\"raises\":false,\"resets\":{\"_last_compression_telemetry\":null}},\"absorbed_roles\":{\"5\":\"user\",\"6\":null},\"coverage\":{\"covered_ids\":[],\"unresolved_held\":[]},\"held_watermark\":{\"result\":2},\"archive_commit\":{\"raises\":false},\"lease\":{\"holder\":\"h1\"},\"new_system_prompt\":\"NP\",\"verbatim_tail\":[{\"role\":\"user\",\"content\":\"kept\"}],\"rejoin\":{\"result\":[{\"role\":\"user\",\"content\":\"s\"},{\"role\":\"user\",\"content\":\"kept\"}]},\"seed\":{},\"snapshot\":{}}"},
{"commit_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{},\"messages\":[{\"role\":\"user\",\"content\":\"a\",\"_db_persisted\":true,\"_row_id\":1},{\"role\":\"assistant\",\"content\":\"b\",\"_db_persisted\":true,\"_row_id\":2},{\"role\":\"user\",\"content\":\"c\",\"_db_persisted\":true,\"_row_id\":3,\"_absorbed_row_ids\":[5,6]}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"},{\"role\":\"user\",\"content\":\"t\",\"_compaction_tail\":true}],\"in_place\":true,\"session_db_present\":true,\"now\":50.0,\"made_progress\":true,\"messages_before\":[{\"role\":\"user\",\"content\":\"a\"}],\"salvage\":{\"refused\":false},\"memory_call\":{\"raises\":false},\"absorbed_roles\":{\"5\":\"user\",\"6\":null},\"coverage\":{\"covered_ids\":[],\"unresolved_held\":[]},\"held_watermark\":{\"result\":3},\"archive_commit\":{\"raises\":false},\"lease\":{\"holder\":\"h1\"},\"new_system_prompt\":\"NP\",\"seed\":{},\"snapshot\":{}}"},
{"finish_compaction_boundary", "{\"agent\":{\"session_id\":\"s2\",\"platform\":\"cli\",\"log_prefix\":\"\"},\"compressor\":{\"compression_count\":1},\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"new_system_prompt\":\"NP\",\"old_session_id\":\"s1\",\"in_place\":false,\"compacted_in_place\":false,\"session_commit_succeeded\":true,\"defer_notification\":false,\"compression_made_progress\":true,\"compression_used_fallback\":false,\"compression_feasibility_skip\":false,\"task_id\":\"t1\",\"clear_labels\":{\"via\":\"store\",\"raises\":false},\"mm_present\":true,\"estimate_request\":400,\"record_boundary_present\":true,\"event_callback_present\":false}"},
{"finish_compaction_boundary", "{\"agent\":{\"session_id\":\"s1\",\"platform\":\"\",\"log_prefix\":\"[p] \"},\"compressor\":{\"compression_count\":3},\"compressed\":[],\"new_system_prompt\":\"\",\"old_session_id\":null,\"in_place\":true,\"compacted_in_place\":true,\"session_commit_succeeded\":true,\"defer_notification\":true,\"compression_made_progress\":false,\"task_id\":\"t1\",\"mm_present\":false,\"estimate_request\":100,\"record_boundary_present\":false,\"event_callback_present\":true}"},
{"finish_compaction_boundary", "{\"agent\":{\"session_id\":\"s1\"},\"compressor\":{\"compression_count\":0},\"compressed\":[],\"new_system_prompt\":\"NP\",\"old_session_id\":null,\"in_place\":false,\"compacted_in_place\":false,\"session_commit_succeeded\":false,\"defer_notification\":false,\"compression_made_progress\":true,\"task_id\":\"t1\",\"mm_present\":false,\"estimate_request\":50,\"record_boundary_present\":true,\"event_callback_present\":false}"},
{"publish_rotated_compaction", "{\"agent\":{\"session_id\":\"s1\",\"_persist_user_message_idx\":1},\"messages\":[{\"role\":\"user\",\"content\":\"a\"},{\"role\":\"user\",\"content\":\"real\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"old_session_id\":\"s1\",\"new_system_prompt\":\"NP\",\"lease\":{\"holder\":\"h1\",\"watermark\":5,\"ttl\":300},\"compressed_user_turn_outcome\":\"inserted\",\"store_present\":true,\"parent_ended\":false,\"foreign_ceiling\":{\"result\":20},\"flush\":{\"raises\":false},\"profile\":{\"result\":\"default\"},\"title\":\"My session\",\"mint\":\"s2\",\"child_source\":\"cli\",\"yolo\":{\"enabled\":true},\"publish\":{\"raises\":false},\"real_user_flags\":[false,true],\"session_messages_separate\":true,\"session_messages\":[{\"role\":\"user\",\"content\":\"real\",\"timestamp\":7},{\"role\":\"user\",\"content\":\"real\"}]}"},
{"publish_rotated_compaction", "{\"agent\":{\"session_id\":\"s1\"},\"messages\":[],\"compressed\":[],\"old_session_id\":\"s1\",\"new_system_prompt\":\"NP\",\"lease\":{\"holder\":null,\"watermark\":null,\"ttl\":300},\"compressed_user_turn_outcome\":\"already_present\",\"store_present\":false,\"parent_ended\":true,\"foreign_ceiling\":{\"result\":null},\"flush\":{\"raises\":false},\"profile\":{\"result\":null},\"title\":null,\"mint\":\"s2\",\"child_source\":\"cli\",\"yolo\":null,\"publish\":{\"raises\":false}}"},
{"publish_rotated_compaction", "{\"agent\":{\"session_id\":\"s1\",\"_persist_user_message_idx\":99},\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"compressed\":[{\"role\":\"user\",\"content\":\"s\"}],\"old_session_id\":\"s1\",\"new_system_prompt\":\"NP\",\"lease\":{\"holder\":\"h1\",\"watermark\":5,\"ttl\":300},\"compressed_user_turn_outcome\":\"merged\",\"store_present\":true,\"parent_ended\":false,\"foreign_ceiling\":{\"raises\":true},\"flush\":{\"raises\":true},\"profile\":{\"raises\":true},\"title\":null,\"mint\":\"s2\",\"child_source\":\"tool\",\"yolo\":null,\"publish\":{\"raises\":true,\"raises_kind\":\"RuntimeError\"},\"real_user_flags\":[true],\"session_messages_separate\":false}"},
{"carry_session_state_to_child", "{\"old_session_id\":\"s1\",\"new_session_id\":\"s2\",\"old_title\":\"\",\"store_present\":true}"},
{"carry_session_state_to_child", "{\"old_session_id\":\"s1\",\"new_session_id\":\"s2\",\"old_title\":\"T\",\"store_present\":true,\"title_source\":{\"result\":\"auto\",\"raises\":false},\"set_title\":{\"raises\":false},\"set_source_raises\":false}"},
{"carry_session_state_to_child", "{\"old_session_id\":\"s1\",\"new_session_id\":\"s2\",\"old_title\":\"T\",\"store_present\":false,\"title_source\":{\"raises\":true},\"set_title\":{\"raises\":true}}"},
{"carry_session_state_to_child", "{\"old_session_id\":\"s1\",\"new_session_id\":\"s2\",\"old_title\":\"T\",\"store_present\":true,\"automation_raises\":true}"},
{"route_codex_compaction", "{\"fence_present\":true,\"begin_commit\":false,\"messages\":[{\"role\":\"user\",\"content\":\"a\"}],\"cached\":\"P\",\"build_result\":\"B\",\"approx_tokens\":500,\"seed\":{}}"},
{"route_codex_compaction", "{\"fence_present\":true,\"begin_commit\":true,\"codex\":{\"result\":{\"messages\":[{\"role\":\"user\",\"content\":\"s\"}],\"prompt\":\"NP\"}},\"messages\":[],\"cached\":\"P\"}"},
{"route_codex_compaction", "{\"fence_present\":false,\"codex\":{\"raises\":\"RuntimeError\"},\"messages\":[],\"cached\":\"P\"}"},
/* ---- stall-fallback ladder + worker waits ---- */
{"stall_retry_routes", "{\"chain_route\":{\"label\":\"fallback_chain[0](p)\",\"provider\":\"p\",\"model\":\"m\"},\"escalate_deterministic\":false}"},
{"stall_retry_routes", "{\"chain_route\":null,\"escalate_deterministic\":true}"},
{"stall_retry_routes", "{\"chain_route\":{\"label\":\"fallback_chain[1](q)\",\"provider\":\"q\",\"model\":\"m2\",\"timeout\":45},\"escalate_deterministic\":true}"},
{"retry_compression_on_fallback_chain", "{\"hard_cancel_present\":true,\"hard_cancel_set\":true,\"routes\":[],\"pinned_results\":[]}"},
{"retry_compression_on_fallback_chain", "{\"hard_cancel_present\":false,\"escalate_deterministic\":false,\"routes\":[{\"label\":\"r1\"},{\"label\":\"r2\"}],\"pinned_results\":[{\"messages\":[1],\"prompt\":\"P\"},null]}"},
{"retry_compression_on_fallback_chain", "{\"hard_cancel_present\":false,\"escalate_deterministic\":true,\"routes\":[{\"label\":\"r1\"},{\"label\":\"det\"}],\"pinned_results\":[null,{\"messages\":[2],\"prompt\":\"Q\"}]}"},
{"retry_compression_on_fallback_chain", "{\"hard_cancel_present\":false,\"routes\":[{\"label\":\"r1\"}],\"pinned_results\":[null]}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"r1\",\"model\":\"m\"},\"now\":100,\"precommit_deadline\":90,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":false},\"streak_before\":0,\"streak_after\":0}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"det\",\"deterministic\":true},\"now\":100,\"precommit_deadline\":90,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":true,\"valid\":true},\"run\":{\"result\":{\"messages\":[1],\"prompt\":\"P\"},\"same_list\":false},\"streak_before\":0,\"streak_after\":0,\"streak_before_int\":true,\"streak_after_int\":true}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"r1\",\"model\":\"m\",\"timeout\":45},\"now\":100,\"precommit_deadline\":100.5,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":true,\"raises\":true},\"run\":{\"result\":{\"messages\":[1],\"prompt\":\"P\"},\"same_list\":false},\"streak_before\":0,\"streak_after\":0,\"streak_before_int\":true,\"streak_after_int\":true}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"r1\",\"model\":\"m\"},\"now\":100,\"precommit_deadline\":150,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":true,\"valid\":false},\"run\":{\"raises\":true},\"streak_before\":0,\"streak_after\":0}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"r1\",\"model\":\"m\"},\"now\":100,\"precommit_deadline\":150,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":false},\"run\":{\"result\":{\"messages\":[],\"prompt\":\"\"},\"same_list\":true},\"streak_before\":0,\"streak_after\":0}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"r1\",\"model\":\"m\"},\"now\":100,\"precommit_deadline\":150,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":true,\"valid\":true},\"run\":{\"result\":{\"messages\":[1],\"prompt\":\"P\"},\"same_list\":false},\"streak_before\":1,\"streak_after\":2,\"streak_before_int\":true,\"streak_after_int\":true}"},
{"run_pinned_compression_retry", "{\"route\":{\"label\":\"r1\",\"model\":\"m\"},\"now\":149.9,\"precommit_deadline\":150,\"idle_timeout_seconds\":30,\"total_ceiling_seconds\":60,\"new_fence\":{\"present\":true,\"valid\":true},\"run\":{\"result\":{\"messages\":[1],\"prompt\":\"P\"},\"same_list\":false},\"streak_before\":0,\"streak_after\":0,\"streak_before_int\":true,\"streak_after_int\":true}"},
{"join_cancelled_worker", "{\"grace\":5,\"grace_kind\":\"int\",\"outcome\":\"ok\"}"},
{"join_cancelled_worker", "{\"grace\":5,\"grace_kind\":\"int\",\"outcome\":\"timeout\",\"done\":true}"},
{"join_cancelled_worker", "{\"grace\":5,\"grace_kind\":\"int\",\"outcome\":\"timeout\",\"done\":false}"},
{"join_cancelled_worker", "{\"grace\":-2,\"grace_kind\":\"int\",\"outcome\":\"cancelled\"}"},
{"join_cancelled_worker", "{\"grace\":\"bad\",\"grace_kind\":\"str\",\"outcome\":\"exception\"}"},
{"await_worker_within_budget", "{\"idle\":10,\"ceiling\":60,\"wait_started\":100,\"ticks\":[{\"now\":101,\"deadline\":160,\"since_progress\":1,\"outcome\":\"result\",\"result\":{\"messages\":[1]}}]}"},
{"await_worker_within_budget", "{\"idle\":10,\"ceiling\":60,\"wait_started\":100,\"ticks\":[{\"now\":161,\"deadline\":160,\"since_progress\":0,\"outcome\":\"timeout\"}]}"},
{"await_worker_within_budget", "{\"idle\":10,\"ceiling\":60,\"wait_started\":100,\"ticks\":[{\"now\":105,\"deadline\":160,\"since_progress\":2,\"outcome\":\"timeout\",\"done\":false},{\"now\":108,\"deadline\":160,\"since_progress\":1,\"outcome\":\"result\",\"result\":\"R\"}]}"},
{"await_worker_within_budget", "{\"idle\":10,\"ceiling\":60,\"wait_started\":100,\"ticks\":[{\"now\":115,\"deadline\":null,\"since_progress\":12,\"outcome\":\"timeout\",\"done\":false}]}"},
{"await_worker_within_budget", "{\"idle\":10,\"ceiling\":60,\"wait_started\":100,\"ticks\":[{\"now\":105,\"deadline\":160,\"since_progress\":2,\"outcome\":\"timeout\",\"done\":true,\"exc_none\":false}]}"},
{"await_worker_within_budget", "{\"idle\":10,\"ceiling\":60,\"wait_started\":100,\"ticks\":[{\"now\":105,\"deadline\":160,\"since_progress\":2,\"outcome\":\"timeout\",\"done\":true,\"exc_none\":true,\"result\":\"R\"}]}"},
{"await_in_flight_commit", "{\"ceiling\":60,\"wait_started\":100,\"callback_present\":true,\"ticks\":[{\"now\":110,\"outcome\":\"result\",\"result\":\"DONE\"}]}"},
{"await_in_flight_commit", "{\"ceiling\":60,\"wait_started\":100,\"callback_present\":true,\"ticks\":[{\"now\":170,\"outcome\":\"timeout\",\"done\":false},{\"now\":175,\"outcome\":\"result\",\"result\":\"DONE\"}]}"},
{"await_in_flight_commit", "{\"ceiling\":60,\"wait_started\":100,\"callback_present\":true,\"callback_raises\":true,\"ticks\":[{\"now\":170,\"outcome\":\"timeout\",\"done\":false},{\"now\":200,\"outcome\":\"timeout\",\"done\":false},{\"now\":240,\"outcome\":\"timeout\",\"done\":false},{\"now\":250,\"outcome\":\"result\",\"result\":\"D\"}]}"},
{"await_in_flight_commit", "{\"ceiling\":60,\"wait_started\":100,\"callback_present\":false,\"ticks\":[{\"now\":170,\"outcome\":\"timeout\",\"done\":true,\"result\":\"D\"}]}"},
{"cancel_or_join_worker", "{\"steps\":[{\"in_flight\":true,\"try_cancel\":null}]}"},
{"cancel_or_join_worker", "{\"steps\":[{\"in_flight\":false,\"try_cancel\":true}]}"},
{"cancel_or_join_worker", "{\"steps\":[{\"in_flight\":false,\"try_cancel\":null},{\"in_flight\":false,\"try_cancel\":false}]}"},
{"release_cancelled_worker", "{\"total_exhausted\":true,\"ceiling\":60,\"join_result\":true}"},
{"release_cancelled_worker", "{\"total_exhausted\":true,\"ceiling\":2,\"join_result\":false}"},
{"release_cancelled_worker", "{\"total_exhausted\":false,\"ceiling\":60}"},
/* ---- coverage top-ups (3+ cases per function) ---- */
{"context_engine_persistence", "{\"accepts\":true,\"persistence\":null}"},
{"mark_compressor_working_attempt", "{\"state\":{\"_compression_working_attempt_generation\":2},\"generation\":7,\"frozen\":false}"},
{"raise_if_stale_attempt", "{\"state\":{\"_compression_working_attempt_generation\":5},\"ctx_generation\":null}"},
{"install_compression_cancelled_check", "{\"state\":{\"_compression_cancelled_check\":\"old\",\"_compression_cancelled_check_owner\":1},\"check\":\"cb2\",\"generation\":2,\"frozen\":false}"},
{"reset_context_compression_timeout_outcome", "{\"agent\":{},\"no_vars\":true}"},
{"reset_context_compression_timeout_outcome", "{\"agent\":{}}"},
{"mark_context_compression_timed_out", "{\"agent\":{\"_context_compression_timeout_state\":{\"__local__\":true,\"timed_out\":false}}}"},
{"mark_context_compression_timed_out", "{\"agent\":{},\"no_vars\":true}"},
{"hand_off_metrics_segment", "{\"old\":\"\",\"new\":\"\",\"raises\":false}"},
{"held_watermark", "{\"watermark\":3,\"messages\":[1],\"verbatim_tail\":[{\"role\":\"user\"}],\"seam_result\":0}"},
{"release_compression_lock", "{\"store_present\":true,\"sid\":\"s1\",\"holder\":\"h1\",\"raises\":true}"},
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    size_t n = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < n; i++) {
        json_t *in = json_parse(cases[i].in, NULL);
        if (!in) in = json_object();
        json_t *out = dispatch(cases[i].func, in);
        if (!out) out = json_null();
        char *ins = json_serialize(in);
        char *outs = json_serialize(out);
        printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n",
               cases[i].func, ins ? ins : "{}", outs ? outs : "null");
        free(ins);
        free(outs);
        json_free(in);
        json_free(out);
    }
    return 0;
}