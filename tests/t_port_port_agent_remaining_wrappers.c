/* AUTO-GENERATED integration oracle harness for port_agent_remaining_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_agent_remaining_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int agent_model_metadata_u_endpoint_scoped_context_length(const char *);
extern int agent_model_metadata_u_skip_persistent_context_cache(const char *);
extern int agent_model_metadata_u_maybe_cache_local_context_length(const char *);
extern int agent_model_metadata_u_reconcile_local_cached_context_length(const char *);
extern int agent_model_metadata_u_localhost_to_ipv4(const char *);
extern int agent_model_metadata_u_context_cache_key(const char *);
extern int agent_model_metadata_u_query_ollama_api_show_uncached(const char *);
extern int agent_model_metadata_u_query_local_context_length_uncached(const char *);
extern int agent_model_metadata_u_codex_oauth_token_fingerprint(const char *);
extern int agent_model_metadata_u_extract_chatgpt_account_id(const char *);
extern int agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce(const char *);
extern int agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(const char *);
extern int agent_model_metadata_u_is_cjk_token_dense_char(const char *);
extern int agent_model_metadata_u_estimate_message_tokens_without_images(const char *);
extern int agent_model_metadata_u_tool_name_for_cache(const char *);
extern int agent_model_metadata_u_estimate_tools_tokens_rough(const char *);
extern int agent_pet_generate_atlas_u_has_slot_padding(const char *);
extern int agent_pet_generate_atlas_u_slot_bounds(const char *);
extern int agent_pet_generate_atlas_u_component_crops(const char *);
extern int agent_pet_generate_atlas_u_sever_expected_gutters(const char *);
extern int agent_pet_generate_atlas_u_slot_crops(const char *);
extern int agent_pet_generate_atlas_u_frame_x_ranges(const char *);
extern int agent_pet_generate_atlas_u_significant_subject_boxes(const char *);
extern int agent_pet_generate_atlas_u_validate_extracted_frames(const char *);
extern int agent_pet_generate_atlas_extract_strip_frames(const char *);
extern int agent_pet_generate_atlas_normalize_cells(const char *);
extern int agent_pet_generate_atlas_single_frame(const char *);
extern int agent_pet_generate_atlas_u_clear_transparent_rgb(const char *);
extern int agent_pet_generate_atlas_mirror_frames(const char *);
extern int agent_pet_generate_atlas_compose_atlas(const char *);
extern int agent_pet_generate_atlas_atlas_to_webp_bytes(const char *);
extern int agent_pet_generate_atlas_validate_atlas(const char *);
extern int agent_tool_dispatch_helpers_u_is_destructive_command(const char *);
extern int agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe(const char *);
extern int agent_tool_dispatch_helpers_u_plan_tool_batch_segments(const char *);
extern int agent_tool_dispatch_helpers_u_should_parallelize_tool_batch(const char *);
extern int agent_tool_dispatch_helpers_u_extract_parallel_scope_path(const char *);
extern int agent_tool_dispatch_helpers_u_paths_overlap(const char *);
extern int agent_tool_dispatch_helpers_u_is_multimodal_tool_result(const char *);
extern int agent_tool_dispatch_helpers_u_multimodal_text_summary(const char *);
extern int agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal(const char *);
extern int agent_tool_dispatch_helpers_u_extract_file_mutation_targets(const char *);
extern int agent_tool_dispatch_helpers_u_extract_error_preview(const char *);
extern int agent_tool_dispatch_helpers_u_trajectory_normalize_msg(const char *);
extern int agent_tool_dispatch_helpers_make_tool_result_message(const char *);
extern int agent_tool_dispatch_helpers_u_is_untrusted_tool(const char *);
extern int agent_tool_dispatch_helpers_u_tool_output_risk_metadata(const char *);
extern int agent_tool_dispatch_helpers_u_maybe_wrap_untrusted(const char *);
extern int agent_subscription_view_can_change_plan(const char *);
extern int agent_subscription_view_u_parse_current(const char *);
extern int agent_subscription_view_u_coalesce(const char *);
extern int agent_subscription_view_u_parse_tier(const char *);
extern int agent_subscription_view_subscription_change_preview_from_pay_ad(const char *);
extern int agent_subscription_view_subscription_state_from_payload(const char *);
extern int agent_subscription_view_build_subscription_state(const char *);
extern int agent_subscription_view_subscription_manage_url(const char *);
extern int agent_subscription_view_u_format_dollars_grouped(const char *);
extern int agent_subscription_view_selectable_tiers(const char *);
extern int agent_subscription_view_format_tier_row(const char *);
extern int agent_subscription_view_is_upgrade(const char *);
extern int agent_subscription_view_u_dev_current(const char *);
extern int agent_subscription_view_u_dev_tiers(const char *);
extern int agent_subscription_view_dev_fixture_subscription_state(const char *);
extern int agent_chat_completion_helpers_openai_codex_stale_timeout_floor(const char *);
extern int agent_chat_completion_helpers_u_provider_preferences_for_agent(const char *);
extern int agent_chat_completion_helpers_u_codex_wait_notice_recovery(const char *);
extern int agent_chat_completion_helpers_u_stale_streak(const char *);
extern int agent_chat_completion_helpers_u_bump_stale_streak(const char *);
extern int agent_chat_completion_helpers_u_reset_stale_streak(const char *);
extern int agent_chat_completion_helpers_u_check_stale_giveup(const char *);
extern int agent_chat_completion_helpers_u_derive_stream_stale_timeout(const char *);
extern int agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor(const char *);
extern int agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st(const char *);
extern int agent_chat_completion_helpers_should_use_direct_api_call(const char *);
extern int agent_chat_completion_helpers_direct_api_call(const char *);
extern int agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl(const char *);
extern int agent_chat_completion_helpers_u_build_partial_stream_stub(const char *);
extern int agent_agent_init_u_moa_reference_output_allowed(const char *);
extern int agent_agent_init_u_relay_moa_reference_event(const char *);
extern int agent_agent_init_u_provider_default_routes(const char *);
extern int agent_agent_init_u_context_route_mismatch(const char *);
extern int agent_agent_init_u_normalize_custom_provider_name(const char *);
extern int agent_agent_init_u_custom_provider_runtime_ids(const char *);
extern int agent_agent_init_u_build_codex_gpt5_autoraise_notice(const char *);
extern int agent_agent_init_u_resolve_compression_threshold(const char *);
extern int agent_agent_init_u_codex_gpt55_autoraise_notice_marker(const char *);
extern int agent_agent_init_u_codex_gpt55_autoraise_notice_state(const char *);
extern int agent_agent_init_u_codex_gpt55_autoraise_notice_seen(const char *);
extern int agent_agent_init_u_record_codex_gpt55_autoraise_notice(const char *);
extern int agent_rate_limit_tracker_usage_pct(const char *);
extern int agent_rate_limit_tracker_remaining_seconds_now(const char *);
extern int agent_rate_limit_tracker_has_data(const char *);
extern int agent_rate_limit_tracker_age_seconds(const char *);
extern int agent_rate_limit_tracker_u_safe_float(const char *);
extern int agent_rate_limit_tracker_parse_rate_limit_headers(const char *);
extern int agent_rate_limit_tracker_u_fmt_count(const char *);
extern int agent_rate_limit_tracker_u_fmt_seconds(const char *);
extern int agent_rate_limit_tracker_u_bar(const char *);
extern int agent_rate_limit_tracker_u_bucket_line(const char *);
extern int agent_rate_limit_tracker_format_rate_limit_display(const char *);
extern int agent_rate_limit_tracker_format_rate_limit_compact(const char *);
extern int agent_credential_pool_u_normalize_pool_auth_type(const char *);
extern int agent_credential_pool_credential_pool_matches_provider(const char *);
extern int agent_credential_pool_u_current_unlocked(const char *);
extern int agent_credential_pool_entry_id_for_api_key(const char *);
extern int agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(const char *);
extern int agent_credential_pool_u_single_use_refresh_lock_timeout(const char *);
extern int agent_credential_pool_u_codex_quota_restored_upstream(const char *);
extern int agent_credential_pool_u_log_no_available_entries(const char *);
extern int agent_credential_pool_try_refresh_matching(const char *);
extern int agent_turn_context_compose_user_api_content(const char *);
extern int agent_turn_context_substitute_api_content(const char *);
extern int agent_turn_context_drop_stale_api_content(const char *);
extern int agent_turn_context_extract_api_content_sidecar(const char *);
extern int agent_turn_context_consume_gateway_turn_context_notes(const char *);
extern int agent_turn_context_append_notes_to_multimodal_content(const char *);
extern int agent_turn_context_reanchor_current_turn_user_idx(const char *);
extern int agent_turn_context_u_compression_warrants_another_preflight__ss(const char *);
extern int agent_turn_context_u_should_idle_compact(const char *);
extern int agent_trace_upload_u_now_iso(const char *);
extern int agent_trace_upload_u_content_to_blocks(const char *);
extern int agent_trace_upload_u_tool_calls_to_blocks(const char *);
extern int agent_trace_upload_build_trace_jsonl(const char *);
extern int agent_trace_upload_u_resolve_hf_token(const char *);
extern int agent_trace_upload_u_do_upload(const char *);
extern int agent_trace_upload_load_session_messages(const char *);
extern int agent_trace_upload_upload_session_trace(const char *);
extern int agent_context_engine_sanitize_memory_context(const char *);
extern int agent_context_engine_automatic_compaction_status_message(const char *);
extern int agent_context_engine_should_compress_info(const char *);
extern int agent_context_engine_prune_tool_results_only(const char *);
extern int agent_context_engine_select_context(const char *);
extern int agent_context_engine_on_turn_complete(const char *);
extern int agent_context_engine_get_automatic_compaction_status_message(const char *);
extern int agent_error_classifier_classify_api_error(const char *);
extern int agent_error_classifier_u_classify_by_status(const char *);
extern int agent_error_classifier_u_classify_402(const char *);
extern int agent_error_classifier_u_classify_400(const char *);
extern int agent_error_classifier_u_classify_by_error_code(const char *);
extern int agent_error_classifier_u_classify_by_message(const char *);
extern int agent_error_classifier_u_extract_error_code(const char *);
extern int agent_memory_provider_queue_prefetch(const char *);
extern int agent_memory_provider_sync_turn(const char *);
extern int agent_memory_provider_on_turn_start(const char *);
extern int agent_memory_provider_on_session_switch(const char *);
extern int agent_memory_provider_on_pre_compress(const char *);
extern int agent_memory_provider_on_delegation(const char *);
extern int agent_memory_provider_on_memory_write(const char *);
extern int agent_codex_runtime_u_codex_item_to_tool_name(const char *);
extern int agent_codex_runtime_u_codex_item_to_args(const char *);
extern int agent_codex_runtime_u_codex_item_to_preview(const char *);
extern int agent_codex_runtime_u_codex_item_completion_payload(const char *);
extern int agent_codex_runtime_make_codex_app_server_event_bridge(const char *);
extern int agent_codex_runtime_u_item_field(const char *);
extern int agent_conversation_loop_u_apply_active_turn_redirect(const char *);
extern int agent_conversation_loop_u_billing_block_dict(const char *);
extern int agent_conversation_loop_u_invalid_tool_name_error_content(const char *);
extern int agent_conversation_loop_u_compression_deferred_result(const char *);
extern int agent_conversation_loop_u_apply_context_engine_selection(const char *);
extern int agent_conversation_loop_u_notify_context_engine_turn_complete(const char *);
extern int agent_pet_generate_imagegen_u_forced_provider_from_env(const char *);
extern int agent_pet_generate_imagegen_resolve_provider(const char *);
extern int agent_pet_generate_imagegen_list_sprite_providers(const char *);
extern int agent_pet_generate_imagegen_u_save_local(const char *);
extern int agent_pet_generate_imagegen_u_rejected_background(const char *);
extern int agent_pet_generate_orchestrate_u_harden_transparency(const char *);
extern int agent_pet_generate_orchestrate_generate_base_drafts(const char *);
extern int agent_pet_generate_orchestrate_u_drafts_failed_reason(const char *);
extern int agent_pet_generate_orchestrate_u_humanize_image_error(const char *);
extern int agent_pet_generate_orchestrate_hatch_pet(const char *);
extern int agent_account_usage_u_codex_backend_urls(const char *);
extern int agent_account_usage_u_resolve_codex_usage_credentials(const char *);
extern int agent_account_usage_redeemed(const char *);
extern int agent_account_usage_redeem_codex_reset_credit(const char *);
extern int agent_bedrock_adapter_u_model_supports_prompt_cache(const char *);
extern int agent_bedrock_adapter_u_safe_text(const char *);
extern int agent_bedrock_adapter_u_static_bedrock_context_length(const char *);
extern int agent_bedrock_adapter_probe_bedrock_context_length(const char *);
extern int agent_kanban_stop_kanban_stop_nudge_enabled(const char *);
extern int agent_kanban_stop_u_tool_call_name(const char *);
extern int agent_kanban_stop_session_called_kanban_terminal(const char *);
extern int agent_kanban_stop_build_kanban_stop_nudge(const char *);
extern int agent_thread_scoped_output_unsilence(const char *);
extern int agent_thread_scoped_output_writelines(const char *);
extern int agent_thread_scoped_output_u__getattr__(const char *);
extern int agent_thread_scoped_output_thread_scoped_silence(const char *);
extern int agent_agent_runtime_helpers_note_turn_start(const char *);
extern int agent_agent_runtime_helpers_note_turn_persisted(const char *);
extern int agent_agent_runtime_helpers_sync_credential_pool_entry_id(const char *);
extern int agent_anthropic_adapter_u_get_hermes_oauth_file(const char *);
extern int agent_anthropic_adapter_u_safe_text(const char *);
extern int agent_anthropic_adapter_u_ensure_leading_user_turn(const char *);
extern int agent_battery_u_read_battery_uncached(const char *);
extern int agent_battery_read_battery(const char *);
extern int agent_battery_clear_cache(const char *);
extern int agent_billing_view_can_change_plan(const char *);
extern int agent_billing_view_u_parse_auto_reload_card(const char *);
extern int agent_billing_view_u_dev_fixture_billing_state(const char *);
extern int agent_bounded_response_read_streaming_error_body(const char *);
extern int agent_bounded_response_u_safe_close(const char *);
extern int agent_bounded_response_read_error_body_or_default(const char *);
extern int agent_display_u_display_url(const char *);
extern int agent_display_build_status_phrase(const char *);
extern int agent_display_u_get_cute_tool_message(const char *);
extern int agent_moa_trace_u_traces_enabled_and_dir(const char *);
extern int agent_moa_trace_u_slot_trace(const char *);
extern int agent_moa_trace_save_moa_turn(const char *);
extern int agent_oneshot_u_commit_message_template(const char *);
extern int agent_oneshot_render_template(const char *);
extern int agent_oneshot_run_oneshot(const char *);
extern int agent_tool_executor_u_ensure_file_checkpoint(const char *);
extern int agent_tool_executor_u_parse_tool_arguments(const char *);
extern int agent_tool_executor_execute_tool_calls_segmented(const char *);
extern int agent_auxiliary_client_u_try_nvidia_nim(const char *);
extern int agent_auxiliary_client_u_obj_get(const char *);
extern int agent_billing_usage_build_usage_model(const char *);
extern int agent_billing_usage_u_dev_fixture_usage_model(const char *);
extern int agent_credits_tracker_has_data(const char *);
extern int agent_credits_tracker_age_seconds(const char *);
extern int agent_prompt_caching_u_can_carry_marker(const char *);
extern int agent_prompt_caching_u_apply_system_cache_markers(const char *);
extern int agent_redact_u_canonical_url_param_name(const char *);
extern int agent_redact_u_redact_strict_url_credentials(const char *);
extern int agent_skill_commands_split_stacked_skill_commands(const char *);
extern int agent_skill_commands_build_stacked_skill_invocation_message(const char *);
extern int agent_stream_single_writer_claim_stream_writer(const char *);
extern int agent_stream_single_writer_stream_writer_is_current(const char *);
extern int agent_turn_finalizer_u_is_pure_tool_call_tail(const char *);
extern int agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng(const char *);
extern int agent_video_gen_provider_save_url_video(const char *);
extern int agent_video_gen_provider_u_create_and_poll(const char *);
extern int agent_context_references_format_reference_value(const char *);
extern int agent_credential_sources_u_remove_xai_oauth_device_code(const char *);
extern int agent_insights_u_get_model_usage(const char *);
extern int agent_jiter_preload_preload_jiter_native_extension(const char *);
extern int agent_moonshot_schema_u_ensure_required_array(const char *);
extern int agent_runtime_cwd_u_is_install_tree(const char *);
extern int agent_system_prompt_u_tui_embedded_pane_clarifier(const char *);
extern int agent_tool_result_classificati_tool_may_have_side_effect(const char *);
extern int agent_web_search_provider_get_provider_env(const char *);
extern int agent_web_search_registry_u_disabled_web_plugin_for(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_agent_model_metadata_u_endpoint_scoped_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_endpoint_scoped_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_endpoint_scoped_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_skip_persistent_context_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_skip_persistent_context_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_skip_persistent_context_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_maybe_cache_local_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_maybe_cache_local_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_maybe_cache_local_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_reconcile_local_cached_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_reconcile_local_cached_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_reconcile_local_cached_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_localhost_to_ipv4(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_localhost_to_ipv4(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_localhost_to_ipv4"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_context_cache_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_context_cache_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_context_cache_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_query_ollama_api_show_uncached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_query_ollama_api_show_uncached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_query_ollama_api_show_uncached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_query_local_context_length_uncached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_query_local_context_length_uncached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_query_local_context_length_uncached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_codex_oauth_token_fingerprint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_codex_oauth_token_fingerprint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_codex_oauth_token_fingerprint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_extract_chatgpt_account_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_extract_chatgpt_account_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_extract_chatgpt_account_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_is_cjk_token_dense_char(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_is_cjk_token_dense_char(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_is_cjk_token_dense_char"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_estimate_message_tokens_without_images(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_estimate_message_tokens_without_images(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_estimate_message_tokens_without_images"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_tool_name_for_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_tool_name_for_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_tool_name_for_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_model_metadata_u_estimate_tools_tokens_rough(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_model_metadata_u_estimate_tools_tokens_rough(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_model_metadata_u_estimate_tools_tokens_rough"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_has_slot_padding(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_has_slot_padding(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_has_slot_padding"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_slot_bounds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_slot_bounds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_slot_bounds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_component_crops(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_component_crops(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_component_crops"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_sever_expected_gutters(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_sever_expected_gutters(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_sever_expected_gutters"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_slot_crops(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_slot_crops(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_slot_crops"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_frame_x_ranges(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_frame_x_ranges(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_frame_x_ranges"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_significant_subject_boxes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_significant_subject_boxes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_significant_subject_boxes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_validate_extracted_frames(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_validate_extracted_frames(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_validate_extracted_frames"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_extract_strip_frames(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_extract_strip_frames(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_extract_strip_frames"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_normalize_cells(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_normalize_cells(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_normalize_cells"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_single_frame(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_single_frame(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_single_frame"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_u_clear_transparent_rgb(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_u_clear_transparent_rgb(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_u_clear_transparent_rgb"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_mirror_frames(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_mirror_frames(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_mirror_frames"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_compose_atlas(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_compose_atlas(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_compose_atlas"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_atlas_to_webp_bytes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_atlas_to_webp_bytes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_atlas_to_webp_bytes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_atlas_validate_atlas(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_atlas_validate_atlas(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_atlas_validate_atlas"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_is_destructive_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_is_destructive_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_is_destructive_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_plan_tool_batch_segments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_plan_tool_batch_segments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_plan_tool_batch_segments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_should_parallelize_tool_batch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_should_parallelize_tool_batch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_should_parallelize_tool_batch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_extract_parallel_scope_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_extract_parallel_scope_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_extract_parallel_scope_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_paths_overlap(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_paths_overlap(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_paths_overlap"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_is_multimodal_tool_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_is_multimodal_tool_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_is_multimodal_tool_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_multimodal_text_summary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_multimodal_text_summary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_multimodal_text_summary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_extract_file_mutation_targets(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_extract_file_mutation_targets(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_extract_file_mutation_targets"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_extract_error_preview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_extract_error_preview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_extract_error_preview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_trajectory_normalize_msg(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_trajectory_normalize_msg(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_trajectory_normalize_msg"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_make_tool_result_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_make_tool_result_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_make_tool_result_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_is_untrusted_tool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_is_untrusted_tool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_is_untrusted_tool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_tool_output_risk_metadata(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_tool_output_risk_metadata(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_tool_output_risk_metadata"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_dispatch_helpers_u_maybe_wrap_untrusted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_dispatch_helpers_u_maybe_wrap_untrusted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_dispatch_helpers_u_maybe_wrap_untrusted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_can_change_plan(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_can_change_plan(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_can_change_plan"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_u_parse_current(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_u_parse_current(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_u_parse_current"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_u_coalesce(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_u_coalesce(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_u_coalesce"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_u_parse_tier(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_u_parse_tier(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_u_parse_tier"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_subscription_change_preview_from_pay_ad(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_subscription_change_preview_from_pay_ad(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_subscription_change_preview_from_pay_ad"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_subscription_state_from_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_subscription_state_from_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_subscription_state_from_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_build_subscription_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_build_subscription_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_build_subscription_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_subscription_manage_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_subscription_manage_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_subscription_manage_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_u_format_dollars_grouped(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_u_format_dollars_grouped(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_u_format_dollars_grouped"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_selectable_tiers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_selectable_tiers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_selectable_tiers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_format_tier_row(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_format_tier_row(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_format_tier_row"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_is_upgrade(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_is_upgrade(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_is_upgrade"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_u_dev_current(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_u_dev_current(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_u_dev_current"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_u_dev_tiers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_u_dev_tiers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_u_dev_tiers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_subscription_view_dev_fixture_subscription_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_subscription_view_dev_fixture_subscription_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_subscription_view_dev_fixture_subscription_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_openai_codex_stale_timeout_floor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_openai_codex_stale_timeout_floor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_openai_codex_stale_timeout_floor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_provider_preferences_for_agent(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_provider_preferences_for_agent(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_provider_preferences_for_agent"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_codex_wait_notice_recovery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_codex_wait_notice_recovery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_codex_wait_notice_recovery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_stale_streak(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_stale_streak(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_stale_streak"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_bump_stale_streak(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_bump_stale_streak(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_bump_stale_streak"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_reset_stale_streak(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_reset_stale_streak(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_reset_stale_streak"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_check_stale_giveup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_check_stale_giveup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_check_stale_giveup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_derive_stream_stale_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_derive_stream_stale_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_derive_stream_stale_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_should_use_direct_api_call(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_should_use_direct_api_call(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_should_use_direct_api_call"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_direct_api_call(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_direct_api_call(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_direct_api_call"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_chat_completion_helpers_u_build_partial_stream_stub(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_chat_completion_helpers_u_build_partial_stream_stub(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_chat_completion_helpers_u_build_partial_stream_stub"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_moa_reference_output_allowed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_moa_reference_output_allowed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_moa_reference_output_allowed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_relay_moa_reference_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_relay_moa_reference_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_relay_moa_reference_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_provider_default_routes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_provider_default_routes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_provider_default_routes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_context_route_mismatch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_context_route_mismatch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_context_route_mismatch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_normalize_custom_provider_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_normalize_custom_provider_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_normalize_custom_provider_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_custom_provider_runtime_ids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_custom_provider_runtime_ids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_custom_provider_runtime_ids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_build_codex_gpt5_autoraise_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_build_codex_gpt5_autoraise_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_build_codex_gpt5_autoraise_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_resolve_compression_threshold(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_resolve_compression_threshold(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_resolve_compression_threshold"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_codex_gpt55_autoraise_notice_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_codex_gpt55_autoraise_notice_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_codex_gpt55_autoraise_notice_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_codex_gpt55_autoraise_notice_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_codex_gpt55_autoraise_notice_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_codex_gpt55_autoraise_notice_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_codex_gpt55_autoraise_notice_seen(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_codex_gpt55_autoraise_notice_seen(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_codex_gpt55_autoraise_notice_seen"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_init_u_record_codex_gpt55_autoraise_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_init_u_record_codex_gpt55_autoraise_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_init_u_record_codex_gpt55_autoraise_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_usage_pct(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_usage_pct(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_usage_pct"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_remaining_seconds_now(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_remaining_seconds_now(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_remaining_seconds_now"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_has_data(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_has_data(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_has_data"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_age_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_age_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_age_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_u_safe_float(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_u_safe_float(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_u_safe_float"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_parse_rate_limit_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_parse_rate_limit_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_parse_rate_limit_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_u_fmt_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_u_fmt_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_u_fmt_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_u_fmt_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_u_fmt_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_u_fmt_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_u_bar(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_u_bar(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_u_bar"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_u_bucket_line(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_u_bucket_line(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_u_bucket_line"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_format_rate_limit_display(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_format_rate_limit_display(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_format_rate_limit_display"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_rate_limit_tracker_format_rate_limit_compact(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_rate_limit_tracker_format_rate_limit_compact(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_rate_limit_tracker_format_rate_limit_compact"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_u_normalize_pool_auth_type(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_u_normalize_pool_auth_type(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_u_normalize_pool_auth_type"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_credential_pool_matches_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_credential_pool_matches_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_credential_pool_matches_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_u_current_unlocked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_u_current_unlocked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_u_current_unlocked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_entry_id_for_api_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_entry_id_for_api_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_entry_id_for_api_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_u_single_use_refresh_lock_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_u_single_use_refresh_lock_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_u_single_use_refresh_lock_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_u_codex_quota_restored_upstream(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_u_codex_quota_restored_upstream(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_u_codex_quota_restored_upstream"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_u_log_no_available_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_u_log_no_available_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_u_log_no_available_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_pool_try_refresh_matching(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_pool_try_refresh_matching(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_pool_try_refresh_matching"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_compose_user_api_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_compose_user_api_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_compose_user_api_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_substitute_api_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_substitute_api_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_substitute_api_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_drop_stale_api_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_drop_stale_api_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_drop_stale_api_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_extract_api_content_sidecar(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_extract_api_content_sidecar(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_extract_api_content_sidecar"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_consume_gateway_turn_context_notes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_consume_gateway_turn_context_notes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_consume_gateway_turn_context_notes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_append_notes_to_multimodal_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_append_notes_to_multimodal_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_append_notes_to_multimodal_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_reanchor_current_turn_user_idx(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_reanchor_current_turn_user_idx(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_reanchor_current_turn_user_idx"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_u_compression_warrants_another_preflight__ss(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_u_compression_warrants_another_preflight__ss(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_u_compression_warrants_another_preflight__ss"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_context_u_should_idle_compact(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_context_u_should_idle_compact(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_context_u_should_idle_compact"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_u_now_iso(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_u_now_iso(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_u_now_iso"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_u_content_to_blocks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_u_content_to_blocks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_u_content_to_blocks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_u_tool_calls_to_blocks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_u_tool_calls_to_blocks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_u_tool_calls_to_blocks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_build_trace_jsonl(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_build_trace_jsonl(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_build_trace_jsonl"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_u_resolve_hf_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_u_resolve_hf_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_u_resolve_hf_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_u_do_upload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_u_do_upload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_u_do_upload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_load_session_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_load_session_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_load_session_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_trace_upload_upload_session_trace(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_trace_upload_upload_session_trace(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_trace_upload_upload_session_trace"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_sanitize_memory_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_sanitize_memory_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_sanitize_memory_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_automatic_compaction_status_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_automatic_compaction_status_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_automatic_compaction_status_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_should_compress_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_should_compress_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_should_compress_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_prune_tool_results_only(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_prune_tool_results_only(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_prune_tool_results_only"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_select_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_select_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_select_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_on_turn_complete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_on_turn_complete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_on_turn_complete"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_engine_get_automatic_compaction_status_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_engine_get_automatic_compaction_status_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_engine_get_automatic_compaction_status_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_classify_api_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_classify_api_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_classify_api_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_u_classify_by_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_u_classify_by_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_u_classify_by_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_u_classify_402(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_u_classify_402(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_u_classify_402"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_u_classify_400(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_u_classify_400(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_u_classify_400"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_u_classify_by_error_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_u_classify_by_error_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_u_classify_by_error_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_u_classify_by_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_u_classify_by_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_u_classify_by_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_error_classifier_u_extract_error_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_error_classifier_u_extract_error_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_error_classifier_u_extract_error_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_queue_prefetch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_queue_prefetch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_queue_prefetch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_sync_turn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_sync_turn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_sync_turn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_on_turn_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_on_turn_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_on_turn_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_on_session_switch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_on_session_switch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_on_session_switch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_on_pre_compress(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_on_pre_compress(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_on_pre_compress"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_on_delegation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_on_delegation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_on_delegation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_memory_provider_on_memory_write(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_memory_provider_on_memory_write(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_memory_provider_on_memory_write"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_codex_runtime_u_codex_item_to_tool_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_codex_runtime_u_codex_item_to_tool_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_codex_runtime_u_codex_item_to_tool_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_codex_runtime_u_codex_item_to_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_codex_runtime_u_codex_item_to_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_codex_runtime_u_codex_item_to_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_codex_runtime_u_codex_item_to_preview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_codex_runtime_u_codex_item_to_preview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_codex_runtime_u_codex_item_to_preview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_codex_runtime_u_codex_item_completion_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_codex_runtime_u_codex_item_completion_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_codex_runtime_u_codex_item_completion_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_codex_runtime_make_codex_app_server_event_bridge(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_codex_runtime_make_codex_app_server_event_bridge(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_codex_runtime_make_codex_app_server_event_bridge"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_codex_runtime_u_item_field(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_codex_runtime_u_item_field(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_codex_runtime_u_item_field"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_conversation_loop_u_apply_active_turn_redirect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_conversation_loop_u_apply_active_turn_redirect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_conversation_loop_u_apply_active_turn_redirect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_conversation_loop_u_billing_block_dict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_conversation_loop_u_billing_block_dict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_conversation_loop_u_billing_block_dict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_conversation_loop_u_invalid_tool_name_error_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_conversation_loop_u_invalid_tool_name_error_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_conversation_loop_u_invalid_tool_name_error_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_conversation_loop_u_compression_deferred_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_conversation_loop_u_compression_deferred_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_conversation_loop_u_compression_deferred_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_conversation_loop_u_apply_context_engine_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_conversation_loop_u_apply_context_engine_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_conversation_loop_u_apply_context_engine_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_conversation_loop_u_notify_context_engine_turn_complete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_conversation_loop_u_notify_context_engine_turn_complete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_conversation_loop_u_notify_context_engine_turn_complete"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_imagegen_u_forced_provider_from_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_imagegen_u_forced_provider_from_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_imagegen_u_forced_provider_from_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_imagegen_resolve_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_imagegen_resolve_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_imagegen_resolve_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_imagegen_list_sprite_providers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_imagegen_list_sprite_providers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_imagegen_list_sprite_providers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_imagegen_u_save_local(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_imagegen_u_save_local(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_imagegen_u_save_local"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_imagegen_u_rejected_background(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_imagegen_u_rejected_background(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_imagegen_u_rejected_background"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_orchestrate_u_harden_transparency(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_orchestrate_u_harden_transparency(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_orchestrate_u_harden_transparency"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_orchestrate_generate_base_drafts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_orchestrate_generate_base_drafts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_orchestrate_generate_base_drafts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_orchestrate_u_drafts_failed_reason(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_orchestrate_u_drafts_failed_reason(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_orchestrate_u_drafts_failed_reason"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_orchestrate_u_humanize_image_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_orchestrate_u_humanize_image_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_orchestrate_u_humanize_image_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_pet_generate_orchestrate_hatch_pet(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_pet_generate_orchestrate_hatch_pet(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_pet_generate_orchestrate_hatch_pet"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_account_usage_u_codex_backend_urls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_account_usage_u_codex_backend_urls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_account_usage_u_codex_backend_urls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_account_usage_u_resolve_codex_usage_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_account_usage_u_resolve_codex_usage_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_account_usage_u_resolve_codex_usage_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_account_usage_redeemed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_account_usage_redeemed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_account_usage_redeemed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_account_usage_redeem_codex_reset_credit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_account_usage_redeem_codex_reset_credit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_account_usage_redeem_codex_reset_credit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bedrock_adapter_u_model_supports_prompt_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bedrock_adapter_u_model_supports_prompt_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bedrock_adapter_u_model_supports_prompt_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bedrock_adapter_u_safe_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bedrock_adapter_u_safe_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bedrock_adapter_u_safe_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bedrock_adapter_u_static_bedrock_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bedrock_adapter_u_static_bedrock_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bedrock_adapter_u_static_bedrock_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bedrock_adapter_probe_bedrock_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bedrock_adapter_probe_bedrock_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bedrock_adapter_probe_bedrock_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_kanban_stop_kanban_stop_nudge_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_kanban_stop_kanban_stop_nudge_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_kanban_stop_kanban_stop_nudge_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_kanban_stop_u_tool_call_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_kanban_stop_u_tool_call_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_kanban_stop_u_tool_call_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_kanban_stop_session_called_kanban_terminal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_kanban_stop_session_called_kanban_terminal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_kanban_stop_session_called_kanban_terminal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_kanban_stop_build_kanban_stop_nudge(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_kanban_stop_build_kanban_stop_nudge(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_kanban_stop_build_kanban_stop_nudge"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_thread_scoped_output_unsilence(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_thread_scoped_output_unsilence(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_thread_scoped_output_unsilence"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_thread_scoped_output_writelines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_thread_scoped_output_writelines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_thread_scoped_output_writelines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_thread_scoped_output_u__getattr__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_thread_scoped_output_u__getattr__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_thread_scoped_output_u__getattr__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_thread_scoped_output_thread_scoped_silence(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_thread_scoped_output_thread_scoped_silence(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_thread_scoped_output_thread_scoped_silence"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_runtime_helpers_note_turn_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_runtime_helpers_note_turn_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_runtime_helpers_note_turn_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_runtime_helpers_note_turn_persisted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_runtime_helpers_note_turn_persisted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_runtime_helpers_note_turn_persisted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_agent_runtime_helpers_sync_credential_pool_entry_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_agent_runtime_helpers_sync_credential_pool_entry_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_agent_runtime_helpers_sync_credential_pool_entry_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_anthropic_adapter_u_get_hermes_oauth_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_anthropic_adapter_u_get_hermes_oauth_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_anthropic_adapter_u_get_hermes_oauth_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_anthropic_adapter_u_safe_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_anthropic_adapter_u_safe_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_anthropic_adapter_u_safe_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_anthropic_adapter_u_ensure_leading_user_turn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_anthropic_adapter_u_ensure_leading_user_turn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_anthropic_adapter_u_ensure_leading_user_turn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_battery_u_read_battery_uncached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_battery_u_read_battery_uncached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_battery_u_read_battery_uncached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_battery_read_battery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_battery_read_battery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_battery_read_battery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_battery_clear_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_battery_clear_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_battery_clear_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_billing_view_can_change_plan(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_billing_view_can_change_plan(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_billing_view_can_change_plan"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_billing_view_u_parse_auto_reload_card(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_billing_view_u_parse_auto_reload_card(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_billing_view_u_parse_auto_reload_card"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_billing_view_u_dev_fixture_billing_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_billing_view_u_dev_fixture_billing_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_billing_view_u_dev_fixture_billing_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bounded_response_read_streaming_error_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bounded_response_read_streaming_error_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bounded_response_read_streaming_error_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bounded_response_u_safe_close(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bounded_response_u_safe_close(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bounded_response_u_safe_close"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_bounded_response_read_error_body_or_default(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_bounded_response_read_error_body_or_default(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_bounded_response_read_error_body_or_default"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_display_u_display_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_display_u_display_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_display_u_display_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_display_build_status_phrase(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_display_build_status_phrase(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_display_build_status_phrase"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_display_u_get_cute_tool_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_display_u_get_cute_tool_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_display_u_get_cute_tool_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_moa_trace_u_traces_enabled_and_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_moa_trace_u_traces_enabled_and_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_moa_trace_u_traces_enabled_and_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_moa_trace_u_slot_trace(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_moa_trace_u_slot_trace(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_moa_trace_u_slot_trace"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_moa_trace_save_moa_turn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_moa_trace_save_moa_turn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_moa_trace_save_moa_turn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_oneshot_u_commit_message_template(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_oneshot_u_commit_message_template(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_oneshot_u_commit_message_template"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_oneshot_render_template(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_oneshot_render_template(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_oneshot_render_template"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_oneshot_run_oneshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_oneshot_run_oneshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_oneshot_run_oneshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_executor_u_ensure_file_checkpoint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_executor_u_ensure_file_checkpoint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_executor_u_ensure_file_checkpoint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_executor_u_parse_tool_arguments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_executor_u_parse_tool_arguments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_executor_u_parse_tool_arguments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_executor_execute_tool_calls_segmented(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_executor_execute_tool_calls_segmented(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_executor_execute_tool_calls_segmented"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_auxiliary_client_u_try_nvidia_nim(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_auxiliary_client_u_try_nvidia_nim(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_auxiliary_client_u_try_nvidia_nim"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_auxiliary_client_u_obj_get(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_auxiliary_client_u_obj_get(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_auxiliary_client_u_obj_get"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_billing_usage_build_usage_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_billing_usage_build_usage_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_billing_usage_build_usage_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_billing_usage_u_dev_fixture_usage_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_billing_usage_u_dev_fixture_usage_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_billing_usage_u_dev_fixture_usage_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credits_tracker_has_data(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credits_tracker_has_data(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credits_tracker_has_data"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credits_tracker_age_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credits_tracker_age_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credits_tracker_age_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_prompt_caching_u_can_carry_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_prompt_caching_u_can_carry_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_prompt_caching_u_can_carry_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_prompt_caching_u_apply_system_cache_markers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_prompt_caching_u_apply_system_cache_markers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_prompt_caching_u_apply_system_cache_markers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_redact_u_canonical_url_param_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_redact_u_canonical_url_param_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_redact_u_canonical_url_param_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_redact_u_redact_strict_url_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_redact_u_redact_strict_url_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_redact_u_redact_strict_url_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_skill_commands_split_stacked_skill_commands(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_skill_commands_split_stacked_skill_commands(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_skill_commands_split_stacked_skill_commands"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_skill_commands_build_stacked_skill_invocation_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_skill_commands_build_stacked_skill_invocation_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_skill_commands_build_stacked_skill_invocation_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_stream_single_writer_claim_stream_writer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_stream_single_writer_claim_stream_writer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_stream_single_writer_claim_stream_writer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_stream_single_writer_stream_writer_is_current(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_stream_single_writer_stream_writer_is_current(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_stream_single_writer_stream_writer_is_current"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_finalizer_u_is_pure_tool_call_tail(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_finalizer_u_is_pure_tool_call_tail(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_finalizer_u_is_pure_tool_call_tail"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_video_gen_provider_save_url_video(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_video_gen_provider_save_url_video(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_video_gen_provider_save_url_video"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_video_gen_provider_u_create_and_poll(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_video_gen_provider_u_create_and_poll(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_video_gen_provider_u_create_and_poll"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_context_references_format_reference_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_context_references_format_reference_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_context_references_format_reference_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_credential_sources_u_remove_xai_oauth_device_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_credential_sources_u_remove_xai_oauth_device_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_credential_sources_u_remove_xai_oauth_device_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_insights_u_get_model_usage(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_insights_u_get_model_usage(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_insights_u_get_model_usage"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_jiter_preload_preload_jiter_native_extension(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_jiter_preload_preload_jiter_native_extension(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_jiter_preload_preload_jiter_native_extension"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_moonshot_schema_u_ensure_required_array(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_moonshot_schema_u_ensure_required_array(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_moonshot_schema_u_ensure_required_array"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_runtime_cwd_u_is_install_tree(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_runtime_cwd_u_is_install_tree(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_runtime_cwd_u_is_install_tree"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_system_prompt_u_tui_embedded_pane_clarifier(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_system_prompt_u_tui_embedded_pane_clarifier(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_system_prompt_u_tui_embedded_pane_clarifier"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_tool_result_classificati_tool_may_have_side_effect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_tool_result_classificati_tool_may_have_side_effect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_tool_result_classificati_tool_may_have_side_effect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_web_search_provider_get_provider_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_web_search_provider_get_provider_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_web_search_provider_get_provider_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_agent_web_search_registry_u_disabled_web_plugin_for(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)agent_web_search_registry_u_disabled_web_plugin_for(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("agent_web_search_registry_u_disabled_web_plugin_for"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "agent_model_metadata_u_endpoint_scoped_context_length") == 0) o = emit_agent_model_metadata_u_endpoint_scoped_context_length(c);
        if (strcmp(op, "agent_model_metadata_u_skip_persistent_context_cache") == 0) o = emit_agent_model_metadata_u_skip_persistent_context_cache(c);
        if (strcmp(op, "agent_model_metadata_u_maybe_cache_local_context_length") == 0) o = emit_agent_model_metadata_u_maybe_cache_local_context_length(c);
        if (strcmp(op, "agent_model_metadata_u_reconcile_local_cached_context_length") == 0) o = emit_agent_model_metadata_u_reconcile_local_cached_context_length(c);
        if (strcmp(op, "agent_model_metadata_u_localhost_to_ipv4") == 0) o = emit_agent_model_metadata_u_localhost_to_ipv4(c);
        if (strcmp(op, "agent_model_metadata_u_context_cache_key") == 0) o = emit_agent_model_metadata_u_context_cache_key(c);
        if (strcmp(op, "agent_model_metadata_u_query_ollama_api_show_uncached") == 0) o = emit_agent_model_metadata_u_query_ollama_api_show_uncached(c);
        if (strcmp(op, "agent_model_metadata_u_query_local_context_length_uncached") == 0) o = emit_agent_model_metadata_u_query_local_context_length_uncached(c);
        if (strcmp(op, "agent_model_metadata_u_codex_oauth_token_fingerprint") == 0) o = emit_agent_model_metadata_u_codex_oauth_token_fingerprint(c);
        if (strcmp(op, "agent_model_metadata_u_extract_chatgpt_account_id") == 0) o = emit_agent_model_metadata_u_extract_chatgpt_account_id(c);
        if (strcmp(op, "agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce") == 0) o = emit_agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce(c);
        if (strcmp(op, "agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce") == 0) o = emit_agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce(c);
        if (strcmp(op, "agent_model_metadata_u_is_cjk_token_dense_char") == 0) o = emit_agent_model_metadata_u_is_cjk_token_dense_char(c);
        if (strcmp(op, "agent_model_metadata_u_estimate_message_tokens_without_images") == 0) o = emit_agent_model_metadata_u_estimate_message_tokens_without_images(c);
        if (strcmp(op, "agent_model_metadata_u_tool_name_for_cache") == 0) o = emit_agent_model_metadata_u_tool_name_for_cache(c);
        if (strcmp(op, "agent_model_metadata_u_estimate_tools_tokens_rough") == 0) o = emit_agent_model_metadata_u_estimate_tools_tokens_rough(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_has_slot_padding") == 0) o = emit_agent_pet_generate_atlas_u_has_slot_padding(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_slot_bounds") == 0) o = emit_agent_pet_generate_atlas_u_slot_bounds(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_component_crops") == 0) o = emit_agent_pet_generate_atlas_u_component_crops(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_sever_expected_gutters") == 0) o = emit_agent_pet_generate_atlas_u_sever_expected_gutters(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_slot_crops") == 0) o = emit_agent_pet_generate_atlas_u_slot_crops(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_frame_x_ranges") == 0) o = emit_agent_pet_generate_atlas_u_frame_x_ranges(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_significant_subject_boxes") == 0) o = emit_agent_pet_generate_atlas_u_significant_subject_boxes(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_validate_extracted_frames") == 0) o = emit_agent_pet_generate_atlas_u_validate_extracted_frames(c);
        if (strcmp(op, "agent_pet_generate_atlas_extract_strip_frames") == 0) o = emit_agent_pet_generate_atlas_extract_strip_frames(c);
        if (strcmp(op, "agent_pet_generate_atlas_normalize_cells") == 0) o = emit_agent_pet_generate_atlas_normalize_cells(c);
        if (strcmp(op, "agent_pet_generate_atlas_single_frame") == 0) o = emit_agent_pet_generate_atlas_single_frame(c);
        if (strcmp(op, "agent_pet_generate_atlas_u_clear_transparent_rgb") == 0) o = emit_agent_pet_generate_atlas_u_clear_transparent_rgb(c);
        if (strcmp(op, "agent_pet_generate_atlas_mirror_frames") == 0) o = emit_agent_pet_generate_atlas_mirror_frames(c);
        if (strcmp(op, "agent_pet_generate_atlas_compose_atlas") == 0) o = emit_agent_pet_generate_atlas_compose_atlas(c);
        if (strcmp(op, "agent_pet_generate_atlas_atlas_to_webp_bytes") == 0) o = emit_agent_pet_generate_atlas_atlas_to_webp_bytes(c);
        if (strcmp(op, "agent_pet_generate_atlas_validate_atlas") == 0) o = emit_agent_pet_generate_atlas_validate_atlas(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_is_destructive_command") == 0) o = emit_agent_tool_dispatch_helpers_u_is_destructive_command(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe") == 0) o = emit_agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_plan_tool_batch_segments") == 0) o = emit_agent_tool_dispatch_helpers_u_plan_tool_batch_segments(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_should_parallelize_tool_batch") == 0) o = emit_agent_tool_dispatch_helpers_u_should_parallelize_tool_batch(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_extract_parallel_scope_path") == 0) o = emit_agent_tool_dispatch_helpers_u_extract_parallel_scope_path(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_paths_overlap") == 0) o = emit_agent_tool_dispatch_helpers_u_paths_overlap(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_is_multimodal_tool_result") == 0) o = emit_agent_tool_dispatch_helpers_u_is_multimodal_tool_result(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_multimodal_text_summary") == 0) o = emit_agent_tool_dispatch_helpers_u_multimodal_text_summary(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal") == 0) o = emit_agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_extract_file_mutation_targets") == 0) o = emit_agent_tool_dispatch_helpers_u_extract_file_mutation_targets(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_extract_error_preview") == 0) o = emit_agent_tool_dispatch_helpers_u_extract_error_preview(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_trajectory_normalize_msg") == 0) o = emit_agent_tool_dispatch_helpers_u_trajectory_normalize_msg(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_make_tool_result_message") == 0) o = emit_agent_tool_dispatch_helpers_make_tool_result_message(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_is_untrusted_tool") == 0) o = emit_agent_tool_dispatch_helpers_u_is_untrusted_tool(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_tool_output_risk_metadata") == 0) o = emit_agent_tool_dispatch_helpers_u_tool_output_risk_metadata(c);
        if (strcmp(op, "agent_tool_dispatch_helpers_u_maybe_wrap_untrusted") == 0) o = emit_agent_tool_dispatch_helpers_u_maybe_wrap_untrusted(c);
        if (strcmp(op, "agent_subscription_view_can_change_plan") == 0) o = emit_agent_subscription_view_can_change_plan(c);
        if (strcmp(op, "agent_subscription_view_u_parse_current") == 0) o = emit_agent_subscription_view_u_parse_current(c);
        if (strcmp(op, "agent_subscription_view_u_coalesce") == 0) o = emit_agent_subscription_view_u_coalesce(c);
        if (strcmp(op, "agent_subscription_view_u_parse_tier") == 0) o = emit_agent_subscription_view_u_parse_tier(c);
        if (strcmp(op, "agent_subscription_view_subscription_change_preview_from_pay_ad") == 0) o = emit_agent_subscription_view_subscription_change_preview_from_pay_ad(c);
        if (strcmp(op, "agent_subscription_view_subscription_state_from_payload") == 0) o = emit_agent_subscription_view_subscription_state_from_payload(c);
        if (strcmp(op, "agent_subscription_view_build_subscription_state") == 0) o = emit_agent_subscription_view_build_subscription_state(c);
        if (strcmp(op, "agent_subscription_view_subscription_manage_url") == 0) o = emit_agent_subscription_view_subscription_manage_url(c);
        if (strcmp(op, "agent_subscription_view_u_format_dollars_grouped") == 0) o = emit_agent_subscription_view_u_format_dollars_grouped(c);
        if (strcmp(op, "agent_subscription_view_selectable_tiers") == 0) o = emit_agent_subscription_view_selectable_tiers(c);
        if (strcmp(op, "agent_subscription_view_format_tier_row") == 0) o = emit_agent_subscription_view_format_tier_row(c);
        if (strcmp(op, "agent_subscription_view_is_upgrade") == 0) o = emit_agent_subscription_view_is_upgrade(c);
        if (strcmp(op, "agent_subscription_view_u_dev_current") == 0) o = emit_agent_subscription_view_u_dev_current(c);
        if (strcmp(op, "agent_subscription_view_u_dev_tiers") == 0) o = emit_agent_subscription_view_u_dev_tiers(c);
        if (strcmp(op, "agent_subscription_view_dev_fixture_subscription_state") == 0) o = emit_agent_subscription_view_dev_fixture_subscription_state(c);
        if (strcmp(op, "agent_chat_completion_helpers_openai_codex_stale_timeout_floor") == 0) o = emit_agent_chat_completion_helpers_openai_codex_stale_timeout_floor(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_provider_preferences_for_agent") == 0) o = emit_agent_chat_completion_helpers_u_provider_preferences_for_agent(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_codex_wait_notice_recovery") == 0) o = emit_agent_chat_completion_helpers_u_codex_wait_notice_recovery(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_stale_streak") == 0) o = emit_agent_chat_completion_helpers_u_stale_streak(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_bump_stale_streak") == 0) o = emit_agent_chat_completion_helpers_u_bump_stale_streak(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_reset_stale_streak") == 0) o = emit_agent_chat_completion_helpers_u_reset_stale_streak(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_check_stale_giveup") == 0) o = emit_agent_chat_completion_helpers_u_check_stale_giveup(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_derive_stream_stale_timeout") == 0) o = emit_agent_chat_completion_helpers_u_derive_stream_stale_timeout(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor") == 0) o = emit_agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st") == 0) o = emit_agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st(c);
        if (strcmp(op, "agent_chat_completion_helpers_should_use_direct_api_call") == 0) o = emit_agent_chat_completion_helpers_should_use_direct_api_call(c);
        if (strcmp(op, "agent_chat_completion_helpers_direct_api_call") == 0) o = emit_agent_chat_completion_helpers_direct_api_call(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl") == 0) o = emit_agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl(c);
        if (strcmp(op, "agent_chat_completion_helpers_u_build_partial_stream_stub") == 0) o = emit_agent_chat_completion_helpers_u_build_partial_stream_stub(c);
        if (strcmp(op, "agent_agent_init_u_moa_reference_output_allowed") == 0) o = emit_agent_agent_init_u_moa_reference_output_allowed(c);
        if (strcmp(op, "agent_agent_init_u_relay_moa_reference_event") == 0) o = emit_agent_agent_init_u_relay_moa_reference_event(c);
        if (strcmp(op, "agent_agent_init_u_provider_default_routes") == 0) o = emit_agent_agent_init_u_provider_default_routes(c);
        if (strcmp(op, "agent_agent_init_u_context_route_mismatch") == 0) o = emit_agent_agent_init_u_context_route_mismatch(c);
        if (strcmp(op, "agent_agent_init_u_normalize_custom_provider_name") == 0) o = emit_agent_agent_init_u_normalize_custom_provider_name(c);
        if (strcmp(op, "agent_agent_init_u_custom_provider_runtime_ids") == 0) o = emit_agent_agent_init_u_custom_provider_runtime_ids(c);
        if (strcmp(op, "agent_agent_init_u_build_codex_gpt5_autoraise_notice") == 0) o = emit_agent_agent_init_u_build_codex_gpt5_autoraise_notice(c);
        if (strcmp(op, "agent_agent_init_u_resolve_compression_threshold") == 0) o = emit_agent_agent_init_u_resolve_compression_threshold(c);
        if (strcmp(op, "agent_agent_init_u_codex_gpt55_autoraise_notice_marker") == 0) o = emit_agent_agent_init_u_codex_gpt55_autoraise_notice_marker(c);
        if (strcmp(op, "agent_agent_init_u_codex_gpt55_autoraise_notice_state") == 0) o = emit_agent_agent_init_u_codex_gpt55_autoraise_notice_state(c);
        if (strcmp(op, "agent_agent_init_u_codex_gpt55_autoraise_notice_seen") == 0) o = emit_agent_agent_init_u_codex_gpt55_autoraise_notice_seen(c);
        if (strcmp(op, "agent_agent_init_u_record_codex_gpt55_autoraise_notice") == 0) o = emit_agent_agent_init_u_record_codex_gpt55_autoraise_notice(c);
        if (strcmp(op, "agent_rate_limit_tracker_usage_pct") == 0) o = emit_agent_rate_limit_tracker_usage_pct(c);
        if (strcmp(op, "agent_rate_limit_tracker_remaining_seconds_now") == 0) o = emit_agent_rate_limit_tracker_remaining_seconds_now(c);
        if (strcmp(op, "agent_rate_limit_tracker_has_data") == 0) o = emit_agent_rate_limit_tracker_has_data(c);
        if (strcmp(op, "agent_rate_limit_tracker_age_seconds") == 0) o = emit_agent_rate_limit_tracker_age_seconds(c);
        if (strcmp(op, "agent_rate_limit_tracker_u_safe_float") == 0) o = emit_agent_rate_limit_tracker_u_safe_float(c);
        if (strcmp(op, "agent_rate_limit_tracker_parse_rate_limit_headers") == 0) o = emit_agent_rate_limit_tracker_parse_rate_limit_headers(c);
        if (strcmp(op, "agent_rate_limit_tracker_u_fmt_count") == 0) o = emit_agent_rate_limit_tracker_u_fmt_count(c);
        if (strcmp(op, "agent_rate_limit_tracker_u_fmt_seconds") == 0) o = emit_agent_rate_limit_tracker_u_fmt_seconds(c);
        if (strcmp(op, "agent_rate_limit_tracker_u_bar") == 0) o = emit_agent_rate_limit_tracker_u_bar(c);
        if (strcmp(op, "agent_rate_limit_tracker_u_bucket_line") == 0) o = emit_agent_rate_limit_tracker_u_bucket_line(c);
        if (strcmp(op, "agent_rate_limit_tracker_format_rate_limit_display") == 0) o = emit_agent_rate_limit_tracker_format_rate_limit_display(c);
        if (strcmp(op, "agent_rate_limit_tracker_format_rate_limit_compact") == 0) o = emit_agent_rate_limit_tracker_format_rate_limit_compact(c);
        if (strcmp(op, "agent_credential_pool_u_normalize_pool_auth_type") == 0) o = emit_agent_credential_pool_u_normalize_pool_auth_type(c);
        if (strcmp(op, "agent_credential_pool_credential_pool_matches_provider") == 0) o = emit_agent_credential_pool_credential_pool_matches_provider(c);
        if (strcmp(op, "agent_credential_pool_u_current_unlocked") == 0) o = emit_agent_credential_pool_u_current_unlocked(c);
        if (strcmp(op, "agent_credential_pool_entry_id_for_api_key") == 0) o = emit_agent_credential_pool_entry_id_for_api_key(c);
        if (strcmp(op, "agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store") == 0) o = emit_agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store(c);
        if (strcmp(op, "agent_credential_pool_u_single_use_refresh_lock_timeout") == 0) o = emit_agent_credential_pool_u_single_use_refresh_lock_timeout(c);
        if (strcmp(op, "agent_credential_pool_u_codex_quota_restored_upstream") == 0) o = emit_agent_credential_pool_u_codex_quota_restored_upstream(c);
        if (strcmp(op, "agent_credential_pool_u_log_no_available_entries") == 0) o = emit_agent_credential_pool_u_log_no_available_entries(c);
        if (strcmp(op, "agent_credential_pool_try_refresh_matching") == 0) o = emit_agent_credential_pool_try_refresh_matching(c);
        if (strcmp(op, "agent_turn_context_compose_user_api_content") == 0) o = emit_agent_turn_context_compose_user_api_content(c);
        if (strcmp(op, "agent_turn_context_substitute_api_content") == 0) o = emit_agent_turn_context_substitute_api_content(c);
        if (strcmp(op, "agent_turn_context_drop_stale_api_content") == 0) o = emit_agent_turn_context_drop_stale_api_content(c);
        if (strcmp(op, "agent_turn_context_extract_api_content_sidecar") == 0) o = emit_agent_turn_context_extract_api_content_sidecar(c);
        if (strcmp(op, "agent_turn_context_consume_gateway_turn_context_notes") == 0) o = emit_agent_turn_context_consume_gateway_turn_context_notes(c);
        if (strcmp(op, "agent_turn_context_append_notes_to_multimodal_content") == 0) o = emit_agent_turn_context_append_notes_to_multimodal_content(c);
        if (strcmp(op, "agent_turn_context_reanchor_current_turn_user_idx") == 0) o = emit_agent_turn_context_reanchor_current_turn_user_idx(c);
        if (strcmp(op, "agent_turn_context_u_compression_warrants_another_preflight__ss") == 0) o = emit_agent_turn_context_u_compression_warrants_another_preflight__ss(c);
        if (strcmp(op, "agent_turn_context_u_should_idle_compact") == 0) o = emit_agent_turn_context_u_should_idle_compact(c);
        if (strcmp(op, "agent_trace_upload_u_now_iso") == 0) o = emit_agent_trace_upload_u_now_iso(c);
        if (strcmp(op, "agent_trace_upload_u_content_to_blocks") == 0) o = emit_agent_trace_upload_u_content_to_blocks(c);
        if (strcmp(op, "agent_trace_upload_u_tool_calls_to_blocks") == 0) o = emit_agent_trace_upload_u_tool_calls_to_blocks(c);
        if (strcmp(op, "agent_trace_upload_build_trace_jsonl") == 0) o = emit_agent_trace_upload_build_trace_jsonl(c);
        if (strcmp(op, "agent_trace_upload_u_resolve_hf_token") == 0) o = emit_agent_trace_upload_u_resolve_hf_token(c);
        if (strcmp(op, "agent_trace_upload_u_do_upload") == 0) o = emit_agent_trace_upload_u_do_upload(c);
        if (strcmp(op, "agent_trace_upload_load_session_messages") == 0) o = emit_agent_trace_upload_load_session_messages(c);
        if (strcmp(op, "agent_trace_upload_upload_session_trace") == 0) o = emit_agent_trace_upload_upload_session_trace(c);
        if (strcmp(op, "agent_context_engine_sanitize_memory_context") == 0) o = emit_agent_context_engine_sanitize_memory_context(c);
        if (strcmp(op, "agent_context_engine_automatic_compaction_status_message") == 0) o = emit_agent_context_engine_automatic_compaction_status_message(c);
        if (strcmp(op, "agent_context_engine_should_compress_info") == 0) o = emit_agent_context_engine_should_compress_info(c);
        if (strcmp(op, "agent_context_engine_prune_tool_results_only") == 0) o = emit_agent_context_engine_prune_tool_results_only(c);
        if (strcmp(op, "agent_context_engine_select_context") == 0) o = emit_agent_context_engine_select_context(c);
        if (strcmp(op, "agent_context_engine_on_turn_complete") == 0) o = emit_agent_context_engine_on_turn_complete(c);
        if (strcmp(op, "agent_context_engine_get_automatic_compaction_status_message") == 0) o = emit_agent_context_engine_get_automatic_compaction_status_message(c);
        if (strcmp(op, "agent_error_classifier_classify_api_error") == 0) o = emit_agent_error_classifier_classify_api_error(c);
        if (strcmp(op, "agent_error_classifier_u_classify_by_status") == 0) o = emit_agent_error_classifier_u_classify_by_status(c);
        if (strcmp(op, "agent_error_classifier_u_classify_402") == 0) o = emit_agent_error_classifier_u_classify_402(c);
        if (strcmp(op, "agent_error_classifier_u_classify_400") == 0) o = emit_agent_error_classifier_u_classify_400(c);
        if (strcmp(op, "agent_error_classifier_u_classify_by_error_code") == 0) o = emit_agent_error_classifier_u_classify_by_error_code(c);
        if (strcmp(op, "agent_error_classifier_u_classify_by_message") == 0) o = emit_agent_error_classifier_u_classify_by_message(c);
        if (strcmp(op, "agent_error_classifier_u_extract_error_code") == 0) o = emit_agent_error_classifier_u_extract_error_code(c);
        if (strcmp(op, "agent_memory_provider_queue_prefetch") == 0) o = emit_agent_memory_provider_queue_prefetch(c);
        if (strcmp(op, "agent_memory_provider_sync_turn") == 0) o = emit_agent_memory_provider_sync_turn(c);
        if (strcmp(op, "agent_memory_provider_on_turn_start") == 0) o = emit_agent_memory_provider_on_turn_start(c);
        if (strcmp(op, "agent_memory_provider_on_session_switch") == 0) o = emit_agent_memory_provider_on_session_switch(c);
        if (strcmp(op, "agent_memory_provider_on_pre_compress") == 0) o = emit_agent_memory_provider_on_pre_compress(c);
        if (strcmp(op, "agent_memory_provider_on_delegation") == 0) o = emit_agent_memory_provider_on_delegation(c);
        if (strcmp(op, "agent_memory_provider_on_memory_write") == 0) o = emit_agent_memory_provider_on_memory_write(c);
        if (strcmp(op, "agent_codex_runtime_u_codex_item_to_tool_name") == 0) o = emit_agent_codex_runtime_u_codex_item_to_tool_name(c);
        if (strcmp(op, "agent_codex_runtime_u_codex_item_to_args") == 0) o = emit_agent_codex_runtime_u_codex_item_to_args(c);
        if (strcmp(op, "agent_codex_runtime_u_codex_item_to_preview") == 0) o = emit_agent_codex_runtime_u_codex_item_to_preview(c);
        if (strcmp(op, "agent_codex_runtime_u_codex_item_completion_payload") == 0) o = emit_agent_codex_runtime_u_codex_item_completion_payload(c);
        if (strcmp(op, "agent_codex_runtime_make_codex_app_server_event_bridge") == 0) o = emit_agent_codex_runtime_make_codex_app_server_event_bridge(c);
        if (strcmp(op, "agent_codex_runtime_u_item_field") == 0) o = emit_agent_codex_runtime_u_item_field(c);
        if (strcmp(op, "agent_conversation_loop_u_apply_active_turn_redirect") == 0) o = emit_agent_conversation_loop_u_apply_active_turn_redirect(c);
        if (strcmp(op, "agent_conversation_loop_u_billing_block_dict") == 0) o = emit_agent_conversation_loop_u_billing_block_dict(c);
        if (strcmp(op, "agent_conversation_loop_u_invalid_tool_name_error_content") == 0) o = emit_agent_conversation_loop_u_invalid_tool_name_error_content(c);
        if (strcmp(op, "agent_conversation_loop_u_compression_deferred_result") == 0) o = emit_agent_conversation_loop_u_compression_deferred_result(c);
        if (strcmp(op, "agent_conversation_loop_u_apply_context_engine_selection") == 0) o = emit_agent_conversation_loop_u_apply_context_engine_selection(c);
        if (strcmp(op, "agent_conversation_loop_u_notify_context_engine_turn_complete") == 0) o = emit_agent_conversation_loop_u_notify_context_engine_turn_complete(c);
        if (strcmp(op, "agent_pet_generate_imagegen_u_forced_provider_from_env") == 0) o = emit_agent_pet_generate_imagegen_u_forced_provider_from_env(c);
        if (strcmp(op, "agent_pet_generate_imagegen_resolve_provider") == 0) o = emit_agent_pet_generate_imagegen_resolve_provider(c);
        if (strcmp(op, "agent_pet_generate_imagegen_list_sprite_providers") == 0) o = emit_agent_pet_generate_imagegen_list_sprite_providers(c);
        if (strcmp(op, "agent_pet_generate_imagegen_u_save_local") == 0) o = emit_agent_pet_generate_imagegen_u_save_local(c);
        if (strcmp(op, "agent_pet_generate_imagegen_u_rejected_background") == 0) o = emit_agent_pet_generate_imagegen_u_rejected_background(c);
        if (strcmp(op, "agent_pet_generate_orchestrate_u_harden_transparency") == 0) o = emit_agent_pet_generate_orchestrate_u_harden_transparency(c);
        if (strcmp(op, "agent_pet_generate_orchestrate_generate_base_drafts") == 0) o = emit_agent_pet_generate_orchestrate_generate_base_drafts(c);
        if (strcmp(op, "agent_pet_generate_orchestrate_u_drafts_failed_reason") == 0) o = emit_agent_pet_generate_orchestrate_u_drafts_failed_reason(c);
        if (strcmp(op, "agent_pet_generate_orchestrate_u_humanize_image_error") == 0) o = emit_agent_pet_generate_orchestrate_u_humanize_image_error(c);
        if (strcmp(op, "agent_pet_generate_orchestrate_hatch_pet") == 0) o = emit_agent_pet_generate_orchestrate_hatch_pet(c);
        if (strcmp(op, "agent_account_usage_u_codex_backend_urls") == 0) o = emit_agent_account_usage_u_codex_backend_urls(c);
        if (strcmp(op, "agent_account_usage_u_resolve_codex_usage_credentials") == 0) o = emit_agent_account_usage_u_resolve_codex_usage_credentials(c);
        if (strcmp(op, "agent_account_usage_redeemed") == 0) o = emit_agent_account_usage_redeemed(c);
        if (strcmp(op, "agent_account_usage_redeem_codex_reset_credit") == 0) o = emit_agent_account_usage_redeem_codex_reset_credit(c);
        if (strcmp(op, "agent_bedrock_adapter_u_model_supports_prompt_cache") == 0) o = emit_agent_bedrock_adapter_u_model_supports_prompt_cache(c);
        if (strcmp(op, "agent_bedrock_adapter_u_safe_text") == 0) o = emit_agent_bedrock_adapter_u_safe_text(c);
        if (strcmp(op, "agent_bedrock_adapter_u_static_bedrock_context_length") == 0) o = emit_agent_bedrock_adapter_u_static_bedrock_context_length(c);
        if (strcmp(op, "agent_bedrock_adapter_probe_bedrock_context_length") == 0) o = emit_agent_bedrock_adapter_probe_bedrock_context_length(c);
        if (strcmp(op, "agent_kanban_stop_kanban_stop_nudge_enabled") == 0) o = emit_agent_kanban_stop_kanban_stop_nudge_enabled(c);
        if (strcmp(op, "agent_kanban_stop_u_tool_call_name") == 0) o = emit_agent_kanban_stop_u_tool_call_name(c);
        if (strcmp(op, "agent_kanban_stop_session_called_kanban_terminal") == 0) o = emit_agent_kanban_stop_session_called_kanban_terminal(c);
        if (strcmp(op, "agent_kanban_stop_build_kanban_stop_nudge") == 0) o = emit_agent_kanban_stop_build_kanban_stop_nudge(c);
        if (strcmp(op, "agent_thread_scoped_output_unsilence") == 0) o = emit_agent_thread_scoped_output_unsilence(c);
        if (strcmp(op, "agent_thread_scoped_output_writelines") == 0) o = emit_agent_thread_scoped_output_writelines(c);
        if (strcmp(op, "agent_thread_scoped_output_u__getattr__") == 0) o = emit_agent_thread_scoped_output_u__getattr__(c);
        if (strcmp(op, "agent_thread_scoped_output_thread_scoped_silence") == 0) o = emit_agent_thread_scoped_output_thread_scoped_silence(c);
        if (strcmp(op, "agent_agent_runtime_helpers_note_turn_start") == 0) o = emit_agent_agent_runtime_helpers_note_turn_start(c);
        if (strcmp(op, "agent_agent_runtime_helpers_note_turn_persisted") == 0) o = emit_agent_agent_runtime_helpers_note_turn_persisted(c);
        if (strcmp(op, "agent_agent_runtime_helpers_sync_credential_pool_entry_id") == 0) o = emit_agent_agent_runtime_helpers_sync_credential_pool_entry_id(c);
        if (strcmp(op, "agent_anthropic_adapter_u_get_hermes_oauth_file") == 0) o = emit_agent_anthropic_adapter_u_get_hermes_oauth_file(c);
        if (strcmp(op, "agent_anthropic_adapter_u_safe_text") == 0) o = emit_agent_anthropic_adapter_u_safe_text(c);
        if (strcmp(op, "agent_anthropic_adapter_u_ensure_leading_user_turn") == 0) o = emit_agent_anthropic_adapter_u_ensure_leading_user_turn(c);
        if (strcmp(op, "agent_battery_u_read_battery_uncached") == 0) o = emit_agent_battery_u_read_battery_uncached(c);
        if (strcmp(op, "agent_battery_read_battery") == 0) o = emit_agent_battery_read_battery(c);
        if (strcmp(op, "agent_battery_clear_cache") == 0) o = emit_agent_battery_clear_cache(c);
        if (strcmp(op, "agent_billing_view_can_change_plan") == 0) o = emit_agent_billing_view_can_change_plan(c);
        if (strcmp(op, "agent_billing_view_u_parse_auto_reload_card") == 0) o = emit_agent_billing_view_u_parse_auto_reload_card(c);
        if (strcmp(op, "agent_billing_view_u_dev_fixture_billing_state") == 0) o = emit_agent_billing_view_u_dev_fixture_billing_state(c);
        if (strcmp(op, "agent_bounded_response_read_streaming_error_body") == 0) o = emit_agent_bounded_response_read_streaming_error_body(c);
        if (strcmp(op, "agent_bounded_response_u_safe_close") == 0) o = emit_agent_bounded_response_u_safe_close(c);
        if (strcmp(op, "agent_bounded_response_read_error_body_or_default") == 0) o = emit_agent_bounded_response_read_error_body_or_default(c);
        if (strcmp(op, "agent_display_u_display_url") == 0) o = emit_agent_display_u_display_url(c);
        if (strcmp(op, "agent_display_build_status_phrase") == 0) o = emit_agent_display_build_status_phrase(c);
        if (strcmp(op, "agent_display_u_get_cute_tool_message") == 0) o = emit_agent_display_u_get_cute_tool_message(c);
        if (strcmp(op, "agent_moa_trace_u_traces_enabled_and_dir") == 0) o = emit_agent_moa_trace_u_traces_enabled_and_dir(c);
        if (strcmp(op, "agent_moa_trace_u_slot_trace") == 0) o = emit_agent_moa_trace_u_slot_trace(c);
        if (strcmp(op, "agent_moa_trace_save_moa_turn") == 0) o = emit_agent_moa_trace_save_moa_turn(c);
        if (strcmp(op, "agent_oneshot_u_commit_message_template") == 0) o = emit_agent_oneshot_u_commit_message_template(c);
        if (strcmp(op, "agent_oneshot_render_template") == 0) o = emit_agent_oneshot_render_template(c);
        if (strcmp(op, "agent_oneshot_run_oneshot") == 0) o = emit_agent_oneshot_run_oneshot(c);
        if (strcmp(op, "agent_tool_executor_u_ensure_file_checkpoint") == 0) o = emit_agent_tool_executor_u_ensure_file_checkpoint(c);
        if (strcmp(op, "agent_tool_executor_u_parse_tool_arguments") == 0) o = emit_agent_tool_executor_u_parse_tool_arguments(c);
        if (strcmp(op, "agent_tool_executor_execute_tool_calls_segmented") == 0) o = emit_agent_tool_executor_execute_tool_calls_segmented(c);
        if (strcmp(op, "agent_auxiliary_client_u_try_nvidia_nim") == 0) o = emit_agent_auxiliary_client_u_try_nvidia_nim(c);
        if (strcmp(op, "agent_auxiliary_client_u_obj_get") == 0) o = emit_agent_auxiliary_client_u_obj_get(c);
        if (strcmp(op, "agent_billing_usage_build_usage_model") == 0) o = emit_agent_billing_usage_build_usage_model(c);
        if (strcmp(op, "agent_billing_usage_u_dev_fixture_usage_model") == 0) o = emit_agent_billing_usage_u_dev_fixture_usage_model(c);
        if (strcmp(op, "agent_credits_tracker_has_data") == 0) o = emit_agent_credits_tracker_has_data(c);
        if (strcmp(op, "agent_credits_tracker_age_seconds") == 0) o = emit_agent_credits_tracker_age_seconds(c);
        if (strcmp(op, "agent_prompt_caching_u_can_carry_marker") == 0) o = emit_agent_prompt_caching_u_can_carry_marker(c);
        if (strcmp(op, "agent_prompt_caching_u_apply_system_cache_markers") == 0) o = emit_agent_prompt_caching_u_apply_system_cache_markers(c);
        if (strcmp(op, "agent_redact_u_canonical_url_param_name") == 0) o = emit_agent_redact_u_canonical_url_param_name(c);
        if (strcmp(op, "agent_redact_u_redact_strict_url_credentials") == 0) o = emit_agent_redact_u_redact_strict_url_credentials(c);
        if (strcmp(op, "agent_skill_commands_split_stacked_skill_commands") == 0) o = emit_agent_skill_commands_split_stacked_skill_commands(c);
        if (strcmp(op, "agent_skill_commands_build_stacked_skill_invocation_message") == 0) o = emit_agent_skill_commands_build_stacked_skill_invocation_message(c);
        if (strcmp(op, "agent_stream_single_writer_claim_stream_writer") == 0) o = emit_agent_stream_single_writer_claim_stream_writer(c);
        if (strcmp(op, "agent_stream_single_writer_stream_writer_is_current") == 0) o = emit_agent_stream_single_writer_stream_writer_is_current(c);
        if (strcmp(op, "agent_turn_finalizer_u_is_pure_tool_call_tail") == 0) o = emit_agent_turn_finalizer_u_is_pure_tool_call_tail(c);
        if (strcmp(op, "agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng") == 0) o = emit_agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng(c);
        if (strcmp(op, "agent_video_gen_provider_save_url_video") == 0) o = emit_agent_video_gen_provider_save_url_video(c);
        if (strcmp(op, "agent_video_gen_provider_u_create_and_poll") == 0) o = emit_agent_video_gen_provider_u_create_and_poll(c);
        if (strcmp(op, "agent_context_references_format_reference_value") == 0) o = emit_agent_context_references_format_reference_value(c);
        if (strcmp(op, "agent_credential_sources_u_remove_xai_oauth_device_code") == 0) o = emit_agent_credential_sources_u_remove_xai_oauth_device_code(c);
        if (strcmp(op, "agent_insights_u_get_model_usage") == 0) o = emit_agent_insights_u_get_model_usage(c);
        if (strcmp(op, "agent_jiter_preload_preload_jiter_native_extension") == 0) o = emit_agent_jiter_preload_preload_jiter_native_extension(c);
        if (strcmp(op, "agent_moonshot_schema_u_ensure_required_array") == 0) o = emit_agent_moonshot_schema_u_ensure_required_array(c);
        if (strcmp(op, "agent_runtime_cwd_u_is_install_tree") == 0) o = emit_agent_runtime_cwd_u_is_install_tree(c);
        if (strcmp(op, "agent_system_prompt_u_tui_embedded_pane_clarifier") == 0) o = emit_agent_system_prompt_u_tui_embedded_pane_clarifier(c);
        if (strcmp(op, "agent_tool_result_classificati_tool_may_have_side_effect") == 0) o = emit_agent_tool_result_classificati_tool_may_have_side_effect(c);
        if (strcmp(op, "agent_web_search_provider_get_provider_env") == 0) o = emit_agent_web_search_provider_get_provider_env(c);
        if (strcmp(op, "agent_web_search_registry_u_disabled_web_plugin_for") == 0) o = emit_agent_web_search_registry_u_disabled_web_plugin_for(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
