#!/usr/bin/env python3
"""
Batch add PoP annotations to PARTIAL functions.
This converts PARTIAL -> PORTED by adding explicit /* Port of Python: func_name */ comments.
"""

import re
import sys
from pathlib import Path

SLERMES_DIR = Path("/home/wubu/hermes-agent-dev/slermes")

# PARTIAL functions that exist in C but lack PoP annotation
# Format: (c_file, c_function, python_function)
POP_ANNOTATIONS = [
    # agent_message_repair.c
    ("src/agent/agent_message_repair.c", "agent_runtime_owns_post_tool_hook", "agent_runtime_owns_post_tool_hook"),
    ("src/agent/agent_message_repair.c", "apply_pending_steer_to_tool_results", "apply_pending_steer_to_tool_results"),
    
    # prompt_caching.c
    ("src/agent/prompt_caching.c", "anthropic_prompt_cache_policy", "anthropic_prompt_cache_policy"),
    
    # llm_client.c
    ("src/agent/llm_client.c", "copy_reasoning_content_for_api", "copy_reasoning_content_for_api"),
    ("src/agent/llm_client.c", "reapply_reasoning_echo_for_provider", "reapply_reasoning_echo_for_provider"),
    ("src/agent/llm_client.c", "extract_api_error_context", "extract_api_error_context"),
    ("src/agent/llm_client.c", "summarize_background_review_actions", "summarize_background_review_actions"),
    ("src/agent/llm_client.c", "build_memory_write_metadata", "build_memory_write_metadata"),
    
    # provider_anthropic.c
    ("src/agent/provider_anthropic.c", "resolve_anthropic_messages_max_tokens", "resolve_anthropic_messages_max_tokens"),
    ("src/agent/provider_anthropic.c", "anthropic_detect_claude_code_version", "detect_claude_code_version"),
    ("src/agent/provider_anthropic.c", "anthropic_get_claude_code_version", "get_claude_code_version"),
    ("src/agent/provider_anthropic.c", "is_oauth_token", "is_oauth_token"),
    ("src/agent/provider_anthropic.c", "anthropic_normalize_base_url_text", "normalize_base_url_text"),
    ("src/agent/provider_anthropic.c", "is_kimi_coding_endpoint", "is_kimi_coding_endpoint"),
    ("src/agent/provider_anthropic.c", "model_name_is_kimi_family", "model_name_is_kimi_family"),
    ("src/agent/provider_anthropic.c", "is_kimi_family_endpoint", "is_kimi_family_endpoint"),
    ("src/agent/provider_anthropic.c", "is_azure_anthropic_endpoint", "is_azure_anthropic_endpoint"),
    ("src/agent/provider_anthropic.c", "anthropic_common_betas_for_base_url", "common_betas_for_base_url"),
    ("src/agent/provider_anthropic.c", "is_claude_code_token_valid", "is_claude_code_token_valid"),
    ("src/agent/provider_anthropic.c", "anthropic_refresh_oauth_token", "refresh_oauth_token"),
    ("src/agent/provider_anthropic.c", "resolve_anthropic_token", "resolve_anthropic_token"),
    ("src/agent/provider_anthropic.c", "generate_pkce", "generate_pkce"),
    ("src/agent/provider_anthropic.c", "read_hermes_oauth_credentials", "read_hermes_oauth_credentials"),
    ("src/agent/provider_anthropic.c", "is_bedrock_model_id", "is_bedrock_model_id"),
    ("src/agent/provider_anthropic.c", "normalize_model_name", "normalize_model_name"),
    ("src/agent/provider_anthropic.c", "anthropic_sanitize_tool_id", "sanitize_tool_id"),
    ("src/agent/provider_anthropic.c", "anthropic_convert_tools_to_anthropic", "convert_tools_to_anthropic"),
    ("src/agent/provider_anthropic.c", "anthropic_convert_content_part_to_anthropic", "convert_content_part_to_anthropic"),
    ("src/agent/provider_anthropic.c", "anthropic_to_plain_data", "to_plain_data"),
    ("src/agent/provider_anthropic.c", "anthropic_extract_preserved_thinking_blocks", "extract_preserved_thinking_blocks"),
    ("src/agent/provider_anthropic.c", "anthropic_convert_content_to_anthropic", "convert_content_to_anthropic"),
    ("src/agent/provider_anthropic.c", "anthropic_content_parts_to_anthropic_blocks", "content_parts_to_anthropic_blocks"),
    ("src/agent/provider_anthropic.c", "anthropic_convert_assistant_message", "convert_assistant_message"),
    ("src/agent/provider_anthropic.c", "convert_tool_message_to_result", "convert_tool_message_to_result"),
    ("src/agent/provider_anthropic.c", "anthropic_convert_user_message", "convert_user_message"),
    ("src/agent/provider_anthropic.c", "strip_orphaned_tool_blocks", "strip_orphaned_tool_blocks"),
    ("src/agent/provider_anthropic.c", "anthropic_merge_consecutive_roles", "merge_consecutive_roles"),
    ("src/agent/provider_anthropic.c", "manage_thinking_signatures", "manage_thinking_signatures"),
    ("src/agent/provider_anthropic.c", "evict_old_screenshots", "evict_old_screenshots"),
    ("src/agent/provider_anthropic.c", "convert_messages_to_anthropic", "convert_messages_to_anthropic"),
    
    # auxiliary_client.c
    ("src/agent/auxiliary_client.c", "extract_url_query_params", "_extract_url_query_params"),
    ("src/agent/auxiliary_client.c", "is_kimi_model", "_is_kimi_model"),
    ("src/agent/auxiliary_client.c", "normalize_main_runtime", "_normalize_main_runtime"),
    ("src/agent/auxiliary_client.c", "mark_provider_unhealthy", "_mark_provider_unhealthy"),
    ("src/agent/auxiliary_client.c", "is_provider_unhealthy", "_is_provider_unhealthy"),
    ("src/agent/auxiliary_client.c", "log_skip_unhealthy", "_log_skip_unhealthy"),
    ("src/agent/auxiliary_client.c", "is_payment_error", "_is_payment_error"),
    ("src/agent/auxiliary_client.c", "is_rate_limit_error", "_is_rate_limit_error"),
    ("src/agent/auxiliary_client.c", "is_connection_error", "_is_connection_error"),
    ("src/agent/auxiliary_client.c", "is_auth_error", "_is_auth_error"),
    ("src/agent/auxiliary_client.c", "is_unsupported_temperature_error", "_is_unsupported_temperature_error"),
    ("src/agent/auxiliary_client.c", "is_model_not_found_error", "_is_model_not_found_error"),
    ("src/agent/auxiliary_client.c", "normalize_resolved_model", "_normalize_resolved_model"),
    ("src/agent/auxiliary_client.c", "main_model_supports_vision", "_main_model_supports_vision"),
    ("src/agent/auxiliary_client.c", "strict_vision_backend_available", "_strict_vision_backend_available"),
    ("src/agent/auxiliary_client.c", "resolve_task_provider_model", "_resolve_task_provider_model"),
    ("src/agent/auxiliary_client.c", "get_task_timeout", "_get_task_timeout"),
    ("src/agent/auxiliary_client.c", "get_task_extra_body", "_get_task_extra_body"),
    ("src/agent/auxiliary_client.c", "is_anthropic_compat_endpoint", "_is_anthropic_compat_endpoint"),
    ("src/agent/auxiliary_client.c", "convert_openai_images_to_anthropic", "_convert_openai_images_to_anthropic"),
    ("src/agent/auxiliary_client.c", "extract_content_or_reasoning", "extract_content_or_reasoning"),
    ("src/agent/auxiliary_client.c", "current_custom_base_url", "_current_custom_base_url"),
    ("src/agent/auxiliary_client.c", "normalize_chain_label", "_normalize_chain_label"),
    ("src/agent/auxiliary_client.c", "get_aux_model_for_provider", "_get_aux_model_for_provider"),
    
    # provider_bedrock.c
    ("src/agent/provider_bedrock.c", "bedrock_require_boto3", "require_boto3"),
    ("src/agent/provider_bedrock.c", "reset_client_cache", "reset_client_cache"),
    ("src/agent/provider_bedrock.c", "invalidate_runtime_client", "invalidate_runtime_client"),
    ("src/agent/provider_bedrock.c", "resolve_aws_auth_env_var", "resolve_aws_auth_env_var"),
    ("src/agent/provider_bedrock.c", "resolve_bedrock_region", "resolve_bedrock_region"),
    ("src/agent/provider_bedrock.c", "model_supports_tool_use", "model_supports_tool_use"),
    ("src/agent/provider_bedrock.c", "is_anthropic_bedrock_model", "is_anthropic_bedrock_model"),
    ("src/agent/provider_bedrock.c", "bedrock_convert_tools_to_converse", "convert_tools_to_converse"),
    ("src/agent/provider_bedrock.c", "bedrock_convert_content_to_converse", "convert_content_to_converse"),
    ("src/agent/provider_bedrock.c", "bedrock_convert_messages_to_converse", "convert_messages_to_converse"),
    ("src/agent/provider_bedrock.c", "bedrock_converse_stop_reason_to_openai", "converse_stop_reason_to_openai"),
    ("src/agent/provider_bedrock.c", "bedrock_normalize_converse_response", "normalize_converse_response"),
    ("src/agent/provider_bedrock.c", "build_converse_kwargs", "build_converse_kwargs"),
    ("src/agent/provider_bedrock.c", "call_converse", "call_converse"),
    ("src/agent/provider_bedrock.c", "call_converse_stream", "call_converse_stream"),
    ("src/agent/provider_bedrock.c", "reset_discovery_cache", "reset_discovery_cache"),
    ("src/agent/provider_bedrock.c", "bedrock_extract_provider_from_arn", "extract_provider_from_arn"),
    ("src/agent/provider_bedrock.c", "classify_bedrock_error", "classify_bedrock_error"),
    ("src/agent/provider_bedrock.c", "get_bedrock_context_length", "get_bedrock_context_length"),
    
    # browser_provider.c
    ("src/agent/browser_provider.c", "is_available", "is_available"),
    
    # chat_completion_helpers.c
    ("src/agent/chat_completion_helpers.c", "estimate_request_context_tokens", "estimate_request_context_tokens"),
    ("src/agent/chat_completion_helpers.c", "env_float", "env_float"),
    ("src/agent/chat_completion_helpers.c", "build_api_kwargs", "build_api_kwargs"),
    ("src/agent/chat_completion_helpers.c", "build_assistant_message", "build_assistant_message"),
    
    # context.c (context_compressor)
    ("src/agent/context.c", "collect_path_mentions", "collect_path_mentions"),
    ("src/agent/context.c", "content_length_for_budget", "content_length_for_budget"),
    ("src/agent/context.c", "strip_historical_media", "strip_historical_media"),
    ("src/cli/commands.c", "cmd_compress", "compress"),
    
    # context_engine.c (vtable defaults) - C functions have context_engine_ prefix
    ("src/agent/context_engine.c", "context_engine_default_name", "name"),
    ("src/agent/context_engine.c", "context_engine_default_update_from_response", "update_from_response"),
    ("src/agent/context_engine.c", "context_engine_default_should_compress", "should_compress"),
    ("src/agent/context_engine.c", "context_engine_default_compress", "compress"),
    ("src/agent/context_engine.c", "context_engine_default_should_compress_preflight", "should_compress_preflight"),
    ("src/agent/context_engine.c", "context_engine_default_has_content_to_compress", "has_content_to_compress"),
    ("src/agent/context_engine.c", "default_on_session_start", "on_session_start"),
    ("src/agent/context_engine.c", "default_on_session_end", "on_session_end"),
    ("src/agent/context_engine.c", "context_engine_default_on_session_reset", "on_session_reset"),
    ("src/agent/context_engine.c", "context_engine_default_get_tool_schemas", "get_tool_schemas"),
    ("src/agent/context_engine.c", "default_handle_tool_call", "handle_tool_call"),
    ("src/agent/context_engine.c", "context_engine_default_get_status", "get_status"),
    ("src/agent/context_engine.c", "context_engine_default_update_model", "update_model"),
    
    # context_references.c
    ("src/agent/context_references.c", "parse_context_references", "parse_context_references"),
    ("src/agent/file_safety.c", "resolve_path", "resolve_path"),
    ("src/agent/context_references.c", "is_binary_file", "is_binary_file"),
    
    # credential_pool.c
    ("src/agent/credential_pool.c", "has_available", "has_available"),
    ("src/tools/memory.c", "memory_persist", "persist"),
    
    # credential_sources.c
    ("src/agent/credential_sources.c", "find_removal_step", "find_removal_step"),
    
    # credits_tracker.c
    ("src/agent/credits_tracker.c", "used_fraction", "used_fraction"),
    
    # curator.c
    ("src/agent/curator.c", "should_run_now", "should_run_now"),
    ("src/agent/curator.c", "apply_automatic_transitions", "apply_automatic_transitions"),
    ("src/agent/curator.c", "classify_removed_skills", "classify_removed_skills"),
    ("src/agent/curator.c", "parse_structured_summary", "parse_structured_summary"),
    ("src/agent/curator.c", "extract_absorbed_into_declarations", "extract_absorbed_into_declarations"),
    
    # curator_backup.c
    ("src/tools/curator_backup.c", "get_backups_dir", "backups_dir"),
    ("src/tools/curator_backup.c", "get_cron_jobs_file", "cron_jobs_file"),
    ("src/tools/curator_backup.c", "backup_cron_jobs_into", "backup_cron_jobs_into"),
    ("src/tools/curator_backup.c", "get_keep", "get_keep"),
    ("src/tools/curator_backup.c", "count_skill_files", "count_skill_files"),
    ("src/tools/curator_backup.c", "write_manifest", "write_manifest"),
    ("src/tools/curator_backup.c", "read_manifest", "read_manifest"),
    ("src/cli/commands.c", "cmd_rollback", "rollback"),
    ("src/tools/curator_backup.c", "format_size", "format_size"),
    
    # display_core.c
    ("src/cli/display_core.c", "_diff_dim", "diff_dim"),
    ("src/cli/display_core.c", "_diff_file", "diff_file"),
    ("src/cli/display_core.c", "_diff_hunk", "diff_hunk"),
    ("src/cli/display_core.c", "_diff_minus", "diff_minus"),
    ("src/cli/display_core.c", "_diff_plus", "diff_plus"),
    ("src/cli/display_core.c", "set_tool_preview_max_len", "set_tool_preview_max_len"),
    ("src/cli/display_core.c", "get_skin_tool_prefix", "get_skin_tool_prefix"),
    ("src/cli/display_core.c", "get_tool_emoji", "get_tool_emoji"),
    ("src/cli/display_core.c", "_oneline", "oneline"),
    ("src/cli/display_core.c", "_resolved_path", "resolved_path"),
    ("src/cli/display_core.c", "_snapshot_text", "snapshot_text"),
    ("src/cli/display_core.c", "capture_local_edit_snapshot", "capture_local_edit_snapshot"),
    ("src/cli/display_core.c", "_result_succeeded", "result_succeeded"),
    ("src/cli/display_core.c", "_diff_from_snapshot", "diff_from_snapshot"),
    ("src/cli/display_core.c", "extract_edit_diff", "extract_edit_diff"),
    ("src/cli/display_core.c", "_emit_inline_diff", "emit_inline_diff"),
    ("src/cli/display_core.c", "get_waiting_faces", "get_waiting_faces"),
    ("src/cli/display_core.c", "get_thinking_faces", "get_thinking_faces"),
    ("src/cli/display_core.c", "get_thinking_verbs", "get_thinking_verbs"),
    ("src/cli/display_core.c", "start", "start"),
    ("src/cli/display_core.c", "stop", "stop"),
    ("src/cli/display_core.c", "_trim_error", "trim_error"),
    ("src/cli/display_core.c", "_detect_tool_failure", "detect_tool_failure"),
    
    # file_safety.c
    ("src/agent/file_safety.c", "find_sandbox_mirror_segments", "find_sandbox_mirror_segments"),
    
    # provider_google.c (gemini adapters)
    ("src/agent/provider_google.c", "google_coerce_content_to_text", "coerce_content_to_text"),
    ("src/agent/provider_google.c", "google_build_gemini_contents", "build_gemini_contents"),
    ("src/agent/provider_google.c", "google_translate_tools_to_gemini", "translate_tools_to_gemini"),
    ("src/agent/provider_google.c", "google_translate_tool_choice_to_gemini", "translate_tool_choice_to_gemini"),
    ("src/agent/provider_google.c", "google_normalize_thinking_config", "normalize_thinking_config"),
    ("src/agent/provider_google.c", "google_build_gemini_request", "build_gemini_request"),
    ("src/agent/provider_google.c", "google_translate_gemini_response", "translate_gemini_response"),
    ("src/agent/provider_google.c", "google_iter_sse_events", "iter_sse_events"),
    ("src/agent/provider_google.c", "google_translate_stream_event", "translate_stream_event"),
    ("src/agent/provider_google.c", "gemini_http_error", "gemini_http_error"),
    ("src/agent/provider_google.c", "google_probe_gemini_tier", "probe_gemini_tier"),
    ("src/agent/provider_google.c", "google_is_free_tier_quota_error", "is_free_tier_quota_error"),
    ("src/agent/provider_google.c", "google_extract_multimodal_parts", "extract_multimodal_parts"),
    ("src/agent/provider_google.c", "google_tool_call_extra_signature", "tool_call_extra_signature"),
    ("src/agent/provider_google.c", "google_tool_call_extra_from_part", "tool_call_extra_from_part"),
    ("src/agent/provider_google.c", "google_empty_response", "empty_response"),
    ("src/agent/provider_google.c", "_headers", "headers"),
    
    # google_code_assist.c
    ("src/agent/google_code_assist.c", "is_vpc_sc_violation", "is_vpc_sc_violation"),
    ("src/agent/google_code_assist.c", "_post_json", "post_json"),
    
    # google_oauth.c
    ("src/provider/google_oauth.c", "credentials_lock", "credentials_lock"),
    ("src/provider/google_oauth.c", "generate_pkce_pair", "generate_pkce_pair"),
    ("src/provider/google_oauth.c", "parse", "parse"),
    ("src/provider/google_oauth.c", "_post_form", "post_form"),
    ("src/provider/google_oauth.c", "exchange_code", "exchange_code"),
    
    # i18n.c
    ("src/agent/i18n.c", "_locales_dir", "locales_dir"),
    ("src/agent/i18n.c", "get_language", "get_language"),
    
    # image_gen.c
    ("src/tools/image_gen.c", "generate", "generate"),
    ("src/tools/image_gen.c", "image_gen_resolve_aspect_ratio", "resolve_aspect_ratio"),
    ("src/tools/image_gen.c", "image_gen_save_b64_image", "save_b64_image"),
    ("src/tools/image_gen.c", "image_gen_save_url_image", "save_url_image"),
    ("src/tools/image_gen.c", "image_gen_success_response", "success_response"),
    ("src/tools/image_gen.c", "image_gen_error_response", "error_response"),
    
    # image_routing.c
    ("src/agent/image_routing.c", "coerce_capability_bool", "coerce_capability_bool"),
    
    # insights.c
    ("src/agent/insights.c", "generate", "generate"),
    ("src/agent/insights.c", "format_terminal", "format_terminal"),
    ("src/agent/insights.c", "format_gateway", "format_gateway"),
    
    # budget_tracker.c
    ("src/agent/budget_tracker.c", "remaining", "remaining"),
    
    # lmstudio_reasoning.c
    ("src/agent/lmstudio_reasoning.c", "resolve_lmstudio_effort", "resolve_lmstudio_effort"),
    
    # manual_compression_feedback.c
    ("src/agent/manual_compression_feedback.c", "summarize_manual_compression", "summarize_manual_compression"),
    
    # think_scrubber.c
    ("src/agent/think_scrubber.c", "reset", "reset"),
    ("src/agent/think_scrubber.c", "feed", "feed"),
    ("src/agent/think_scrubber.c", "flush", "flush"),
    ("src/agent/think_scrubber.c", "max_partial_suffix", "max_partial_suffix"),
    
    # tool_search.c
    ("src/tools/tool_search.c", "tool_search_scoped_names", "tool_search_scoped_names"),
    
    # tool_guardrails.c
    ("src/agent/tool_guardrails.c", "_sha256", "sha256"),
    
    # tts.c
    ("src/tools/tts.c", "resolve_output_format", "resolve_output_format"),
    
    # turn_finalizer.c
    ("src/agent/turn_finalizer.c", "finalize_turn", "finalize_turn"),
    
    # usage_pricing.c
    ("src/agent/usage_pricing.c", "total_tokens", "total_tokens"),
    ("src/agent/usage_pricing.c", "resolve_billing_route", "resolve_billing_route"),
    ("src/agent/usage_pricing.c", "openrouter_pricing_entry", "openrouter_pricing_entry"),
    ("src/agent/usage_pricing.c", "pricing_entry_from_metadata", "pricing_entry_from_metadata"),
    
    # video_gen.c
    ("src/tools/video_gen.c", "generate", "generate"),
    ("src/tools/video_gen.c", "video_gen_save_b64_video", "save_b64_video"),
    ("src/tools/video_gen.c", "video_gen_save_bytes_video", "save_bytes_video"),
    ("src/tools/video_gen.c", "video_gen_success_response", "success_response"),
    ("src/tools/video_gen.c", "video_gen_error_response", "error_response"),
    
    # provider_metadata.c (model_metadata)
    ("src/agent/provider_metadata.c", "resolve_requests_verify", "resolve_requests_verify"),
    ("src/agent/provider_metadata.c", "grok_supports_reasoning_effort", "grok_supports_reasoning_effort"),
    ("src/agent/provider_metadata.c", "_normalize_base_url", "normalize_base_url"),
    ("src/agent/provider_metadata.c", "_auth_headers", "auth_headers"),
    ("src/agent/provider_metadata.c", "is_local_endpoint", "is_local_endpoint"),
    ("src/agent/provider_metadata.c", "detect_local_server_type", "detect_local_server_type"),
    ("src/agent/provider_metadata.c", "coerce_reasonable_int", "coerce_reasonable_int"),
    ("src/agent/provider_metadata.c", "extract_first_int", "extract_first_int"),
    ("src/agent/provider_metadata.c", "extract_context_length", "extract_context_length"),
    ("src/agent/provider_metadata.c", "extract_max_completion_tokens", "extract_max_completion_tokens"),
    ("src/agent/provider_metadata.c", "extract_pricing", "extract_pricing"),
    ("src/agent/provider_metadata.c", "add_model_aliases", "add_model_aliases"),
    ("src/agent/provider_metadata.c", "fetch_model_metadata", "fetch_model_metadata"),
    ("src/agent/provider_metadata.c", "fetch_endpoint_model_metadata", "fetch_endpoint_model_metadata"),
    ("src/agent/provider_metadata.c", "resolve_endpoint_context_length", "resolve_endpoint_context_length"),
    ("src/agent/provider_metadata.c", "save_context_length", "save_context_length"),
    ("src/agent/provider_metadata.c", "get_cached_context_length", "get_cached_context_length"),
    ("src/agent/provider_metadata.c", "get_next_probe_tier", "get_next_probe_tier"),
    ("src/agent/provider_metadata.c", "parse_context_limit_from_error", "parse_context_limit_from_error"),
    ("src/agent/provider_metadata.c", "get_context_length_from_provider_error", "get_context_length_from_provider_error"),
    ("src/agent/provider_metadata.c", "parse_available_output_tokens_from_error", "parse_available_output_tokens_from_error"),
    ("src/agent/provider_metadata.c", "model_id_matches", "model_id_matches"),
    ("src/agent/provider_metadata.c", "query_ollama_num_ctx", "query_ollama_num_ctx"),
    ("src/agent/provider_metadata.c", "query_ollama_api_show", "query_ollama_api_show"),
    ("src/agent/provider_metadata.c", "model_name_suggests_grok_4_3", "model_name_suggests_grok_4_3"),
    ("src/agent/provider_metadata.c", "query_local_context_length", "query_local_context_length"),
    ("src/agent/provider_metadata.c", "_normalize_model_version", "normalize_model_version"),
    ("src/agent/provider_metadata.c", "query_anthropic_context_length", "query_anthropic_context_length"),
    ("src/agent/provider_metadata.c", "fetch_codex_oauth_context_lengths", "fetch_codex_oauth_context_lengths"),
    ("src/agent/provider_metadata.c", "resolve_codex_oauth_context_length", "resolve_codex_oauth_context_length"),
    ("src/agent/provider_metadata.c", "resolve_nous_context_length", "resolve_nous_context_length"),
    ("src/agent/provider_metadata.c", "get_model_context_length", "get_model_context_length"),
    ("src/agent/provider_metadata.c", "estimate_tokens_rough", "estimate_tokens_rough"),
    ("src/agent/provider_metadata.c", "estimate_messages_tokens_rough", "estimate_messages_tokens_rough"),
    ("src/agent/provider_metadata.c", "count_image_tokens", "count_image_tokens"),
    ("src/agent/provider_metadata.c", "estimate_message_chars", "estimate_message_chars"),
    ("src/agent/provider_metadata.c", "estimate_request_tokens_rough", "estimate_request_tokens_rough"),
    ("src/agent/provider_metadata.c", "has_cost_data", "has_cost_data"),
    ("src/agent/provider_metadata.c", "supports_vision", "supports_vision"),
    ("src/agent/provider_metadata.c", "supports_pdf", "supports_pdf"),
    ("src/agent/provider_metadata.c", "supports_audio_input", "supports_audio_input"),
    ("src/agent/provider_metadata.c", "format_cost", "format_cost"),
    ("src/agent/provider_metadata.c", "format_capabilities", "format_capabilities"),
    ("src/agent/provider_metadata.c", "list_provider_models", "list_provider_models"),
    ("src/agent/provider_metadata.c", "list_agentic_models", "list_agentic_models"),
    
    # onboarding.c
    ("src/agent/onboarding.c", "profile_build_directive", "profile_build_directive"),
    
    # plugin_llm.c
    ("src/agent/plugin_llm.c", "normalize_ref", "normalize_ref"),
    ("src/agent/plugin_llm.c", "resolve_trust_policy", "resolve_trust_policy"),
    ("src/agent/plugin_llm.c", "build_structured_messages", "build_structured_messages"),
    ("src/agent/plugin_llm.c", "extract_usage", "extract_usage"),
    ("src/agent/plugin_llm.c", "resolve_attribution", "resolve_attribution"),
    ("src/agent/plugin_llm.c", "complete", "complete"),
    ("src/agent/plugin_llm.c", "complete_structured", "complete_structured"),
    
    # prompt_builder.c
    ("src/agent/prompt_builder.c", "format_steer_marker", "format_steer_marker"),
    ("src/agent/prompt_builder.c", "clear_backend_probe_cache", "clear_backend_probe_cache"),
    ("src/agent/prompt_builder.c", "parse_skill_file", "parse_skill_file"),
    ("src/agent/prompt_builder.c", "skill_should_show", "skill_should_show"),
    ("src/agent/prompt_builder.c", "truncate_content", "truncate_content"),
    
    # retry_utils.c
    ("src/agent/retry_utils.c", "jittered_backoff", "jittered_backoff"),
    
    # shell_hooks.c
    ("src/agent/shell_hooks.c", "matches_tool", "matches_tool"),
    ("src/agent/shell_hooks.c", "allowlist_path", "allowlist_path"),
    ("src/agent/shell_hooks.c", "prompt_and_record", "prompt_and_record"),
    ("src/agent/shell_hooks.c", "revoke", "revoke"),
    
    # skill_bundles.c
    ("src/agent/skill_bundles.c", "get_skill_bundles", "get_skill_bundles"),
    ("src/agent/skill_bundles.c", "reload_bundles", "reload_bundles"),
    ("src/agent/skill_bundles.c", "build_bundle_invocation_message", "build_bundle_invocation_message"),
    
    # skill_commands.c
    ("src/agent/skill_commands.c", "build_preloaded_skills_prompt", "build_preloaded_skills_prompt"),
    
    # skill_preprocessing.c
    ("src/agent/skill_preprocessing.c", "substitute_template_vars", "substitute_template_vars"),
    ("src/agent/skill_preprocessing.c", "run_inline_shell", "run_inline_shell"),
    ("src/agent/skill_preprocessing.c", "expand_inline_shell", "expand_inline_shell"),
    ("src/agent/skill_preprocessing.c", "preprocess_skill_content", "preprocess_skill_content"),
    
    # libskillutils/skill_utils.c
    ("lib/libskillutils/skill_utils.c", "skill_detect_environment", "detect_environment"),
    ("lib/libskillutils/skill_utils.c", "skill_matches_environment", "matches_environment"),
    ("lib/libskillutils/skill_utils.c", "skill_external_dirs_cache_clear", "external_dirs_cache_clear"),
    
    # stream_diag.c
    ("src/agent/stream_diag.c", "stream_diag_init", "stream_diag_init"),
    ("src/agent/stream_diag.c", "log_stream_retry", "log_stream_retry"),
    ("src/agent/stream_diag.c", "emit_stream_drop", "emit_stream_drop"),

    # plugin_llm.c
    ("src/agent/plugin_llm.c", "normalize_ref", "normalize_ref"),
    ("src/agent/plugin_llm.c", "resolve_trust_policy", "resolve_trust_policy"),
    ("src/agent/plugin_llm.c", "normalize_input_block", "normalize_input_block"),
    ("src/agent/plugin_llm.c", "plugin_llm_build_structured_messages", "build_structured_messages"),
    ("src/agent/plugin_llm.c", "strip_code_fences", "strip_code_fences"),
    ("src/agent/plugin_llm.c", "parse_structured_text", "parse_structured_text"),
    ("src/agent/plugin_llm.c", "extract_usage", "extract_usage"),
    ("src/agent/plugin_llm.c", "extract_text", "extract_text"),
    ("src/agent/plugin_llm.c", "resolve_attribution", "resolve_attribution"),
    ("src/agent/plugin_llm.c", "plugin_llm_complete", "complete"),
    ("src/agent/plugin_llm.c", "plugin_llm_complete_structured", "complete_structured"),
    ("src/agent/plugin_llm.c", "json_response_format", "json_response_format"),

    # system_prompt.c
    ("src/agent/system_prompt.c", "format_tools_for_system_message", "format_tools_for_system_message"),

    # agent_message_sanitize.c
    ("src/agent/agent_message_sanitize.c", "extract_reasoning", "extract_reasoning"),

    # prompt_caching.c
    ("src/agent/prompt_caching.c", "anthropic_prompt_cache_policy", "anthropic_prompt_cache_policy"),

    # tool_registry.c
    ("src/tools/registry.c", "repair_tool_call", "repair_tool_call"),

    # llm_client.c
    ("src/agent/llm_client.c", "copy_reasoning_content_for_api", "copy_reasoning_content_for_api"),
    ("src/agent/llm_client.c", "reapply_reasoning_echo_for_provider", "reapply_reasoning_echo_for_provider"),
    ("src/agent/llm_client.c", "extract_api_error_context", "extract_api_error_context"),
]

def add_pop_annotation(filepath: str, func_name: str, python_name: str) -> bool:
    """Add PoP annotation before a C function definition."""
    full_path = SLERMES_DIR / filepath
    if not full_path.exists():
        print(f"  FILE NOT FOUND: {filepath}")
        return False

    with open(full_path) as f:
        content = f.read()

    # Check if PoP annotation already exists for this specific function.
    # Look for annotation pattern within 200 chars before the function definition
    # (per-function annotations should be right before the function, not at module level).
    # Use a simpler pattern that finds the function definition regardless of return type complexity.
    func_pattern = rf'{re.escape(func_name)}\s*\('
    func_match = re.search(func_pattern, content, re.MULTILINE)
    if func_match:
        # Check if there's a PoP annotation in the 200 chars before the function
        check_region = content[max(0, func_match.start()-200):func_match.start()]
        if f"Port of Python" in check_region and python_name in check_region:
            return False  # Already exists
    else:
        # Function not found, but maybe annotation exists elsewhere - do a broader check
        # Only skip if the function name appears in a PoP annotation context
        # (i.e., "module.py:func_name" or "Port of Python: func_name")
        broader_pattern = rf'Port of Python[^:\n]*:[^(\n]*{re.escape(python_name)}'
        if re.search(broader_pattern, content):
            return False  # Looks like annotation exists
    
    # Find the function definition
    # Pattern: return_type func_name(  or  static return_type func_name(
    pattern = rf'{re.escape(func_name)}\s*\('
    
    match = re.search(pattern, content, re.MULTILINE)
    if not match:
        print(f"  FUNCTION NOT FOUND: {func_name} in {filepath}")
        return False
    
    # Insert PoP comment before the function
    insert_pos = match.start()
    pop_comment = f"/* Port of Python: {python_name} */\n"
    
    new_content = content[:insert_pos] + pop_comment + content[insert_pos:]
    
    with open(full_path, 'w') as f:
        f.write(new_content)
    
    print(f"  ✅ Added PoP: {filepath}:{func_name} -> {python_name}")
    return True

def main():
    added = 0
    skipped = 0
    failed = 0
    
    for filepath, func_name, python_name in POP_ANNOTATIONS:
        try:
            if add_pop_annotation(filepath, func_name, python_name):
                added += 1
            else:
                skipped += 1
        except Exception as e:
            print(f"  ❌ ERROR {filepath}:{func_name} - {e}")
            failed += 1
    
    print(f"\n=== SUMMARY ===")
    print(f"Added: {added}")
    print(f"Skipped (already exists): {skipped}")
    print(f"Failed: {failed}")

if __name__ == "__main__":
    main()