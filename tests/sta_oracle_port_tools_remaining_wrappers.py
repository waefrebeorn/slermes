"""AUTO-GENERATED integration oracle for port_tools_remaining_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.ansi_strip'] = _load('tools/ansi_strip.py')
MODS['tools.binary_extensions'] = _load('tools/binary_extensions.py')
MODS['tools.browser_camofox_state'] = _load('tools/browser_camofox_state.py')
MODS['tools.browser_tool'] = _load('tools/browser_tool.py')
MODS['tools.budget_config'] = _load('tools/budget_config.py')
MODS['tools.checkpoint_manager'] = _load('tools/checkpoint_manager.py')
MODS['tools.clarify_gateway'] = _load('tools/clarify_gateway.py')
MODS['tools.computer_use.backend'] = _load('tools/computer_use/backend.py')
MODS['tools.computer_use.doctor'] = _load('tools/computer_use/doctor.py')
MODS['tools.computer_use.permissions'] = _load('tools/computer_use/permissions.py')
MODS['tools.computer_use.tool'] = _load('tools/computer_use/tool.py')
MODS['tools.credential_files'] = _load('tools/credential_files.py')
MODS['tools.daemon_pool'] = _load('tools/daemon_pool.py')
MODS['tools.debug_helpers'] = _load('tools/debug_helpers.py')
MODS['tools.delegate_tool'] = _load('tools/delegate_tool.py')
MODS['tools.delegation_live_log'] = _load('tools/delegation_live_log.py')
MODS['tools.desktop_ui'] = _load('tools/desktop_ui.py')
MODS['tools.env_probe'] = _load('tools/env_probe.py')
MODS['tools.environments.file_sync'] = _load('tools/environments/file_sync.py')
MODS['tools.environments.managed_modal'] = _load('tools/environments/managed_modal.py')
MODS['tools.environments.modal'] = _load('tools/environments/modal.py')
MODS['tools.feishu_drive_tool'] = _load('tools/feishu_drive_tool.py')
MODS['tools.file_state'] = _load('tools/file_state.py')
MODS['tools.focus_pane_tool'] = _load('tools/focus_pane_tool.py')
MODS['tools.homeassistant_tool'] = _load('tools/homeassistant_tool.py')
MODS['tools.hook_output_spill'] = _load('tools/hook_output_spill.py')
MODS['tools.image_generation_tool'] = _load('tools/image_generation_tool.py')
MODS['tools.image_source'] = _load('tools/image_source.py')
MODS['tools.interrupt'] = _load('tools/interrupt.py')
MODS['tools.kanban_tools'] = _load('tools/kanban_tools.py')
MODS['tools.lazy_deps'] = _load('tools/lazy_deps.py')
MODS['tools.mcp_dashboard_oauth'] = _load('tools/mcp_dashboard_oauth.py')
MODS['tools.mcp_stdio_watchdog'] = _load('tools/mcp_stdio_watchdog.py')
MODS['tools.microsoft_graph_client'] = _load('tools/microsoft_graph_client.py')
MODS['tools.mixture_of_agents_tool'] = _load('tools/mixture_of_agents_tool.py')
MODS['tools.moa_performance'] = _load('tools/moa_performance.py')
MODS['tools.online_research'] = _load('tools/online_research.py')
MODS['tools.open_preview_tool'] = _load('tools/open_preview_tool.py')
MODS['tools.project_tools'] = _load('tools/project_tools.py')
MODS['tools.registry'] = _load('tools/registry.py')
MODS['tools.send_message_tool'] = _load('tools/send_message_tool.py')
MODS['tools.session_search_tool'] = _load('tools/session_search_tool.py')
MODS['tools.skill_provenance'] = _load('tools/skill_provenance.py')
MODS['tools.skills_guard'] = _load('tools/skills_guard.py')
MODS['tools.skills_hub'] = _load('tools/skills_hub.py')
MODS['tools.thread_context'] = _load('tools/thread_context.py')
MODS['tools.tirith_security'] = _load('tools/tirith_security.py')
MODS['tools.tool_backend_helpers'] = _load('tools/tool_backend_helpers.py')
MODS['tools.tool_search'] = _load('tools/tool_search.py')
MODS['tools.tts_streaming'] = _load('tools/tts_streaming.py')
MODS['tools.video_generation_tool'] = _load('tools/video_generation_tool.py')
MODS['tools.voice_mode'] = _load('tools/voice_mode.py')
MODS['tools.web_tools'] = _load('tools/web_tools.py')
MODS['tools.website_policy'] = _load('tools/website_policy.py')
MODS['tools.x_search_tool'] = _load('tools/x_search_tool.py')
MODS['tools.xai_video_tools'] = _load('tools/xai_video_tools.py')

DISPATCH = {
    'tools_computer_use_tool_u_canon_key_combo': ('tools.computer_use.tool', '_canon_key_combo'),
    'tools_computer_use_tool_reset_backend_for_tests': ('tools.computer_use.tool', 'reset_backend_for_tests'),
    'tools_computer_use_tool_type_text': ('tools.computer_use.tool', 'type_text'),
    'tools_computer_use_tool_list_apps': ('tools.computer_use.tool', 'list_apps'),
    'tools_computer_use_tool_list_windows': ('tools.computer_use.tool', 'list_windows'),
    'tools_computer_use_tool_set_value': ('tools.computer_use.tool', 'set_value'),
    'tools_computer_use_tool_u_request_approval': ('tools.computer_use.tool', '_request_approval'),
    'tools_computer_use_tool_u_summarize_action': ('tools.computer_use.tool', '_summarize_action'),
    'tools_computer_use_tool_u_image_dimensions_from_b64': ('tools.computer_use.tool', '_image_dimensions_from_b64'),
    'tools_computer_use_tool_u_coerce_max_elements': ('tools.computer_use.tool', '_coerce_max_elements'),
    'tools_computer_use_tool_u_shrink_capture_for_vision': ('tools.computer_use.tool', '_shrink_capture_for_vision'),
    'tools_computer_use_tool_u_should_route_through_aux_vision': ('tools.computer_use.tool', '_should_route_through_aux_vision'),
    'tools_computer_use_tool_u_capture_after_mode': ('tools.computer_use.tool', '_capture_after_mode'),
    'tools_computer_use_tool_u_route_capture_through_aux_vision': ('tools.computer_use.tool', '_route_capture_through_aux_vision'),
    'tools_computer_use_tool_u_maybe_follow_capture': ('tools.computer_use.tool', '_maybe_follow_capture'),
    'tools_computer_use_tool_u_format_elements': ('tools.computer_use.tool', '_format_elements'),
    'tools_computer_use_tool_u_element_to_dict': ('tools.computer_use.tool', '_element_to_dict'),
    'tools_computer_use_tool_check_computer_use_requirements': ('tools.computer_use.tool', 'check_computer_use_requirements'),
    'tools_lazy_deps_u_format': ('tools.lazy_deps', '_format'),
    'tools_lazy_deps_u_python_abi_tag': ('tools.lazy_deps', '_python_abi_tag'),
    'tools_lazy_deps_u_lazy_install_target': ('tools.lazy_deps', '_lazy_install_target'),
    'tools_lazy_deps_u_ensure_target_ready': ('tools.lazy_deps', '_ensure_target_ready'),
    'tools_lazy_deps_u_activate_target_on_syspath': ('tools.lazy_deps', '_activate_target_on_syspath'),
    'tools_lazy_deps_activate_durable_lazy_target': ('tools.lazy_deps', 'activate_durable_lazy_target'),
    'tools_lazy_deps_u_allow_lazy_installs': ('tools.lazy_deps', '_allow_lazy_installs'),
    'tools_lazy_deps_u_unsupported_feature_reason': ('tools.lazy_deps', '_unsupported_feature_reason'),
    'tools_lazy_deps_u_is_satisfied': ('tools.lazy_deps', '_is_satisfied'),
    'tools_lazy_deps_u_is_present': ('tools.lazy_deps', '_is_present'),
    'tools_lazy_deps_u_core_constraints_file': ('tools.lazy_deps', '_core_constraints_file'),
    'tools_lazy_deps_u_venv_pip_install': ('tools.lazy_deps', '_venv_pip_install'),
    'tools_lazy_deps_active_features': ('tools.lazy_deps', 'active_features'),
    'tools_lazy_deps_refresh_active_features': ('tools.lazy_deps', 'refresh_active_features'),
    'tools_lazy_deps_ensure_and_bind': ('tools.lazy_deps', 'ensure_and_bind'),
    'tools_homeassistant_tool_u_get_config': ('tools.homeassistant_tool', '_get_config'),
    'tools_homeassistant_tool_u_get_headers': ('tools.homeassistant_tool', '_get_headers'),
    'tools_homeassistant_tool_u_filter_and_summarize': ('tools.homeassistant_tool', '_filter_and_summarize'),
    'tools_homeassistant_tool_u_async_list_entities': ('tools.homeassistant_tool', '_async_list_entities'),
    'tools_homeassistant_tool_u_async_get_state': ('tools.homeassistant_tool', '_async_get_state'),
    'tools_homeassistant_tool_u_build_service_payload': ('tools.homeassistant_tool', '_build_service_payload'),
    'tools_homeassistant_tool_u_parse_service_response': ('tools.homeassistant_tool', '_parse_service_response'),
    'tools_homeassistant_tool_u_async_call_service': ('tools.homeassistant_tool', '_async_call_service'),
    'tools_homeassistant_tool_u_handle_list_entities': ('tools.homeassistant_tool', '_handle_list_entities'),
    'tools_homeassistant_tool_u_handle_get_state': ('tools.homeassistant_tool', '_handle_get_state'),
    'tools_homeassistant_tool_u_handle_call_service': ('tools.homeassistant_tool', '_handle_call_service'),
    'tools_homeassistant_tool_u_async_list_services': ('tools.homeassistant_tool', '_async_list_services'),
    'tools_homeassistant_tool_u_handle_list_services': ('tools.homeassistant_tool', '_handle_list_services'),
    'tools_homeassistant_tool_u_check_ha_available': ('tools.homeassistant_tool', '_check_ha_available'),
    'tools_registry_u_is_registry_register_call': ('tools.registry', '_is_registry_register_call'),
    'tools_registry_u_module_registers_tools': ('tools.registry', '_module_registers_tools'),
    'tools_registry_discover_builtin_tools': ('tools.registry', 'discover_builtin_tools'),
    'tools_registry_u_check_fn_cached': ('tools.registry', '_check_fn_cached'),
    'tools_registry_invalidate_check_fn_cache': ('tools.registry', 'invalidate_check_fn_cache'),
    'tools_registry_u_snapshot_state': ('tools.registry', '_snapshot_state'),
    'tools_registry_u_snapshot_entries': ('tools.registry', '_snapshot_entries'),
    'tools_registry_get_entry': ('tools.registry', 'get_entry'),
    'tools_registry_register_plugin_override_policy': ('tools.registry', 'register_plugin_override_policy'),
    'tools_registry_u_plugin_owner_of': ('tools.registry', '_plugin_owner_of'),
    'tools_registry_u_caller_module': ('tools.registry', '_caller_module'),
    'tools_registry_get_definitions': ('tools.registry', 'get_definitions'),
    'tools_registry_u_normalize_handler_result': ('tools.registry', '_normalize_handler_result'),
    'tools_registry_check_tool_availability': ('tools.registry', 'check_tool_availability'),
    'tools_x_search_tool_u_load_x_search_config': ('tools.x_search_tool', '_load_x_search_config'),
    'tools_x_search_tool_u_get_x_search_model': ('tools.x_search_tool', '_get_x_search_model'),
    'tools_x_search_tool_u_get_x_search_reasoning_effort': ('tools.x_search_tool', '_get_x_search_reasoning_effort'),
    'tools_x_search_tool_u_get_x_search_timeout_seconds': ('tools.x_search_tool', '_get_x_search_timeout_seconds'),
    'tools_x_search_tool_u_get_x_search_retries': ('tools.x_search_tool', '_get_x_search_retries'),
    'tools_x_search_tool_u_resolve_xai_bearer': ('tools.x_search_tool', '_resolve_xai_bearer'),
    'tools_x_search_tool_check_x_search_requirements': ('tools.x_search_tool', 'check_x_search_requirements'),
    'tools_x_search_tool_u_normalize_handles': ('tools.x_search_tool', '_normalize_handles'),
    'tools_x_search_tool_u_parse_iso_date': ('tools.x_search_tool', '_parse_iso_date'),
    'tools_x_search_tool_u_validate_date_range': ('tools.x_search_tool', '_validate_date_range'),
    'tools_x_search_tool_u_extract_inline_citations': ('tools.x_search_tool', '_extract_inline_citations'),
    'tools_x_search_tool_u_http_error_message': ('tools.x_search_tool', '_http_error_message'),
    'tools_x_search_tool_x_search_tool': ('tools.x_search_tool', 'x_search_tool'),
    'tools_x_search_tool_u_handle_x_search': ('tools.x_search_tool', '_handle_x_search'),
    'tools_delegate_tool_u_blocked_toolsets_for_role': ('tools.delegate_tool', '_blocked_toolsets_for_role'),
    'tools_delegate_tool_u_emit_parent_console': ('tools.delegate_tool', '_emit_parent_console'),
    'tools_delegate_tool_u_build_child_progress_callback': ('tools.delegate_tool', '_build_child_progress_callback'),
    'tools_delegate_tool_u_inherit_parent_base_url': ('tools.delegate_tool', '_inherit_parent_base_url'),
    'tools_delegate_tool_u_dump_subagent_timeout_diagnostic': ('tools.delegate_tool', '_dump_subagent_timeout_diagnostic'),
    'tools_delegate_tool_u_spill_summary_to_file': ('tools.delegate_tool', '_spill_summary_to_file'),
    'tools_delegate_tool_u_parent_summary_char_budget': ('tools.delegate_tool', '_parent_summary_char_budget'),
    'tools_delegate_tool_u_apply_summary_budget': ('tools.delegate_tool', '_apply_summary_budget'),
    'tools_delegate_tool_u_run_single_child': ('tools.delegate_tool', '_run_single_child'),
    'tools_delegate_tool_u_resolve_child_credential_pool': ('tools.delegate_tool', '_resolve_child_credential_pool'),
    'tools_delegate_tool_u_resolve_delegation_credentials': ('tools.delegate_tool', '_resolve_delegation_credentials'),
    'tools_delegate_tool_u_build_dynamic_schema_overrides': ('tools.delegate_tool', '_build_dynamic_schema_overrides'),
    'tools_delegate_tool_u_strip_model_hidden_task_fields': ('tools.delegate_tool', '_strip_model_hidden_task_fields'),
    'tools_delegation_live_log_live_transcript_root': ('tools.delegation_live_log', 'live_transcript_root'),
    'tools_delegation_live_log_new_live_delegation_id': ('tools.delegation_live_log', 'new_live_delegation_id'),
    'tools_delegation_live_log_u_one_line': ('tools.delegation_live_log', '_one_line'),
    'tools_delegation_live_log_assistant_text': ('tools.delegation_live_log', 'assistant_text'),
    'tools_delegation_live_log_tool_start': ('tools.delegation_live_log', 'tool_start'),
    'tools_delegation_live_log_tool_result': ('tools.delegation_live_log', 'tool_result'),
    'tools_delegation_live_log_add_stream_delta': ('tools.delegation_live_log', 'add_stream_delta'),
    'tools_delegation_live_log_observe': ('tools.delegation_live_log', 'observe'),
    'tools_delegation_live_log_wrap_progress_callback': ('tools.delegation_live_log', 'wrap_progress_callback'),
    'tools_delegation_live_log_create_live_transcripts': ('tools.delegation_live_log', 'create_live_transcripts'),
    'tools_delegation_live_log_u_manifest_path': ('tools.delegation_live_log', '_manifest_path'),
    'tools_delegation_live_log_update_manifest_statuses': ('tools.delegation_live_log', 'update_manifest_statuses'),
    'tools_delegation_live_log_prune_stale_live_dirs': ('tools.delegation_live_log', 'prune_stale_live_dirs'),
    'tools_environments_modal_u_direct_snapshot_key': ('tools.environments.modal', '_direct_snapshot_key'),
    'tools_environments_modal_u_get_snapshot_restore_candidate': ('tools.environments.modal', '_get_snapshot_restore_candidate'),
    'tools_environments_modal_u_store_direct_snapshot': ('tools.environments.modal', '_store_direct_snapshot'),
    'tools_environments_modal_u_delete_direct_snapshot': ('tools.environments.modal', '_delete_direct_snapshot'),
    'tools_environments_modal_u_ensure_modal_sdk': ('tools.environments.modal', '_ensure_modal_sdk'),
    'tools_environments_modal_u_resolve_modal_image': ('tools.environments.modal', '_resolve_modal_image'),
    'tools_environments_modal_u_run_loop': ('tools.environments.modal', '_run_loop'),
    'tools_environments_modal_run_coroutine': ('tools.environments.modal', 'run_coroutine'),
    'tools_environments_modal_u_modal_upload': ('tools.environments.modal', '_modal_upload'),
    'tools_environments_modal_u_modal_bulk_upload': ('tools.environments.modal', '_modal_bulk_upload'),
    'tools_environments_modal_u_modal_bulk_download': ('tools.environments.modal', '_modal_bulk_download'),
    'tools_environments_modal_u_modal_delete': ('tools.environments.modal', '_modal_delete'),
    'tools_file_state_u_lock_for': ('tools.file_state', '_lock_for'),
    'tools_file_state_record_read': ('tools.file_state', 'record_read'),
    'tools_file_state_note_write': ('tools.file_state', 'note_write'),
    'tools_file_state_check_stale': ('tools.file_state', 'check_stale'),
    'tools_file_state_writes_since': ('tools.file_state', 'writes_since'),
    'tools_file_state_known_reads': ('tools.file_state', 'known_reads'),
    'tools_file_state_record_read_2': ('tools.file_state', 'record_read'),
    'tools_file_state_note_write_2': ('tools.file_state', 'note_write'),
    'tools_file_state_check_stale_2': ('tools.file_state', 'check_stale'),
    'tools_file_state_writes_since_2': ('tools.file_state', 'writes_since'),
    'tools_file_state_known_reads_2': ('tools.file_state', 'known_reads'),
    'tools_mcp_dashboard_oauth_publish_authorization_url': ('tools.mcp_dashboard_oauth', 'publish_authorization_url'),
    'tools_mcp_dashboard_oauth_wait_for_authorization_url': ('tools.mcp_dashboard_oauth', 'wait_for_authorization_url'),
    'tools_mcp_dashboard_oauth_deliver_callback': ('tools.mcp_dashboard_oauth', 'deliver_callback'),
    'tools_mcp_dashboard_oauth_wait_for_callback': ('tools.mcp_dashboard_oauth', 'wait_for_callback'),
    'tools_mcp_dashboard_oauth_mark_approved': ('tools.mcp_dashboard_oauth', 'mark_approved'),
    'tools_mcp_dashboard_oauth_mark_error': ('tools.mcp_dashboard_oauth', 'mark_error'),
    'tools_mcp_dashboard_oauth_mark_worker_done': ('tools.mcp_dashboard_oauth', 'mark_worker_done'),
    'tools_mcp_dashboard_oauth_worker_done': ('tools.mcp_dashboard_oauth', 'worker_done'),
    'tools_mcp_dashboard_oauth_dashboard_oauth_flow': ('tools.mcp_dashboard_oauth', 'dashboard_oauth_flow'),
    'tools_mcp_dashboard_oauth_get_dashboard_oauth_flow': ('tools.mcp_dashboard_oauth', 'get_dashboard_oauth_flow'),
    'tools_online_research_clear_expired': ('tools.online_research', 'clear_expired'),
    'tools_online_research_u__aenter__': ('tools.online_research', '__aenter__'),
    'tools_online_research_u__aexit__': ('tools.online_research', '__aexit__'),
    'tools_online_research_search_duckduckgo': ('tools.online_research', 'search_duckduckgo'),
    'tools_online_research_search_brave': ('tools.online_research', 'search_brave'),
    'tools_online_research_search_google_cse': ('tools.online_research', 'search_google_cse'),
    'tools_online_research_get_researcher': ('tools.online_research', 'get_researcher'),
    'tools_online_research_close_researcher': ('tools.online_research', 'close_researcher'),
    'tools_online_research_research_model_benchmarks': ('tools.online_research', 'research_model_benchmarks'),
    'tools_online_research_research_general': ('tools.online_research', 'research_general'),
    'tools_image_source_resolve_image_source': ('tools.image_source', 'resolve_image_source'),
    'tools_image_source_u_resolve_data_url': ('tools.image_source', '_resolve_data_url'),
    'tools_image_source_u_http_block_reason': ('tools.image_source', '_http_block_reason'),
    'tools_image_source_u_download_to_bytes': ('tools.image_source', '_download_to_bytes'),
    'tools_image_source_u_is_local_terminal_backend': ('tools.image_source', '_is_local_terminal_backend'),
    'tools_image_source_u_media_cache_roots': ('tools.image_source', '_media_cache_roots'),
    'tools_image_source_u_permitted_host_read_target': ('tools.image_source', '_permitted_host_read_target'),
    'tools_image_source_u_resolve_container_fallback': ('tools.image_source', '_resolve_container_fallback'),
    'tools_kanban_tools_u_is_delegated_child_context': ('tools.kanban_tools', '_is_delegated_child_context'),
    'tools_kanban_tools_u_reject_delegated_child_mutation': ('tools.kanban_tools', '_reject_delegated_child_mutation'),
    'tools_kanban_tools_u_connect': ('tools.kanban_tools', '_connect'),
    'tools_kanban_tools_heartbeat_current_worker_from_env': ('tools.kanban_tools', 'heartbeat_current_worker_from_env'),
    'tools_kanban_tools_u_handle_attach': ('tools.kanban_tools', '_handle_attach'),
    'tools_kanban_tools_u_download_url_with_cap': ('tools.kanban_tools', '_download_url_with_cap'),
    'tools_kanban_tools_u_handle_attach_url': ('tools.kanban_tools', '_handle_attach_url'),
    'tools_kanban_tools_u_handle_attachments': ('tools.kanban_tools', '_handle_attachments'),
    'tools_tool_backend_helpers_managed_nous_tools_enabled': ('tools.tool_backend_helpers', 'managed_nous_tools_enabled'),
    'tools_tool_backend_helpers_normalize_browser_cloud_provider': ('tools.tool_backend_helpers', 'normalize_browser_cloud_provider'),
    'tools_tool_backend_helpers_coerce_modal_mode': ('tools.tool_backend_helpers', 'coerce_modal_mode'),
    'tools_tool_backend_helpers_normalize_modal_mode': ('tools.tool_backend_helpers', 'normalize_modal_mode'),
    'tools_tool_backend_helpers_has_direct_modal_credentials': ('tools.tool_backend_helpers', 'has_direct_modal_credentials'),
    'tools_tool_backend_helpers_resolve_modal_backend_state': ('tools.tool_backend_helpers', 'resolve_modal_backend_state'),
    'tools_tool_backend_helpers_resolve_openai_audio_api_key': ('tools.tool_backend_helpers', 'resolve_openai_audio_api_key'),
    'tools_tool_backend_helpers_prefers_gateway': ('tools.tool_backend_helpers', 'prefers_gateway'),
    'tools_skills_hub_u_referenced_support_paths': ('tools.skills_hub', '_referenced_support_paths'),
    'tools_skills_hub_source_url_for_bundle': ('tools.skills_hub', 'source_url_for_bundle'),
    'tools_skills_hub_u_ssrf_safe_http_get': ('tools.skills_hub', '_ssrf_safe_http_get'),
    'tools_skills_hub_u_fetch_file_bytes': ('tools.skills_hub', '_fetch_file_bytes'),
    'tools_skills_hub_u_fetch_bytes': ('tools.skills_hub', '_fetch_bytes'),
    'tools_skills_hub_u_find_skill_dir': ('tools.skills_hub', '_find_skill_dir'),
    'tools_skills_hub_u_parse_frontmatter': ('tools.skills_hub', '_parse_frontmatter'),
    'tools_xai_video_tools_u_configured_for_xai_video': ('tools.xai_video_tools', '_configured_for_xai_video'),
    'tools_xai_video_tools_u_check_xai_video_requirements': ('tools.xai_video_tools', '_check_xai_video_requirements'),
    'tools_xai_video_tools_u_clean_string': ('tools.xai_video_tools', '_clean_string'),
    'tools_xai_video_tools_u_provider_not_configured_error': ('tools.xai_video_tools', '_provider_not_configured_error'),
    'tools_xai_video_tools_u_normalize_public_video_url': ('tools.xai_video_tools', '_normalize_public_video_url'),
    'tools_xai_video_tools_u_handle_xai_video_edit': ('tools.xai_video_tools', '_handle_xai_video_edit'),
    'tools_xai_video_tools_u_handle_xai_video_extend': ('tools.xai_video_tools', '_handle_xai_video_extend'),
    'tools_computer_use_permissions_u_resolve_driver_cmd': ('tools.computer_use.permissions', '_resolve_driver_cmd'),
    'tools_computer_use_permissions_u_child_env': ('tools.computer_use.permissions', '_child_env'),
    'tools_computer_use_permissions_u_json_out': ('tools.computer_use.permissions', '_json_out'),
    'tools_computer_use_permissions_u_mac_permissions': ('tools.computer_use.permissions', '_mac_permissions'),
    'tools_computer_use_permissions_computer_use_status': ('tools.computer_use.permissions', 'computer_use_status'),
    'tools_computer_use_permissions_request_permissions_grant': ('tools.computer_use.permissions', 'request_permissions_grant'),
    'tools_project_tools_set_project_workspace_callback': ('tools.project_tools', 'set_project_workspace_callback'),
    'tools_project_tools_u_primary_path': ('tools.project_tools', '_primary_path'),
    'tools_project_tools_u_apply_workspace': ('tools.project_tools', '_apply_workspace'),
    'tools_project_tools_project_list': ('tools.project_tools', 'project_list'),
    'tools_project_tools_project_create': ('tools.project_tools', 'project_create'),
    'tools_project_tools_project_switch': ('tools.project_tools', 'project_switch'),
    'tools_credential_files_register_credential_file': ('tools.credential_files', 'register_credential_file'),
    'tools_credential_files_register_credential_files': ('tools.credential_files', 'register_credential_files'),
    'tools_credential_files_iter_skills_files': ('tools.credential_files', 'iter_skills_files'),
    'tools_credential_files_from_agent_visible_cache_path': ('tools.credential_files', 'from_agent_visible_cache_path'),
    'tools_credential_files_iter_cache_files': ('tools.credential_files', 'iter_cache_files'),
    'tools_hook_output_spill_u_coerce_non_negative_int': ('tools.hook_output_spill', '_coerce_non_negative_int'),
    'tools_hook_output_spill_get_spill_config': ('tools.hook_output_spill', 'get_spill_config'),
    'tools_hook_output_spill_u_resolve_spill_dir': ('tools.hook_output_spill', '_resolve_spill_dir'),
    'tools_hook_output_spill_u_build_preview': ('tools.hook_output_spill', '_build_preview'),
    'tools_hook_output_spill_spill_if_oversized': ('tools.hook_output_spill', 'spill_if_oversized'),
    'tools_session_search_tool_u_is_compaction_summary': ('tools.session_search_tool', '_is_compaction_summary'),
    'tools_session_search_tool_u_resolve_lineage': ('tools.session_search_tool', '_resolve_lineage'),
    'tools_session_search_tool_u_is_compression_ended': ('tools.session_search_tool', '_is_compression_ended'),
    'tools_session_search_tool_u_is_compacted_message': ('tools.session_search_tool', '_is_compacted_message'),
    'tools_session_search_tool_u_annotate_rebuild_status': ('tools.session_search_tool', '_annotate_rebuild_status'),
    'tools_browser_tool_u_is_headed_mode': ('tools.browser_tool', '_is_headed_mode'),
    'tools_browser_tool_u_store_full_snapshot': ('tools.browser_tool', '_store_full_snapshot'),
    'tools_browser_tool_u_restrict_browser_evaluate': ('tools.browser_tool', '_restrict_browser_evaluate'),
    'tools_browser_tool_u_camofox_current_page_private_url': ('tools.browser_tool', '_camofox_current_page_private_url'),
    'tools_checkpoint_manager_u_volume_evidence': ('tools.checkpoint_manager', '_volume_evidence'),
    'tools_checkpoint_manager_u_pre_v2_shadow_repos': ('tools.checkpoint_manager', '_pre_v2_shadow_repos'),
    'tools_checkpoint_manager_u_workdir_is_observably_gone': ('tools.checkpoint_manager', '_workdir_is_observably_gone'),
    'tools_checkpoint_manager_u_dir_has_any_entry': ('tools.checkpoint_manager', '_dir_has_any_entry'),
    'tools_computer_use_doctor_u_cua_child_env': ('tools.computer_use.doctor', '_cua_child_env'),
    'tools_computer_use_doctor_u_sanitized_cua_env': ('tools.computer_use.doctor', '_sanitized_cua_env'),
    'tools_computer_use_doctor_u_drive_health_report': ('tools.computer_use.doctor', '_drive_health_report'),
    'tools_computer_use_doctor_u_print_text_report': ('tools.computer_use.doctor', '_print_text_report'),
    'tools_skill_provenance_set_current_write_origin': ('tools.skill_provenance', 'set_current_write_origin'),
    'tools_skill_provenance_reset_current_write_origin': ('tools.skill_provenance', 'reset_current_write_origin'),
    'tools_skill_provenance_get_current_write_origin': ('tools.skill_provenance', 'get_current_write_origin'),
    'tools_skill_provenance_is_background_review': ('tools.skill_provenance', 'is_background_review'),
    'tools_web_tools_u_web_extract_url': ('tools.web_tools', '_web_extract_url'),
    'tools_web_tools_u_registered_web_provider': ('tools.web_tools', '_registered_web_provider'),
    'tools_web_tools_u_registered_web_provider_available': ('tools.web_tools', '_registered_web_provider_available'),
    'tools_web_tools_u_list_registered_web_providers': ('tools.web_tools', '_list_registered_web_providers'),
    'tools_env_probe_u_probe_worker': ('tools.env_probe', '_probe_worker'),
    'tools_env_probe_u_ensure_probe_started': ('tools.env_probe', '_ensure_probe_started'),
    'tools_env_probe_warm_environment_probe_async': ('tools.env_probe', 'warm_environment_probe_async'),
    'tools_mcp_stdio_watchdog_u_is_orphaned': ('tools.mcp_stdio_watchdog', '_is_orphaned'),
    'tools_mcp_stdio_watchdog_u_terminate_process_group': ('tools.mcp_stdio_watchdog', '_terminate_process_group'),
    'tools_mcp_stdio_watchdog_u_watchdog_loop': ('tools.mcp_stdio_watchdog', '_watchdog_loop'),
    'tools_open_preview_tool_u_normalize_target': ('tools.open_preview_tool', '_normalize_target'),
    'tools_open_preview_tool_open_preview_tool': ('tools.open_preview_tool', 'open_preview_tool'),
    'tools_open_preview_tool_check_open_preview_requirements': ('tools.open_preview_tool', 'check_open_preview_requirements'),
    'tools_tts_streaming_mark_speech_interrupted': ('tools.tts_streaming', 'mark_speech_interrupted'),
    'tools_tts_streaming_take_speech_interrupted': ('tools.tts_streaming', 'take_speech_interrupted'),
    'tools_tts_streaming_resolve_streaming_provider': ('tools.tts_streaming', 'resolve_streaming_provider'),
    'tools_computer_use_backend_list_windows': ('tools.computer_use.backend', 'list_windows'),
    'tools_computer_use_backend_set_value': ('tools.computer_use.backend', 'set_value'),
    'tools_focus_pane_tool_focus_pane_tool': ('tools.focus_pane_tool', 'focus_pane_tool'),
    'tools_focus_pane_tool_check_focus_pane_requirements': ('tools.focus_pane_tool', 'check_focus_pane_requirements'),
    'tools_image_generation_tool_check_image_generation_requirements': ('tools.image_generation_tool', 'check_image_generation_requirements'),
    'tools_image_generation_tool_u_dispatch_to_plugin_provider': ('tools.image_generation_tool', '_dispatch_to_plugin_provider'),
    'tools_microsoft_graph_client_post_json': ('tools.microsoft_graph_client', 'post_json'),
    'tools_microsoft_graph_client_u_request': ('tools.microsoft_graph_client', '_request'),
    'tools_send_message_tool_u_media_caption_split': ('tools.send_message_tool', '_media_caption_split'),
    'tools_send_message_tool_u_resolve_slack_user_target': ('tools.send_message_tool', '_resolve_slack_user_target'),
    'tools_thread_context_u_callback_api': ('tools.thread_context', '_callback_api'),
    'tools_thread_context_propagate_context_to_thread': ('tools.thread_context', 'propagate_context_to_thread'),
    'tools_video_generation_tool_u_resolve_active_provider': ('tools.video_generation_tool', '_resolve_active_provider'),
    'tools_video_generation_tool_u_handle_video_generate': ('tools.video_generation_tool', '_handle_video_generate'),
    'tools_voice_mode_cancel': ('tools.voice_mode', 'cancel'),
    'tools_voice_mode_cancel_2': ('tools.voice_mode', 'cancel'),
    'tools_ansi_strip_sanitize_display_text': ('tools.ansi_strip', 'sanitize_display_text'),
    'tools_binary_extensions_has_binary_extension': ('tools.binary_extensions', 'has_binary_extension'),
    'tools_browser_camofox_state_get_camofox_state_dir': ('tools.browser_camofox_state', 'get_camofox_state_dir'),
    'tools_budget_config_resolve_threshold': ('tools.budget_config', 'resolve_threshold'),
    'tools_clarify_gateway_resolve_clarify_timeout': ('tools.clarify_gateway', 'resolve_clarify_timeout'),
    'tools_daemon_pool_u_adjust_thread_count': ('tools.daemon_pool', '_adjust_thread_count'),
    'tools_debug_helpers_log_call': ('tools.debug_helpers', 'log_call'),
    'tools_desktop_ui_set_emitter': ('tools.desktop_ui', 'set_emitter'),
    'tools_environments_file_sync_u_resolve_host_path': ('tools.environments.file_sync', '_resolve_host_path'),
    'tools_environments_managed_mod_u_request': ('tools.environments.managed_modal', '_request'),
    'tools_feishu_drive_tool_u_do_request': ('tools.feishu_drive_tool', '_do_request'),
    'tools_interrupt_clear_current_thread_interrupt': ('tools.interrupt', 'clear_current_thread_interrupt'),
    'tools_mixture_of_agents_tool_u_build_auth_header': ('tools.mixture_of_agents_tool', '_build_auth_header'),
    'tools_moa_performance_u_build_auth_header': ('tools.moa_performance', '_build_auth_header'),
    'tools_skills_guard_scan_file': ('tools.skills_guard', 'scan_file'),
    'tools_tirith_security_u_record_tirith_crash': ('tools.tirith_security', '_record_tirith_crash'),
    'tools_tool_search_u_safe_float': ('tools.tool_search', '_safe_float'),
    'tools_website_policy_check_website_access': ('tools.website_policy', 'check_website_access'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_tools_remaining_wrappers.py <cases.json>\n"); return 2
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
