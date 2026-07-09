# Slermes Façade Audit (v546, post-checkpoint)

**Date:** 2026-07-08

## Definition
PoP-annotated C fn whose entire body (after stripping comments, `hermes_log`, `(void)arg`) is a single `return <hardcoded const>`. Passes scanner + depth-check, does none of the Python's real work.

## Totals
- TRUE FAÇADES found: **133**
  - **fraudulent** (caller lied to / real work skipped): **110**
  - honest-limitation (const return is truthful in C, e.g. SDK getter -> NULL): **23**
- Thin wrappers (return var/expr): 42
- No-return bodies: 37

## Fraudulent façades (must be re-implemented from Python)

### cli/port_agent_think_scrubber.c
- `cli_agent_think_scrubber_is_in_think_block` (`agent/think_scrubber.py:is_in_think_block`) -> `0`

### cli/port_agent_tts_provider_methods.c
- `tts_provider_voice_compatible` (`agent/tts_provider.py:voice_compatible`) -> `0`

### cli/port_gateway_delivery.c
- `cli_gateway_delivery__filter_silence_narration_enabled` (`gateway/delivery.py:_filter_silence_narration_enabled`) -> `1`

### cli/port_gateway_hooks.c
- `cli_gateway_hooks_loaded_hooks` (`gateway/hooks.py:loaded_hooks`) -> `0`

### cli/port_gateway_platform_registry.c
- `cli_gateway_platform_registry_all_entries` (`gateway/platform_registry.py:all_entries`) -> `0`
- `cli_gateway_platform_registry_plugin_entries` (`gateway/platform_registry.py:plugin_entries`) -> `0`

### cli/port_hermes_cli_hooks.c
- `cli_hermes_cli_hooks__doctor_one` (`hermes_cli/hooks.py:_doctor_one`) -> `0`

### cli/port_hermes_cli_voice.c
- `cli_hermes_cli_voice__beeps_enabled` (`hermes_cli/voice.py:_beeps_enabled`) -> `1`
- `cli_hermes_cli_voice_start_continuous` (`hermes_cli/voice.py:start_continuous`) -> `0`
- `cli_hermes_cli_voice_stop_and_transcribe` (`hermes_cli/voice.py:stop_and_transcribe`) -> `0`

### cli/port_tools_computer_use_backend.c
- `cli_tools_computer_use_backend_capture` (`tools/computer_use/backend.py:capture`) -> `0`
- `cli_tools_computer_use_backend_drag` (`tools/computer_use/backend.py:drag`) -> `0`
- `cli_tools_computer_use_backend_list_apps` (`tools/computer_use/backend.py:list_apps`) -> `0`

### cli/port_tools_environments_file_sync.c
- `cli_tools_environments_file_sync__sync_back_impl` (`tools/environments/file_sync.py:_sync_back_impl`) -> `0`
- `cli_tools_environments_file_sync_sync` (`tools/environments/file_sync.py:sync`) -> `0`

### cli/port_tools_environments_managed_modal.c
- `cli_tools_environments_managed_modal__guard_unsupported_credential_passthrough` (`tools/environments/managed_modal.py:_guard_unsupported_credential_passthrough`) -> `0`

### cli/port_tools_environments_modal_utils.c
- `cli_tools_environments_modal_utils__before_execute` (`tools/environments/modal_utils.py:_before_execute`) -> `0`

### cli/port_tools_environments_ssh.c
- `cli_tools_environments_ssh__before_execute` (`tools/environments/ssh.py:_before_execute`) -> `0`

### cli/port_tools_slash_confirm.c
- `cli_tools_slash_confirm_clear_if_stale` (`tools/slash_confirm.py:clear_if_stale`) -> `0`
- `cli_tools_slash_confirm_get_pending` (`tools/slash_confirm.py:get_pending`) -> `0`

### tools/port_browser_tool.c
- `_browser_cleanup_thread_worker` (`tools/browser_tool.py:_browser_cleanup_thread_worker`) -> `true`
- `_browser_install_hint` (`tools/browser_tool.py:_browser_install_hint`) -> `false`
- `_get_browser_engine` (`tools/browser_tool.py:_get_browser_engine`) -> `false`
- `_get_cloud_provider` (`tools/browser_tool.py:_get_cloud_provider`) -> `false`
- `_is_legacy_provider_registry_overridden` (`tools/browser_tool.py:_is_legacy_provider_registry_overridden`) -> `false`
- `_lightpanda_fallback_reason` (`tools/browser_tool.py:_lightpanda_fallback_reason`) -> `false`
- `_needs_lightpanda_fallback` (`tools/browser_tool.py:_needs_lightpanda_fallback`) -> `false`
- `_requires_real_termux_browser_install` (`tools/browser_tool.py:_requires_real_termux_browser_install`) -> `false`
- `_should_inject_engine` (`tools/browser_tool.py:_should_inject_engine`) -> `false`
- `_start_browser_cleanup_thread` (`tools/browser_tool.py:_start_browser_cleanup_thread`) -> `true`
- `_termux_browser_install_error` (`tools/browser_tool.py:_termux_browser_install_error`) -> `false`
- `_write_owner_pid` (`tools/browser_tool.py:_write_owner_pid`) -> `true`
- `browser_verify_reapable_browser_daemon` (`tools/browser_tool.py:_verify_reapable_browser_daemon`) -> `false`

### tools/port_file_operations.c
- `file_ops_lsp_handles_extension` (`tools/file_operations.py:_lsp_handles_extension`) -> `true`
- `file_ops_lsp_local_only` (`tools/file_operations.py:_lsp_local_only`) -> `true`
- `file_ops_lsp_will_handle` (`tools/file_operations.py:_lsp_will_handle`) -> `true`
- `file_ops_search_content` (`tools/file_operations.py:_search_content`) -> `true`
- `file_ops_search_files` (`tools/file_operations.py:_search_files`) -> `true`
- `file_ops_search_files_rg` (`tools/file_operations.py:_search_files_rg`) -> `true`
- `file_ops_search_with_grep` (`tools/file_operations.py:_search_with_grep`) -> `true`
- `file_ops_search_with_rg` (`tools/file_operations.py:_search_with_rg`) -> `true`
- `file_ops_suggest_similar_files` (`tools/file_operations.py:_suggest_similar_files`) -> `true`
- `file_ops_unified_diff` (`tools/file_operations.py:_unified_diff`) -> `true`

### tools/port_mcp_tool.c
- `mcp_tool_handle_session_expired_and_retry` (`tools/mcp_tool.py:_handle_session_expired_and_retry`) -> `true`
- `mcp_tool_has_registered_mcp_tools` (`tools/mcp_tool.py:has_registered_mcp_tools`) -> `true`
- `mcp_tool_kill_orphaned_mcp_children` (`tools/mcp_tool.py:_kill_orphaned_mcp_children`) -> `0`
- `mcp_tool_run_http` (`tools/mcp_tool.py:_run_http`) -> `0`
- `mcp_tool_run_sse` (`tools/mcp_tool.py:_run_sse`) -> `0`
- `mcp_tool_run_stdio` (`tools/mcp_tool.py:_run_stdio`) -> `0`
- `mcp_tool_run_streamable_http` (`tools/mcp_tool.py:_run_streamable_http`) -> `0`
- `mcp_tool_run_ws` (`tools/mcp_tool.py:_run_ws`) -> `0`
- `mcp_tool_wait_for_lifecycle_event` (`tools/mcp_tool.py:_wait_for_lifecycle_event`) -> `0`
- `mcp_tool_wait_for_reconnect_or_shutdown` (`tools/mcp_tool.py:_wait_for_reconnect_or_shutdown`) -> `0`

### tools/port_skills_hub.c
- `skills_hub_cache_valid` (`tools/skills_hub.py:_cache_valid`) -> `true`
- `skills_hub_convert_to_skill_md` (`tools/skills_hub.py:_convert_to_skill_md`) -> `true`
- `skills_hub_dedupe_results` (`tools/skills_hub.py:_dedupe_results`) -> `true`
- `skills_hub_detail_to_metadata` (`tools/skills_hub.py:_detail_to_metadata`) -> `0`
- `skills_hub_discover_identifier` (`tools/skills_hub.py:_discover_identifier`) -> `true`
- `skills_hub_download_directory` (`tools/skills_hub.py:_download_directory`) -> `true`
- `skills_hub_download_directory_recursive` (`tools/skills_hub.py:_download_directory_recursive`) -> `true`
- `skills_hub_download_zip` (`tools/skills_hub.py:_download_zip`) -> `true`
- `skills_hub_exact_slug_meta` (`tools/skills_hub.py:_exact_slug_meta`) -> `true`
- `skills_hub_extract_files` (`tools/skills_hub.py:_extract_files`) -> `true`
- `skills_hub_extract_first_match` (`tools/skills_hub.py:_extract_first_match`) -> `0`
- `skills_hub_extract_repo_slug` (`tools/skills_hub.py:_extract_repo_slug`) -> `0`
- `skills_hub_extract_weekly_installs` (`tools/skills_hub.py:_extract_weekly_installs`) -> `0`
- `skills_hub_featured_skills` (`tools/skills_hub.py:_featured_skills`) -> `true`
- `skills_hub_fetch_agent` (`tools/skills_hub.py:_fetch_agent`) -> `true`
- `skills_hub_fetch_browesh_source` (`tools/skills_hub.py:_fetch_browesh_source`) -> `true`
- `skills_hub_fetch_detail_page` (`tools/skills_hub.py:_fetch_detail_page`) -> `true`
- `skills_hub_fetch_index` (`tools/skills_hub.py:_fetch_index`) -> `true`
- `skills_hub_fetch_marketplace_index` (`tools/skills_hub.py:_fetch_marketplace_index`) -> `true`
- `skills_hub_finalize_inspect_meta` (`tools/skills_hub.py:_finalize_inspect_meta`) -> `true`
- `skills_hub_finalize_search_results` (`tools/skills_hub.py:_finalize_search_results`) -> `true`
- `skills_hub_find_entry` (`tools/skills_hub.py:_find_entry`) -> `true`
- `skills_hub_get_github` (`tools/skills_hub.py:_get_github`) -> `true`
- `skills_hub_get_json` (`tools/skills_hub.py:_get_json`) -> `true`
- `skills_hub_github_get` (`tools/skills_hub.py:_github_get`) -> `true`
- `skills_hub_item_to_meta` (`tools/skills_hub.py:_item_to_meta`) -> `true`
- `skills_hub_list_skills_in_repo` (`tools/skills_hub.py:_list_skills_in_repo`) -> `true`
- `skills_hub_lock_file_read` (`tools/skills_hub.py:_lock_file_read`) -> `true`
- `skills_hub_lock_file_write` (`tools/skills_hub.py:_lock_file_write`) -> `true`
- `skills_hub_matches_skill_tokens` (`tools/skills_hub.py:_matches_skill_tokens`) -> `true`
- `skills_hub_meta_from_search_item` (`tools/skills_hub.py:_meta_from_search_item`) -> `true`
- `skills_hub_parse_detail_page` (`tools/skills_hub.py:_parse_detail_page`) -> `true`
- `skills_hub_scan_all` (`tools/skills_hub.py:_scan_all`) -> `true`
- `skills_hub_search_catalog` (`tools/skills_hub.py:_search_catalog`) -> `0`
- `skills_hub_sitemap_catalog` (`tools/skills_hub.py:_sitemap_catalog`) -> `true`
- `skills_hub_slug_from_identifier` (`tools/skills_hub.py:_slug_from_identifier`) -> `true`
- `skills_hub_to_meta` (`tools/skills_hub.py:_to_meta`) -> `true`
- `skills_hub_token_variants` (`tools/skills_hub.py:_token_variants`) -> `0`

### tools/port_tts_tool.c
- `tts_tool_clean_gemini_audio_tag_rewrite` (`tools/tts_tool.py:_clean_gemini_audio_tag_rewrite`) -> `true`
- `tts_tool_compose_gemini_tts_prompt` (`tools/tts_tool.py:_compose_gemini_tts_prompt`) -> `true`
- `tts_tool_configured_command_tts_output_path` (`tools/tts_tool.py:_configured_command_tts_output_path`) -> `true`
- `tts_tool_default_neutts_ref_audio` (`tools/tts_tool.py:_default_neutts_ref_audio`) -> `true`
- `tts_tool_default_neutts_ref_text` (`tools/tts_tool.py:_default_neutts_ref_text`) -> `true`
- `tts_tool_extract_auxiliary_message_content` (`tools/tts_tool.py:_extract_auxiliary_message_content`) -> `true`
- `tts_tool_gemini_audio_tags_enabled` (`tools/tts_tool.py:_gemini_audio_tags_enabled`) -> `true`
- `tts_tool_generate_command_tts` (`tools/tts_tool.py:_generate_command_tts`) -> `true`
- `tts_tool_generate_gemini_tts` (`tools/tts_tool.py:_generate_gemini_tts`) -> `true`
- `tts_tool_generate_neutts` (`tools/tts_tool.py:_generate_neutts`) -> `true`
- `tts_tool_get_command_tts_output_format` (`tools/tts_tool.py:_get_command_tts_output_format`) -> `true`
- `tts_tool_has_any_command_tts_provider` (`tools/tts_tool.py:_has_any_command_tts_provider`) -> `true`
- `tts_tool_has_ffmpeg` (`tools/tts_tool.py:_has_ffmpeg`) -> `true`
- `tts_tool_is_command_tts_voice_compatible` (`tools/tts_tool.py:_is_command_tts_voice_compatible`) -> `true`
- `tts_tool_plugin_provider_is_voice_compatible` (`tools/tts_tool.py:_plugin_provider_is_voice_compatible`) -> `true`
- `tts_tool_render_command_tts_template` (`tools/tts_tool.py:_render_command_tts_template`) -> `true`
- `tts_tool_rewrite_gemini_tts_audio_tags` (`tools/tts_tool.py:_rewrite_gemini_tts_audio_tags`) -> `true`
- `tts_tool_run_command_tts` (`tools/tts_tool.py:_run_command_tts`) -> `true`
- `tts_tool_terminate_command_tts_process_tree` (`tools/tts_tool.py:_terminate_command_tts_process_tree`) -> `true`

## Honest-limitation façades (const return is truthful in C; revisit only if caller needs real semantics)

### cli/port_agent_anthropic_adapter.c
- `cli_agent_anthropic_adapter__get_anthropic_sdk` (`agent/anthropic_adapter.py:_get_anthropic_sdk`) -> `NULL`

### cli/port_agent_bedrock_adapter.c
- `cli_agent_bedrock_adapter__require_boto3` (`agent/bedrock_adapter.py:_require_boto3`) -> `0`

### cli/port_agent_display.c
- `cli_agent_display___enter__` (`agent/display.py:__enter__`) -> `0`
- `cli_agent_display___exit__` (`agent/display.py:__exit__`) -> `0`

### cli/port_hermes_cli_memory_setup.c
- `cli_hermes_cli_memory_setup__get_available_providers` (`hermes_cli/memory_setup.py:_get_available_providers`) -> `0`

### cli/port_tools_clarify_tool.c
- `cli_tools_clarify_tool_check_clarify_requirements` (`tools/clarify_tool.py:check_clarify_requirements`) -> `1`

### cli/port_tools_env_passthrough.c
- `cli_tools_env_passthrough__load_config_passthrough` (`tools/env_passthrough.py:_load_config_passthrough`) -> `0`

### cli/port_tools_osv_check.c
- `cli_tools_osv_check__query_osv` (`tools/osv_check.py:_query_osv`) -> `0`

### cli/port_tools_todo_tool.c
- `todo_tool_check_todo_requirements` (`tools/todo_tool.py:check_todo_requirements`) -> `0`

### cron/port_cron_scheduler_provider.c
- `cron_is_available` (`cron/scheduler_provider.py:is_available`) -> `true`

### tools/port_mcp_tool.c
- `mcp_tool_check_rate_limit` (`tools/mcp_tool.py:_check_rate_limit`) -> `true`
- `mcp_tool_ensure_mcp_loop` (`tools/mcp_tool.py:_ensure_mcp_loop`) -> `true`

### tools/port_skills_hub.c
- `skills_hub_ensure_loaded` (`tools/skills_hub.py:_ensure_loaded`) -> `true`
- `skills_hub_load_catalog_index` (`tools/skills_hub.py:_load_catalog_index`) -> `true`
- `skills_hub_resolve_github_meta` (`tools/skills_hub.py:_resolve_github_meta`) -> `true`
- `skills_hub_resolve_latest_version` (`tools/skills_hub.py:_resolve_latest_version`) -> `true`
- `skills_hub_resolve_skill_md_url` (`tools/skills_hub.py:_resolve_skill_md_url`) -> `true`
- `skills_hub_resolve_skill_name` (`tools/skills_hub.py:_resolve_skill_name`) -> `true`

### tools/port_tts_tool.c
- `tts_tool_check_kittentts_available` (`tools/tts_tool.py:_check_kittentts_available`) -> `true`
- `tts_tool_check_neutts_available` (`tools/tts_tool.py:_check_neutts_available`) -> `true`
- `tts_tool_check_piper_available` (`tools/tts_tool.py:_check_piper_available`) -> `true`
- `tts_tool_gemini_model_supports_audio_tags` (`tools/tts_tool.py:_gemini_model_supports_audio_tags`) -> `true`
- `tts_tool_resolve_command_provider_config` (`tools/tts_tool.py:_resolve_command_provider_config`) -> `true`
