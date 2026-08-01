"""AUTO-GENERATED integration oracle for port_agent_remaining_wrappers (gen_integration_oracle.py)."""
import os, sys, json, importlib.util

MODS = {}
def _load(rel):
    _repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    if _repo not in sys.path: sys.path.insert(0, _repo)
    for base in sys.path:
        cand = os.path.join(base, rel)
        try:
            spec = importlib.util.spec_from_file_location('live_' + rel.replace('/', '_').replace('.', '_'), cand)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            return mod
        except Exception:
            continue
    return None
MODS['agent.account_usage'] = _load('agent/account_usage.py')
MODS['agent.agent_init'] = _load('agent/agent_init.py')
MODS['agent.agent_runtime_helpers'] = _load('agent/agent_runtime_helpers.py')
MODS['agent.anthropic_adapter'] = _load('agent/anthropic_adapter.py')
MODS['agent.auxiliary_client'] = _load('agent/auxiliary_client.py')
MODS['agent.battery'] = _load('agent/battery.py')
MODS['agent.bedrock_adapter'] = _load('agent/bedrock_adapter.py')
MODS['agent.billing_usage'] = _load('agent/billing_usage.py')
MODS['agent.billing_view'] = _load('agent/billing_view.py')
MODS['agent.bounded_response'] = _load('agent/bounded_response.py')
MODS['agent.chat_completion_helpers'] = _load('agent/chat_completion_helpers.py')
MODS['agent.codex_runtime'] = _load('agent/codex_runtime.py')
MODS['agent.context_engine'] = _load('agent/context_engine.py')
MODS['agent.context_references'] = _load('agent/context_references.py')
MODS['agent.conversation_loop'] = _load('agent/conversation_loop.py')
MODS['agent.credential_pool'] = _load('agent/credential_pool.py')
MODS['agent.credential_sources'] = _load('agent/credential_sources.py')
MODS['agent.credits_tracker'] = _load('agent/credits_tracker.py')
MODS['agent.display'] = _load('agent/display.py')
MODS['agent.error_classifier'] = _load('agent/error_classifier.py')
MODS['agent.insights'] = _load('agent/insights.py')
MODS['agent.jiter_preload'] = _load('agent/jiter_preload.py')
MODS['agent.kanban_stop'] = _load('agent/kanban_stop.py')
MODS['agent.memory_provider'] = _load('agent/memory_provider.py')
MODS['agent.moa_trace'] = _load('agent/moa_trace.py')
MODS['agent.model_metadata'] = _load('agent/model_metadata.py')
MODS['agent.moonshot_schema'] = _load('agent/moonshot_schema.py')
MODS['agent.oneshot'] = _load('agent/oneshot.py')
MODS['agent.pet.generate.atlas'] = _load('agent/pet/generate/atlas.py')
MODS['agent.pet.generate.imagegen'] = _load('agent/pet/generate/imagegen.py')
MODS['agent.pet.generate.orchestrate'] = _load('agent/pet/generate/orchestrate.py')
MODS['agent.prompt_caching'] = _load('agent/prompt_caching.py')
MODS['agent.rate_limit_tracker'] = _load('agent/rate_limit_tracker.py')
MODS['agent.redact'] = _load('agent/redact.py')
MODS['agent.runtime_cwd'] = _load('agent/runtime_cwd.py')
MODS['agent.skill_commands'] = _load('agent/skill_commands.py')
MODS['agent.stream_single_writer'] = _load('agent/stream_single_writer.py')
MODS['agent.subscription_view'] = _load('agent/subscription_view.py')
MODS['agent.system_prompt'] = _load('agent/system_prompt.py')
MODS['agent.thread_scoped_output'] = _load('agent/thread_scoped_output.py')
MODS['agent.tool_dispatch_helpers'] = _load('agent/tool_dispatch_helpers.py')
MODS['agent.tool_executor'] = _load('agent/tool_executor.py')
MODS['agent.tool_result_classification'] = _load('agent/tool_result_classification.py')
MODS['agent.trace_upload'] = _load('agent/trace_upload.py')
MODS['agent.turn_context'] = _load('agent/turn_context.py')
MODS['agent.turn_finalizer'] = _load('agent/turn_finalizer.py')
MODS['agent.video_gen_provider'] = _load('agent/video_gen_provider.py')
MODS['agent.web_search_provider'] = _load('agent/web_search_provider.py')
MODS['agent.web_search_registry'] = _load('agent/web_search_registry.py')

DISPATCH = {
    'agent_model_metadata_u_endpoint_scoped_context_length': ('agent.model_metadata', '_endpoint_scoped_context_length'),
    'agent_model_metadata_u_skip_persistent_context_cache': ('agent.model_metadata', '_skip_persistent_context_cache'),
    'agent_model_metadata_u_maybe_cache_local_context_length': ('agent.model_metadata', '_maybe_cache_local_context_length'),
    'agent_model_metadata_u_reconcile_local_cached_context_length': ('agent.model_metadata', '_reconcile_local_cached_context_length'),
    'agent_model_metadata_u_localhost_to_ipv4': ('agent.model_metadata', '_localhost_to_ipv4'),
    'agent_model_metadata_u_context_cache_key': ('agent.model_metadata', '_context_cache_key'),
    'agent_model_metadata_u_query_ollama_api_show_uncached': ('agent.model_metadata', '_query_ollama_api_show_uncached'),
    'agent_model_metadata_u_query_local_context_length_uncached': ('agent.model_metadata', '_query_local_context_length_uncached'),
    'agent_model_metadata_u_codex_oauth_token_fingerprint': ('agent.model_metadata', '_codex_oauth_token_fingerprint'),
    'agent_model_metadata_u_extract_chatgpt_account_id': ('agent.model_metadata', '_extract_chatgpt_account_id'),
    'agent_model_metadata_u_fetch_codex_oauth_context_lengths_wit_ce': ('agent.model_metadata', '_fetch_codex_oauth_context_lengths_with_source'),
    'agent_model_metadata_u_resolve_codex_oauth_context_length_wi_ce': ('agent.model_metadata', '_resolve_codex_oauth_context_length_with_source'),
    'agent_model_metadata_u_is_cjk_token_dense_char': ('agent.model_metadata', '_is_cjk_token_dense_char'),
    'agent_model_metadata_u_estimate_message_tokens_without_images': ('agent.model_metadata', '_estimate_message_tokens_without_images'),
    'agent_model_metadata_u_tool_name_for_cache': ('agent.model_metadata', '_tool_name_for_cache'),
    'agent_model_metadata_u_estimate_tools_tokens_rough': ('agent.model_metadata', '_estimate_tools_tokens_rough'),
    'agent_pet_generate_atlas_u_has_slot_padding': ('agent.pet.generate.atlas', '_has_slot_padding'),
    'agent_pet_generate_atlas_u_slot_bounds': ('agent.pet.generate.atlas', '_slot_bounds'),
    'agent_pet_generate_atlas_u_component_crops': ('agent.pet.generate.atlas', '_component_crops'),
    'agent_pet_generate_atlas_u_sever_expected_gutters': ('agent.pet.generate.atlas', '_sever_expected_gutters'),
    'agent_pet_generate_atlas_u_slot_crops': ('agent.pet.generate.atlas', '_slot_crops'),
    'agent_pet_generate_atlas_u_frame_x_ranges': ('agent.pet.generate.atlas', '_frame_x_ranges'),
    'agent_pet_generate_atlas_u_significant_subject_boxes': ('agent.pet.generate.atlas', '_significant_subject_boxes'),
    'agent_pet_generate_atlas_u_validate_extracted_frames': ('agent.pet.generate.atlas', '_validate_extracted_frames'),
    'agent_pet_generate_atlas_extract_strip_frames': ('agent.pet.generate.atlas', 'extract_strip_frames'),
    'agent_pet_generate_atlas_normalize_cells': ('agent.pet.generate.atlas', 'normalize_cells'),
    'agent_pet_generate_atlas_single_frame': ('agent.pet.generate.atlas', 'single_frame'),
    'agent_pet_generate_atlas_u_clear_transparent_rgb': ('agent.pet.generate.atlas', '_clear_transparent_rgb'),
    'agent_pet_generate_atlas_mirror_frames': ('agent.pet.generate.atlas', 'mirror_frames'),
    'agent_pet_generate_atlas_compose_atlas': ('agent.pet.generate.atlas', 'compose_atlas'),
    'agent_pet_generate_atlas_atlas_to_webp_bytes': ('agent.pet.generate.atlas', 'atlas_to_webp_bytes'),
    'agent_pet_generate_atlas_validate_atlas': ('agent.pet.generate.atlas', 'validate_atlas'),
    'agent_tool_dispatch_helpers_u_is_destructive_command': ('agent.tool_dispatch_helpers', '_is_destructive_command'),
    'agent_tool_dispatch_helpers_u_is_mcp_tool_parallel_safe': ('agent.tool_dispatch_helpers', '_is_mcp_tool_parallel_safe'),
    'agent_tool_dispatch_helpers_u_plan_tool_batch_segments': ('agent.tool_dispatch_helpers', '_plan_tool_batch_segments'),
    'agent_tool_dispatch_helpers_u_should_parallelize_tool_batch': ('agent.tool_dispatch_helpers', '_should_parallelize_tool_batch'),
    'agent_tool_dispatch_helpers_u_extract_parallel_scope_path': ('agent.tool_dispatch_helpers', '_extract_parallel_scope_path'),
    'agent_tool_dispatch_helpers_u_paths_overlap': ('agent.tool_dispatch_helpers', '_paths_overlap'),
    'agent_tool_dispatch_helpers_u_is_multimodal_tool_result': ('agent.tool_dispatch_helpers', '_is_multimodal_tool_result'),
    'agent_tool_dispatch_helpers_u_multimodal_text_summary': ('agent.tool_dispatch_helpers', '_multimodal_text_summary'),
    'agent_tool_dispatch_helpers_u_append_subdir_hint_to_multimodal': ('agent.tool_dispatch_helpers', '_append_subdir_hint_to_multimodal'),
    'agent_tool_dispatch_helpers_u_extract_file_mutation_targets': ('agent.tool_dispatch_helpers', '_extract_file_mutation_targets'),
    'agent_tool_dispatch_helpers_u_extract_error_preview': ('agent.tool_dispatch_helpers', '_extract_error_preview'),
    'agent_tool_dispatch_helpers_u_trajectory_normalize_msg': ('agent.tool_dispatch_helpers', '_trajectory_normalize_msg'),
    'agent_tool_dispatch_helpers_make_tool_result_message': ('agent.tool_dispatch_helpers', 'make_tool_result_message'),
    'agent_tool_dispatch_helpers_u_is_untrusted_tool': ('agent.tool_dispatch_helpers', '_is_untrusted_tool'),
    'agent_tool_dispatch_helpers_u_tool_output_risk_metadata': ('agent.tool_dispatch_helpers', '_tool_output_risk_metadata'),
    'agent_tool_dispatch_helpers_u_maybe_wrap_untrusted': ('agent.tool_dispatch_helpers', '_maybe_wrap_untrusted'),
    'agent_subscription_view_can_change_plan': ('agent.subscription_view', 'can_change_plan'),
    'agent_subscription_view_u_parse_current': ('agent.subscription_view', '_parse_current'),
    'agent_subscription_view_u_coalesce': ('agent.subscription_view', '_coalesce'),
    'agent_subscription_view_u_parse_tier': ('agent.subscription_view', '_parse_tier'),
    'agent_subscription_view_subscription_change_preview_from_pay_ad': ('agent.subscription_view', 'subscription_change_preview_from_payload'),
    'agent_subscription_view_subscription_state_from_payload': ('agent.subscription_view', 'subscription_state_from_payload'),
    'agent_subscription_view_build_subscription_state': ('agent.subscription_view', 'build_subscription_state'),
    'agent_subscription_view_subscription_manage_url': ('agent.subscription_view', 'subscription_manage_url'),
    'agent_subscription_view_u_format_dollars_grouped': ('agent.subscription_view', '_format_dollars_grouped'),
    'agent_subscription_view_selectable_tiers': ('agent.subscription_view', 'selectable_tiers'),
    'agent_subscription_view_format_tier_row': ('agent.subscription_view', 'format_tier_row'),
    'agent_subscription_view_is_upgrade': ('agent.subscription_view', 'is_upgrade'),
    'agent_subscription_view_u_dev_current': ('agent.subscription_view', '_dev_current'),
    'agent_subscription_view_u_dev_tiers': ('agent.subscription_view', '_dev_tiers'),
    'agent_subscription_view_dev_fixture_subscription_state': ('agent.subscription_view', 'dev_fixture_subscription_state'),
    'agent_chat_completion_helpers_openai_codex_stale_timeout_floor': ('agent.chat_completion_helpers', 'openai_codex_stale_timeout_floor'),
    'agent_chat_completion_helpers_u_provider_preferences_for_agent': ('agent.chat_completion_helpers', '_provider_preferences_for_agent'),
    'agent_chat_completion_helpers_u_codex_wait_notice_recovery': ('agent.chat_completion_helpers', '_codex_wait_notice_recovery'),
    'agent_chat_completion_helpers_u_stale_streak': ('agent.chat_completion_helpers', '_stale_streak'),
    'agent_chat_completion_helpers_u_bump_stale_streak': ('agent.chat_completion_helpers', '_bump_stale_streak'),
    'agent_chat_completion_helpers_u_reset_stale_streak': ('agent.chat_completion_helpers', '_reset_stale_streak'),
    'agent_chat_completion_helpers_u_check_stale_giveup': ('agent.chat_completion_helpers', '_check_stale_giveup'),
    'agent_chat_completion_helpers_u_derive_stream_stale_timeout': ('agent.chat_completion_helpers', '_derive_stream_stale_timeout'),
    'agent_chat_completion_helpers_u_bedrock_reasoning_stale_floor': ('agent.chat_completion_helpers', '_bedrock_reasoning_stale_floor'),
    'agent_chat_completion_helpers_u_dispatch_nonstreaming_api_re_st': ('agent.chat_completion_helpers', '_dispatch_nonstreaming_api_request'),
    'agent_chat_completion_helpers_should_use_direct_api_call': ('agent.chat_completion_helpers', 'should_use_direct_api_call'),
    'agent_chat_completion_helpers_direct_api_call': ('agent.chat_completion_helpers', 'direct_api_call'),
    'agent_chat_completion_helpers_u_fallback_entry_is_same_backe_rl': ('agent.chat_completion_helpers', '_fallback_entry_is_same_backend_by_base_url'),
    'agent_chat_completion_helpers_u_build_partial_stream_stub': ('agent.chat_completion_helpers', '_build_partial_stream_stub'),
    'agent_agent_init_u_moa_reference_output_allowed': ('agent.agent_init', '_moa_reference_output_allowed'),
    'agent_agent_init_u_relay_moa_reference_event': ('agent.agent_init', '_relay_moa_reference_event'),
    'agent_agent_init_u_provider_default_routes': ('agent.agent_init', '_provider_default_routes'),
    'agent_agent_init_u_context_route_mismatch': ('agent.agent_init', '_context_route_mismatch'),
    'agent_agent_init_u_normalize_custom_provider_name': ('agent.agent_init', '_normalize_custom_provider_name'),
    'agent_agent_init_u_custom_provider_runtime_ids': ('agent.agent_init', '_custom_provider_runtime_ids'),
    'agent_agent_init_u_build_codex_gpt5_autoraise_notice': ('agent.agent_init', '_build_codex_gpt5_autoraise_notice'),
    'agent_agent_init_u_resolve_compression_threshold': ('agent.agent_init', '_resolve_compression_threshold'),
    'agent_agent_init_u_codex_gpt55_autoraise_notice_marker': ('agent.agent_init', '_codex_gpt55_autoraise_notice_marker'),
    'agent_agent_init_u_codex_gpt55_autoraise_notice_state': ('agent.agent_init', '_codex_gpt55_autoraise_notice_state'),
    'agent_agent_init_u_codex_gpt55_autoraise_notice_seen': ('agent.agent_init', '_codex_gpt55_autoraise_notice_seen'),
    'agent_agent_init_u_record_codex_gpt55_autoraise_notice': ('agent.agent_init', '_record_codex_gpt55_autoraise_notice'),
    'agent_rate_limit_tracker_usage_pct': ('agent.rate_limit_tracker', 'usage_pct'),
    'agent_rate_limit_tracker_remaining_seconds_now': ('agent.rate_limit_tracker', 'remaining_seconds_now'),
    'agent_rate_limit_tracker_has_data': ('agent.rate_limit_tracker', 'has_data'),
    'agent_rate_limit_tracker_age_seconds': ('agent.rate_limit_tracker', 'age_seconds'),
    'agent_rate_limit_tracker_u_safe_float': ('agent.rate_limit_tracker', '_safe_float'),
    'agent_rate_limit_tracker_parse_rate_limit_headers': ('agent.rate_limit_tracker', 'parse_rate_limit_headers'),
    'agent_rate_limit_tracker_u_fmt_count': ('agent.rate_limit_tracker', '_fmt_count'),
    'agent_rate_limit_tracker_u_fmt_seconds': ('agent.rate_limit_tracker', '_fmt_seconds'),
    'agent_rate_limit_tracker_u_bar': ('agent.rate_limit_tracker', '_bar'),
    'agent_rate_limit_tracker_u_bucket_line': ('agent.rate_limit_tracker', '_bucket_line'),
    'agent_rate_limit_tracker_format_rate_limit_display': ('agent.rate_limit_tracker', 'format_rate_limit_display'),
    'agent_rate_limit_tracker_format_rate_limit_compact': ('agent.rate_limit_tracker', 'format_rate_limit_compact'),
    'agent_credential_pool_u_normalize_pool_auth_type': ('agent.credential_pool', '_normalize_pool_auth_type'),
    'agent_credential_pool_credential_pool_matches_provider': ('agent.credential_pool', 'credential_pool_matches_provider'),
    'agent_credential_pool_u_current_unlocked': ('agent.credential_pool', '_current_unlocked'),
    'agent_credential_pool_entry_id_for_api_key': ('agent.credential_pool', 'entry_id_for_api_key'),
    'agent_credential_pool_u_sync_xai_oauth_entry_from_pool_store': ('agent.credential_pool', '_sync_xai_oauth_entry_from_pool_store'),
    'agent_credential_pool_u_single_use_refresh_lock_timeout': ('agent.credential_pool', '_single_use_refresh_lock_timeout'),
    'agent_credential_pool_u_codex_quota_restored_upstream': ('agent.credential_pool', '_codex_quota_restored_upstream'),
    'agent_credential_pool_u_log_no_available_entries': ('agent.credential_pool', '_log_no_available_entries'),
    'agent_credential_pool_try_refresh_matching': ('agent.credential_pool', 'try_refresh_matching'),
    'agent_turn_context_compose_user_api_content': ('agent.turn_context', 'compose_user_api_content'),
    'agent_turn_context_substitute_api_content': ('agent.turn_context', 'substitute_api_content'),
    'agent_turn_context_drop_stale_api_content': ('agent.turn_context', 'drop_stale_api_content'),
    'agent_turn_context_extract_api_content_sidecar': ('agent.turn_context', 'extract_api_content_sidecar'),
    'agent_turn_context_consume_gateway_turn_context_notes': ('agent.turn_context', 'consume_gateway_turn_context_notes'),
    'agent_turn_context_append_notes_to_multimodal_content': ('agent.turn_context', 'append_notes_to_multimodal_content'),
    'agent_turn_context_reanchor_current_turn_user_idx': ('agent.turn_context', 'reanchor_current_turn_user_idx'),
    'agent_turn_context_u_compression_warrants_another_preflight__ss': ('agent.turn_context', '_compression_warrants_another_preflight_pass'),
    'agent_turn_context_u_should_idle_compact': ('agent.turn_context', '_should_idle_compact'),
    'agent_trace_upload_u_now_iso': ('agent.trace_upload', '_now_iso'),
    'agent_trace_upload_u_content_to_blocks': ('agent.trace_upload', '_content_to_blocks'),
    'agent_trace_upload_u_tool_calls_to_blocks': ('agent.trace_upload', '_tool_calls_to_blocks'),
    'agent_trace_upload_build_trace_jsonl': ('agent.trace_upload', 'build_trace_jsonl'),
    'agent_trace_upload_u_resolve_hf_token': ('agent.trace_upload', '_resolve_hf_token'),
    'agent_trace_upload_u_do_upload': ('agent.trace_upload', '_do_upload'),
    'agent_trace_upload_load_session_messages': ('agent.trace_upload', 'load_session_messages'),
    'agent_trace_upload_upload_session_trace': ('agent.trace_upload', 'upload_session_trace'),
    'agent_context_engine_sanitize_memory_context': ('agent.context_engine', 'sanitize_memory_context'),
    'agent_context_engine_automatic_compaction_status_message': ('agent.context_engine', 'automatic_compaction_status_message'),
    'agent_context_engine_should_compress_info': ('agent.context_engine', 'should_compress_info'),
    'agent_context_engine_prune_tool_results_only': ('agent.context_engine', 'prune_tool_results_only'),
    'agent_context_engine_select_context': ('agent.context_engine', 'select_context'),
    'agent_context_engine_on_turn_complete': ('agent.context_engine', 'on_turn_complete'),
    'agent_context_engine_get_automatic_compaction_status_message': ('agent.context_engine', 'get_automatic_compaction_status_message'),
    'agent_error_classifier_classify_api_error': ('agent.error_classifier', 'classify_api_error'),
    'agent_error_classifier_u_classify_by_status': ('agent.error_classifier', '_classify_by_status'),
    'agent_error_classifier_u_classify_402': ('agent.error_classifier', '_classify_402'),
    'agent_error_classifier_u_classify_400': ('agent.error_classifier', '_classify_400'),
    'agent_error_classifier_u_classify_by_error_code': ('agent.error_classifier', '_classify_by_error_code'),
    'agent_error_classifier_u_classify_by_message': ('agent.error_classifier', '_classify_by_message'),
    'agent_error_classifier_u_extract_error_code': ('agent.error_classifier', '_extract_error_code'),
    'agent_memory_provider_queue_prefetch': ('agent.memory_provider', 'queue_prefetch'),
    'agent_memory_provider_sync_turn': ('agent.memory_provider', 'sync_turn'),
    'agent_memory_provider_on_turn_start': ('agent.memory_provider', 'on_turn_start'),
    'agent_memory_provider_on_session_switch': ('agent.memory_provider', 'on_session_switch'),
    'agent_memory_provider_on_pre_compress': ('agent.memory_provider', 'on_pre_compress'),
    'agent_memory_provider_on_delegation': ('agent.memory_provider', 'on_delegation'),
    'agent_memory_provider_on_memory_write': ('agent.memory_provider', 'on_memory_write'),
    'agent_codex_runtime_u_codex_item_to_tool_name': ('agent.codex_runtime', '_codex_item_to_tool_name'),
    'agent_codex_runtime_u_codex_item_to_args': ('agent.codex_runtime', '_codex_item_to_args'),
    'agent_codex_runtime_u_codex_item_to_preview': ('agent.codex_runtime', '_codex_item_to_preview'),
    'agent_codex_runtime_u_codex_item_completion_payload': ('agent.codex_runtime', '_codex_item_completion_payload'),
    'agent_codex_runtime_make_codex_app_server_event_bridge': ('agent.codex_runtime', 'make_codex_app_server_event_bridge'),
    'agent_codex_runtime_u_item_field': ('agent.codex_runtime', '_item_field'),
    'agent_conversation_loop_u_apply_active_turn_redirect': ('agent.conversation_loop', '_apply_active_turn_redirect'),
    'agent_conversation_loop_u_billing_block_dict': ('agent.conversation_loop', '_billing_block_dict'),
    'agent_conversation_loop_u_invalid_tool_name_error_content': ('agent.conversation_loop', '_invalid_tool_name_error_content'),
    'agent_conversation_loop_u_compression_deferred_result': ('agent.conversation_loop', '_compression_deferred_result'),
    'agent_conversation_loop_u_apply_context_engine_selection': ('agent.conversation_loop', '_apply_context_engine_selection'),
    'agent_conversation_loop_u_notify_context_engine_turn_complete': ('agent.conversation_loop', '_notify_context_engine_turn_complete'),
    'agent_pet_generate_imagegen_u_forced_provider_from_env': ('agent.pet.generate.imagegen', '_forced_provider_from_env'),
    'agent_pet_generate_imagegen_resolve_provider': ('agent.pet.generate.imagegen', 'resolve_provider'),
    'agent_pet_generate_imagegen_list_sprite_providers': ('agent.pet.generate.imagegen', 'list_sprite_providers'),
    'agent_pet_generate_imagegen_u_save_local': ('agent.pet.generate.imagegen', '_save_local'),
    'agent_pet_generate_imagegen_u_rejected_background': ('agent.pet.generate.imagegen', '_rejected_background'),
    'agent_pet_generate_orchestrate_u_harden_transparency': ('agent.pet.generate.orchestrate', '_harden_transparency'),
    'agent_pet_generate_orchestrate_generate_base_drafts': ('agent.pet.generate.orchestrate', 'generate_base_drafts'),
    'agent_pet_generate_orchestrate_u_drafts_failed_reason': ('agent.pet.generate.orchestrate', '_drafts_failed_reason'),
    'agent_pet_generate_orchestrate_u_humanize_image_error': ('agent.pet.generate.orchestrate', '_humanize_image_error'),
    'agent_pet_generate_orchestrate_hatch_pet': ('agent.pet.generate.orchestrate', 'hatch_pet'),
    'agent_account_usage_u_codex_backend_urls': ('agent.account_usage', '_codex_backend_urls'),
    'agent_account_usage_u_resolve_codex_usage_credentials': ('agent.account_usage', '_resolve_codex_usage_credentials'),
    'agent_account_usage_redeemed': ('agent.account_usage', 'redeemed'),
    'agent_account_usage_redeem_codex_reset_credit': ('agent.account_usage', 'redeem_codex_reset_credit'),
    'agent_bedrock_adapter_u_model_supports_prompt_cache': ('agent.bedrock_adapter', '_model_supports_prompt_cache'),
    'agent_bedrock_adapter_u_safe_text': ('agent.bedrock_adapter', '_safe_text'),
    'agent_bedrock_adapter_u_static_bedrock_context_length': ('agent.bedrock_adapter', '_static_bedrock_context_length'),
    'agent_bedrock_adapter_probe_bedrock_context_length': ('agent.bedrock_adapter', 'probe_bedrock_context_length'),
    'agent_kanban_stop_kanban_stop_nudge_enabled': ('agent.kanban_stop', 'kanban_stop_nudge_enabled'),
    'agent_kanban_stop_u_tool_call_name': ('agent.kanban_stop', '_tool_call_name'),
    'agent_kanban_stop_session_called_kanban_terminal': ('agent.kanban_stop', 'session_called_kanban_terminal'),
    'agent_kanban_stop_build_kanban_stop_nudge': ('agent.kanban_stop', 'build_kanban_stop_nudge'),
    'agent_thread_scoped_output_unsilence': ('agent.thread_scoped_output', 'unsilence'),
    'agent_thread_scoped_output_writelines': ('agent.thread_scoped_output', 'writelines'),
    'agent_thread_scoped_output_u__getattr__': ('agent.thread_scoped_output', '__getattr__'),
    'agent_thread_scoped_output_thread_scoped_silence': ('agent.thread_scoped_output', 'thread_scoped_silence'),
    'agent_agent_runtime_helpers_note_turn_start': ('agent.agent_runtime_helpers', 'note_turn_start'),
    'agent_agent_runtime_helpers_note_turn_persisted': ('agent.agent_runtime_helpers', 'note_turn_persisted'),
    'agent_agent_runtime_helpers_sync_credential_pool_entry_id': ('agent.agent_runtime_helpers', 'sync_credential_pool_entry_id'),
    'agent_anthropic_adapter_u_get_hermes_oauth_file': ('agent.anthropic_adapter', '_get_hermes_oauth_file'),
    'agent_anthropic_adapter_u_safe_text': ('agent.anthropic_adapter', '_safe_text'),
    'agent_anthropic_adapter_u_ensure_leading_user_turn': ('agent.anthropic_adapter', '_ensure_leading_user_turn'),
    'agent_battery_u_read_battery_uncached': ('agent.battery', '_read_battery_uncached'),
    'agent_battery_read_battery': ('agent.battery', 'read_battery'),
    'agent_battery_clear_cache': ('agent.battery', 'clear_cache'),
    'agent_billing_view_can_change_plan': ('agent.billing_view', 'can_change_plan'),
    'agent_billing_view_u_parse_auto_reload_card': ('agent.billing_view', '_parse_auto_reload_card'),
    'agent_billing_view_u_dev_fixture_billing_state': ('agent.billing_view', '_dev_fixture_billing_state'),
    'agent_bounded_response_read_streaming_error_body': ('agent.bounded_response', 'read_streaming_error_body'),
    'agent_bounded_response_u_safe_close': ('agent.bounded_response', '_safe_close'),
    'agent_bounded_response_read_error_body_or_default': ('agent.bounded_response', 'read_error_body_or_default'),
    'agent_display_u_display_url': ('agent.display', '_display_url'),
    'agent_display_build_status_phrase': ('agent.display', 'build_status_phrase'),
    'agent_display_u_get_cute_tool_message': ('agent.display', '_get_cute_tool_message'),
    'agent_moa_trace_u_traces_enabled_and_dir': ('agent.moa_trace', '_traces_enabled_and_dir'),
    'agent_moa_trace_u_slot_trace': ('agent.moa_trace', '_slot_trace'),
    'agent_moa_trace_save_moa_turn': ('agent.moa_trace', 'save_moa_turn'),
    'agent_oneshot_u_commit_message_template': ('agent.oneshot', '_commit_message_template'),
    'agent_oneshot_render_template': ('agent.oneshot', 'render_template'),
    'agent_oneshot_run_oneshot': ('agent.oneshot', 'run_oneshot'),
    'agent_tool_executor_u_ensure_file_checkpoint': ('agent.tool_executor', '_ensure_file_checkpoint'),
    'agent_tool_executor_u_parse_tool_arguments': ('agent.tool_executor', '_parse_tool_arguments'),
    'agent_tool_executor_execute_tool_calls_segmented': ('agent.tool_executor', 'execute_tool_calls_segmented'),
    'agent_auxiliary_client_u_try_nvidia_nim': ('agent.auxiliary_client', '_try_nvidia_nim'),
    'agent_auxiliary_client_u_obj_get': ('agent.auxiliary_client', '_obj_get'),
    'agent_billing_usage_build_usage_model': ('agent.billing_usage', 'build_usage_model'),
    'agent_billing_usage_u_dev_fixture_usage_model': ('agent.billing_usage', '_dev_fixture_usage_model'),
    'agent_credits_tracker_has_data': ('agent.credits_tracker', 'has_data'),
    'agent_credits_tracker_age_seconds': ('agent.credits_tracker', 'age_seconds'),
    'agent_prompt_caching_u_can_carry_marker': ('agent.prompt_caching', '_can_carry_marker'),
    'agent_prompt_caching_u_apply_system_cache_markers': ('agent.prompt_caching', '_apply_system_cache_markers'),
    'agent_redact_u_canonical_url_param_name': ('agent.redact', '_canonical_url_param_name'),
    'agent_redact_u_redact_strict_url_credentials': ('agent.redact', '_redact_strict_url_credentials'),
    'agent_skill_commands_split_stacked_skill_commands': ('agent.skill_commands', 'split_stacked_skill_commands'),
    'agent_skill_commands_build_stacked_skill_invocation_message': ('agent.skill_commands', 'build_stacked_skill_invocation_message'),
    'agent_stream_single_writer_claim_stream_writer': ('agent.stream_single_writer', 'claim_stream_writer'),
    'agent_stream_single_writer_stream_writer_is_current': ('agent.stream_single_writer', 'stream_writer_is_current'),
    'agent_turn_finalizer_u_is_pure_tool_call_tail': ('agent.turn_finalizer', '_is_pure_tool_call_tail'),
    'agent_turn_finalizer_u_drop_verification_continuation_scaffo_ng': ('agent.turn_finalizer', '_drop_verification_continuation_scaffolding'),
    'agent_video_gen_provider_save_url_video': ('agent.video_gen_provider', 'save_url_video'),
    'agent_video_gen_provider_u_create_and_poll': ('agent.video_gen_provider', '_create_and_poll'),
    'agent_context_references_format_reference_value': ('agent.context_references', 'format_reference_value'),
    'agent_credential_sources_u_remove_xai_oauth_device_code': ('agent.credential_sources', '_remove_xai_oauth_device_code'),
    'agent_insights_u_get_model_usage': ('agent.insights', '_get_model_usage'),
    'agent_jiter_preload_preload_jiter_native_extension': ('agent.jiter_preload', 'preload_jiter_native_extension'),
    'agent_moonshot_schema_u_ensure_required_array': ('agent.moonshot_schema', '_ensure_required_array'),
    'agent_runtime_cwd_u_is_install_tree': ('agent.runtime_cwd', '_is_install_tree'),
    'agent_system_prompt_u_tui_embedded_pane_clarifier': ('agent.system_prompt', '_tui_embedded_pane_clarifier'),
    'agent_tool_result_classificati_tool_may_have_side_effect': ('agent.tool_result_classification', 'tool_may_have_side_effect'),
    'agent_web_search_provider_get_provider_env': ('agent.web_search_provider', 'get_provider_env'),
    'agent_web_search_registry_u_disabled_web_plugin_for': ('agent.web_search_registry', '_disabled_web_plugin_for'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_agent_remaining_wrappers.py <cases.json>\n"); return 2
    with open(sys.argv[1], 'r', encoding='utf-8') as f: cases = json.load(f)
    for c in cases:
        op = c.get('op'); value = c.get('value', '')
        d = DISPATCH.get(op)
        if not d: sys.stdout.write(json.dumps({'fn':op}, separators=(',',':')) + '\n'); continue
        pymod, pyfn = d
        mod = MODS.get(pymod)
        try:
            out = getattr(mod, pyfn)(value) if mod else None
        except Exception as e:
            out = 'PYERR:' + str(e)
        if isinstance(out, bool): out = bool(out)
        elif isinstance(out, (int, float)) and not isinstance(out, bool): out = int(out)
        elif out is None: out = ''
        else: out = str(out)
        sys.stdout.write(json.dumps({'fn':op,'out':out}, ensure_ascii=True, separators=(',',':')) + '\n')
    return 0

if __name__ == '__main__':
    sys.exit(main())
