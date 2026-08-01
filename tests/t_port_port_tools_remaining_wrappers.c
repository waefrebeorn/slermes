/* AUTO-GENERATED integration oracle harness for port_tools_remaining_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_tools_remaining_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int tools_computer_use_tool_u_canon_key_combo(const char *);
extern int tools_computer_use_tool_reset_backend_for_tests(const char *);
extern int tools_computer_use_tool_type_text(const char *);
extern int tools_computer_use_tool_list_apps(const char *);
extern int tools_computer_use_tool_list_windows(const char *);
extern int tools_computer_use_tool_set_value(const char *);
extern int tools_computer_use_tool_u_request_approval(const char *);
extern int tools_computer_use_tool_u_summarize_action(const char *);
extern int tools_computer_use_tool_u_image_dimensions_from_b64(const char *);
extern int tools_computer_use_tool_u_coerce_max_elements(const char *);
extern int tools_computer_use_tool_u_shrink_capture_for_vision(const char *);
extern int tools_computer_use_tool_u_should_route_through_aux_vision(const char *);
extern int tools_computer_use_tool_u_capture_after_mode(const char *);
extern int tools_computer_use_tool_u_route_capture_through_aux_vision(const char *);
extern int tools_computer_use_tool_u_maybe_follow_capture(const char *);
extern int tools_computer_use_tool_u_format_elements(const char *);
extern int tools_computer_use_tool_u_element_to_dict(const char *);
extern int tools_computer_use_tool_check_computer_use_requirements(const char *);
extern int tools_lazy_deps_u_format(const char *);
extern int tools_lazy_deps_u_python_abi_tag(const char *);
extern int tools_lazy_deps_u_lazy_install_target(const char *);
extern int tools_lazy_deps_u_ensure_target_ready(const char *);
extern int tools_lazy_deps_u_activate_target_on_syspath(const char *);
extern int tools_lazy_deps_activate_durable_lazy_target(const char *);
extern int tools_lazy_deps_u_allow_lazy_installs(const char *);
extern int tools_lazy_deps_u_unsupported_feature_reason(const char *);
extern int tools_lazy_deps_u_is_satisfied(const char *);
extern int tools_lazy_deps_u_is_present(const char *);
extern int tools_lazy_deps_u_core_constraints_file(const char *);
extern int tools_lazy_deps_u_venv_pip_install(const char *);
extern int tools_lazy_deps_active_features(const char *);
extern int tools_lazy_deps_refresh_active_features(const char *);
extern int tools_lazy_deps_ensure_and_bind(const char *);
extern int tools_homeassistant_tool_u_get_config(const char *);
extern int tools_homeassistant_tool_u_get_headers(const char *);
extern int tools_homeassistant_tool_u_filter_and_summarize(const char *);
extern int tools_homeassistant_tool_u_async_list_entities(const char *);
extern int tools_homeassistant_tool_u_async_get_state(const char *);
extern int tools_homeassistant_tool_u_build_service_payload(const char *);
extern int tools_homeassistant_tool_u_parse_service_response(const char *);
extern int tools_homeassistant_tool_u_async_call_service(const char *);
extern int tools_homeassistant_tool_u_handle_list_entities(const char *);
extern int tools_homeassistant_tool_u_handle_get_state(const char *);
extern int tools_homeassistant_tool_u_handle_call_service(const char *);
extern int tools_homeassistant_tool_u_async_list_services(const char *);
extern int tools_homeassistant_tool_u_handle_list_services(const char *);
extern int tools_homeassistant_tool_u_check_ha_available(const char *);
extern int tools_registry_u_is_registry_register_call(const char *);
extern int tools_registry_u_module_registers_tools(const char *);
extern int tools_registry_discover_builtin_tools(const char *);
extern int tools_registry_u_check_fn_cached(const char *);
extern int tools_registry_invalidate_check_fn_cache(const char *);
extern int tools_registry_u_snapshot_state(const char *);
extern int tools_registry_u_snapshot_entries(const char *);
extern int tools_registry_get_entry(const char *);
extern int tools_registry_register_plugin_override_policy(const char *);
extern int tools_registry_u_plugin_owner_of(const char *);
extern int tools_registry_u_caller_module(const char *);
extern int tools_registry_get_definitions(const char *);
extern int tools_registry_u_normalize_handler_result(const char *);
extern int tools_registry_check_tool_availability(const char *);
extern int tools_x_search_tool_u_load_x_search_config(const char *);
extern int tools_x_search_tool_u_get_x_search_model(const char *);
extern int tools_x_search_tool_u_get_x_search_reasoning_effort(const char *);
extern int tools_x_search_tool_u_get_x_search_timeout_seconds(const char *);
extern int tools_x_search_tool_u_get_x_search_retries(const char *);
extern int tools_x_search_tool_u_resolve_xai_bearer(const char *);
extern int tools_x_search_tool_check_x_search_requirements(const char *);
extern int tools_x_search_tool_u_normalize_handles(const char *);
extern int tools_x_search_tool_u_parse_iso_date(const char *);
extern int tools_x_search_tool_u_validate_date_range(const char *);
extern int tools_x_search_tool_u_extract_inline_citations(const char *);
extern int tools_x_search_tool_u_http_error_message(const char *);
extern int tools_x_search_tool_x_search_tool(const char *);
extern int tools_x_search_tool_u_handle_x_search(const char *);
extern int tools_delegate_tool_u_blocked_toolsets_for_role(const char *);
extern int tools_delegate_tool_u_emit_parent_console(const char *);
extern int tools_delegate_tool_u_build_child_progress_callback(const char *);
extern int tools_delegate_tool_u_inherit_parent_base_url(const char *);
extern int tools_delegate_tool_u_dump_subagent_timeout_diagnostic(const char *);
extern int tools_delegate_tool_u_spill_summary_to_file(const char *);
extern int tools_delegate_tool_u_parent_summary_char_budget(const char *);
extern int tools_delegate_tool_u_apply_summary_budget(const char *);
extern int tools_delegate_tool_u_run_single_child(const char *);
extern int tools_delegate_tool_u_resolve_child_credential_pool(const char *);
extern int tools_delegate_tool_u_resolve_delegation_credentials(const char *);
extern int tools_delegate_tool_u_build_dynamic_schema_overrides(const char *);
extern int tools_delegate_tool_u_strip_model_hidden_task_fields(const char *);
extern int tools_delegation_live_log_live_transcript_root(const char *);
extern int tools_delegation_live_log_new_live_delegation_id(const char *);
extern int tools_delegation_live_log_u_one_line(const char *);
extern int tools_delegation_live_log_assistant_text(const char *);
extern int tools_delegation_live_log_tool_start(const char *);
extern int tools_delegation_live_log_tool_result(const char *);
extern int tools_delegation_live_log_add_stream_delta(const char *);
extern int tools_delegation_live_log_observe(const char *);
extern int tools_delegation_live_log_wrap_progress_callback(const char *);
extern int tools_delegation_live_log_create_live_transcripts(const char *);
extern int tools_delegation_live_log_u_manifest_path(const char *);
extern int tools_delegation_live_log_update_manifest_statuses(const char *);
extern int tools_delegation_live_log_prune_stale_live_dirs(const char *);
extern int tools_environments_modal_u_direct_snapshot_key(const char *);
extern int tools_environments_modal_u_get_snapshot_restore_candidate(const char *);
extern int tools_environments_modal_u_store_direct_snapshot(const char *);
extern int tools_environments_modal_u_delete_direct_snapshot(const char *);
extern int tools_environments_modal_u_ensure_modal_sdk(const char *);
extern int tools_environments_modal_u_resolve_modal_image(const char *);
extern int tools_environments_modal_u_run_loop(const char *);
extern int tools_environments_modal_run_coroutine(const char *);
extern int tools_environments_modal_u_modal_upload(const char *);
extern int tools_environments_modal_u_modal_bulk_upload(const char *);
extern int tools_environments_modal_u_modal_bulk_download(const char *);
extern int tools_environments_modal_u_modal_delete(const char *);
extern int tools_file_state_u_lock_for(const char *);
extern int tools_file_state_record_read(const char *);
extern int tools_file_state_note_write(const char *);
extern int tools_file_state_check_stale(const char *);
extern int tools_file_state_writes_since(const char *);
extern int tools_file_state_known_reads(const char *);
extern int tools_file_state_record_read_2(const char *);
extern int tools_file_state_note_write_2(const char *);
extern int tools_file_state_check_stale_2(const char *);
extern int tools_file_state_writes_since_2(const char *);
extern int tools_file_state_known_reads_2(const char *);
extern int tools_mcp_dashboard_oauth_publish_authorization_url(const char *);
extern int tools_mcp_dashboard_oauth_wait_for_authorization_url(const char *);
extern int tools_mcp_dashboard_oauth_deliver_callback(const char *);
extern int tools_mcp_dashboard_oauth_wait_for_callback(const char *);
extern int tools_mcp_dashboard_oauth_mark_approved(const char *);
extern int tools_mcp_dashboard_oauth_mark_error(const char *);
extern int tools_mcp_dashboard_oauth_mark_worker_done(const char *);
extern int tools_mcp_dashboard_oauth_worker_done(const char *);
extern int tools_mcp_dashboard_oauth_dashboard_oauth_flow(const char *);
extern int tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(const char *);
extern int tools_online_research_clear_expired(const char *);
extern int tools_online_research_u__aenter__(const char *);
extern int tools_online_research_u__aexit__(const char *);
extern int tools_online_research_search_duckduckgo(const char *);
extern int tools_online_research_search_brave(const char *);
extern int tools_online_research_search_google_cse(const char *);
extern int tools_online_research_get_researcher(const char *);
extern int tools_online_research_close_researcher(const char *);
extern int tools_online_research_research_model_benchmarks(const char *);
extern int tools_online_research_research_general(const char *);
extern int tools_image_source_resolve_image_source(const char *);
extern int tools_image_source_u_resolve_data_url(const char *);
extern int tools_image_source_u_http_block_reason(const char *);
extern int tools_image_source_u_download_to_bytes(const char *);
extern int tools_image_source_u_is_local_terminal_backend(const char *);
extern int tools_image_source_u_media_cache_roots(const char *);
extern int tools_image_source_u_permitted_host_read_target(const char *);
extern int tools_image_source_u_resolve_container_fallback(const char *);
extern int tools_kanban_tools_u_is_delegated_child_context(const char *);
extern int tools_kanban_tools_u_reject_delegated_child_mutation(const char *);
extern int tools_kanban_tools_u_connect(const char *);
extern int tools_kanban_tools_heartbeat_current_worker_from_env(const char *);
extern int tools_kanban_tools_u_handle_attach(const char *);
extern int tools_kanban_tools_u_download_url_with_cap(const char *);
extern int tools_kanban_tools_u_handle_attach_url(const char *);
extern int tools_kanban_tools_u_handle_attachments(const char *);
extern int tools_tool_backend_helpers_managed_nous_tools_enabled(const char *);
extern int tools_tool_backend_helpers_normalize_browser_cloud_provider(const char *);
extern int tools_tool_backend_helpers_coerce_modal_mode(const char *);
extern int tools_tool_backend_helpers_normalize_modal_mode(const char *);
extern int tools_tool_backend_helpers_has_direct_modal_credentials(const char *);
extern int tools_tool_backend_helpers_resolve_modal_backend_state(const char *);
extern int tools_tool_backend_helpers_resolve_openai_audio_api_key(const char *);
extern int tools_tool_backend_helpers_prefers_gateway(const char *);
extern int tools_skills_hub_u_referenced_support_paths(const char *);
extern int tools_skills_hub_source_url_for_bundle(const char *);
extern int tools_skills_hub_u_ssrf_safe_http_get(const char *);
extern int tools_skills_hub_u_fetch_file_bytes(const char *);
extern int tools_skills_hub_u_fetch_bytes(const char *);
extern int tools_skills_hub_u_find_skill_dir(const char *);
extern int tools_skills_hub_u_parse_frontmatter(const char *);
extern int tools_xai_video_tools_u_configured_for_xai_video(const char *);
extern int tools_xai_video_tools_u_check_xai_video_requirements(const char *);
extern int tools_xai_video_tools_u_clean_string(const char *);
extern int tools_xai_video_tools_u_provider_not_configured_error(const char *);
extern int tools_xai_video_tools_u_normalize_public_video_url(const char *);
extern int tools_xai_video_tools_u_handle_xai_video_edit(const char *);
extern int tools_xai_video_tools_u_handle_xai_video_extend(const char *);
extern int tools_computer_use_permissions_u_resolve_driver_cmd(const char *);
extern int tools_computer_use_permissions_u_child_env(const char *);
extern int tools_computer_use_permissions_u_json_out(const char *);
extern int tools_computer_use_permissions_u_mac_permissions(const char *);
extern int tools_computer_use_permissions_computer_use_status(const char *);
extern int tools_computer_use_permissions_request_permissions_grant(const char *);
extern int tools_project_tools_set_project_workspace_callback(const char *);
extern int tools_project_tools_u_primary_path(const char *);
extern int tools_project_tools_u_apply_workspace(const char *);
extern int tools_project_tools_project_list(const char *);
extern int tools_project_tools_project_create(const char *);
extern int tools_project_tools_project_switch(const char *);
extern int tools_credential_files_register_credential_file(const char *);
extern int tools_credential_files_register_credential_files(const char *);
extern int tools_credential_files_iter_skills_files(const char *);
extern int tools_credential_files_from_agent_visible_cache_path(const char *);
extern int tools_credential_files_iter_cache_files(const char *);
extern int tools_hook_output_spill_u_coerce_non_negative_int(const char *);
extern int tools_hook_output_spill_get_spill_config(const char *);
extern int tools_hook_output_spill_u_resolve_spill_dir(const char *);
extern int tools_hook_output_spill_u_build_preview(const char *);
extern int tools_hook_output_spill_spill_if_oversized(const char *);
extern int tools_session_search_tool_u_is_compaction_summary(const char *);
extern int tools_session_search_tool_u_resolve_lineage(const char *);
extern int tools_session_search_tool_u_is_compression_ended(const char *);
extern int tools_session_search_tool_u_is_compacted_message(const char *);
extern int tools_session_search_tool_u_annotate_rebuild_status(const char *);
extern int tools_browser_tool_u_is_headed_mode(const char *);
extern int tools_browser_tool_u_store_full_snapshot(const char *);
extern int tools_browser_tool_u_restrict_browser_evaluate(const char *);
extern int tools_browser_tool_u_camofox_current_page_private_url(const char *);
extern int tools_checkpoint_manager_u_volume_evidence(const char *);
extern int tools_checkpoint_manager_u_pre_v2_shadow_repos(const char *);
extern int tools_checkpoint_manager_u_workdir_is_observably_gone(const char *);
extern int tools_checkpoint_manager_u_dir_has_any_entry(const char *);
extern int tools_computer_use_doctor_u_cua_child_env(const char *);
extern int tools_computer_use_doctor_u_sanitized_cua_env(const char *);
extern int tools_computer_use_doctor_u_drive_health_report(const char *);
extern int tools_computer_use_doctor_u_print_text_report(const char *);
extern int tools_skill_provenance_set_current_write_origin(const char *);
extern int tools_skill_provenance_reset_current_write_origin(const char *);
extern int tools_skill_provenance_get_current_write_origin(const char *);
extern int tools_skill_provenance_is_background_review(const char *);
extern int tools_web_tools_u_web_extract_url(const char *);
extern int tools_web_tools_u_registered_web_provider(const char *);
extern int tools_web_tools_u_registered_web_provider_available(const char *);
extern int tools_web_tools_u_list_registered_web_providers(const char *);
extern int tools_env_probe_u_probe_worker(const char *);
extern int tools_env_probe_u_ensure_probe_started(const char *);
extern int tools_env_probe_warm_environment_probe_async(const char *);
extern int tools_mcp_stdio_watchdog_u_is_orphaned(const char *);
extern int tools_mcp_stdio_watchdog_u_terminate_process_group(const char *);
extern int tools_mcp_stdio_watchdog_u_watchdog_loop(const char *);
extern int tools_open_preview_tool_u_normalize_target(const char *);
extern int tools_open_preview_tool_open_preview_tool(const char *);
extern int tools_open_preview_tool_check_open_preview_requirements(const char *);
extern int tools_tts_streaming_mark_speech_interrupted(const char *);
extern int tools_tts_streaming_take_speech_interrupted(const char *);
extern int tools_tts_streaming_resolve_streaming_provider(const char *);
extern int tools_computer_use_backend_list_windows(const char *);
extern int tools_computer_use_backend_set_value(const char *);
extern int tools_focus_pane_tool_focus_pane_tool(const char *);
extern int tools_focus_pane_tool_check_focus_pane_requirements(const char *);
extern int tools_image_generation_tool_check_image_generation_requirements(const char *);
extern int tools_image_generation_tool_u_dispatch_to_plugin_provider(const char *);
extern int tools_microsoft_graph_client_post_json(const char *);
extern int tools_microsoft_graph_client_u_request(const char *);
extern int tools_send_message_tool_u_media_caption_split(const char *);
extern int tools_send_message_tool_u_resolve_slack_user_target(const char *);
extern int tools_thread_context_u_callback_api(const char *);
extern int tools_thread_context_propagate_context_to_thread(const char *);
extern int tools_video_generation_tool_u_resolve_active_provider(const char *);
extern int tools_video_generation_tool_u_handle_video_generate(const char *);
extern int tools_voice_mode_cancel(const char *);
extern int tools_voice_mode_cancel_2(const char *);
extern int tools_ansi_strip_sanitize_display_text(const char *);
extern int tools_binary_extensions_has_binary_extension(const char *);
extern int tools_browser_camofox_state_get_camofox_state_dir(const char *);
extern int tools_budget_config_resolve_threshold(const char *);
extern int tools_clarify_gateway_resolve_clarify_timeout(const char *);
extern int tools_daemon_pool_u_adjust_thread_count(const char *);
extern int tools_debug_helpers_log_call(const char *);
extern int tools_desktop_ui_set_emitter(const char *);
extern int tools_environments_file_sync_u_resolve_host_path(const char *);
extern int tools_environments_managed_mod_u_request(const char *);
extern int tools_feishu_drive_tool_u_do_request(const char *);
extern int tools_interrupt_clear_current_thread_interrupt(const char *);
extern int tools_mixture_of_agents_tool_u_build_auth_header(const char *);
extern int tools_moa_performance_u_build_auth_header(const char *);
extern int tools_skills_guard_scan_file(const char *);
extern int tools_tirith_security_u_record_tirith_crash(const char *);
extern int tools_tool_search_u_safe_float(const char *);
extern int tools_website_policy_check_website_access(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_tools_computer_use_tool_u_canon_key_combo(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_canon_key_combo(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_canon_key_combo"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_reset_backend_for_tests(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_reset_backend_for_tests(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_reset_backend_for_tests"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_type_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_type_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_type_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_list_apps(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_list_apps(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_list_apps"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_list_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_list_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_list_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_set_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_set_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_set_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_request_approval(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_request_approval(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_request_approval"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_summarize_action(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_summarize_action(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_summarize_action"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_image_dimensions_from_b64(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_image_dimensions_from_b64(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_image_dimensions_from_b64"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_coerce_max_elements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_coerce_max_elements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_coerce_max_elements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_shrink_capture_for_vision(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_shrink_capture_for_vision(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_shrink_capture_for_vision"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_should_route_through_aux_vision(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_should_route_through_aux_vision(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_should_route_through_aux_vision"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_capture_after_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_capture_after_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_capture_after_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_route_capture_through_aux_vision(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_route_capture_through_aux_vision(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_route_capture_through_aux_vision"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_maybe_follow_capture(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_maybe_follow_capture(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_maybe_follow_capture"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_format_elements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_format_elements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_format_elements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_u_element_to_dict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_u_element_to_dict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_u_element_to_dict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_tool_check_computer_use_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_tool_check_computer_use_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_tool_check_computer_use_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_format(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_format(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_format"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_python_abi_tag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_python_abi_tag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_python_abi_tag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_lazy_install_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_lazy_install_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_lazy_install_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_ensure_target_ready(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_ensure_target_ready(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_ensure_target_ready"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_activate_target_on_syspath(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_activate_target_on_syspath(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_activate_target_on_syspath"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_activate_durable_lazy_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_activate_durable_lazy_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_activate_durable_lazy_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_allow_lazy_installs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_allow_lazy_installs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_allow_lazy_installs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_unsupported_feature_reason(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_unsupported_feature_reason(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_unsupported_feature_reason"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_is_satisfied(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_is_satisfied(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_is_satisfied"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_is_present(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_is_present(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_is_present"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_core_constraints_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_core_constraints_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_core_constraints_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_u_venv_pip_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_u_venv_pip_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_u_venv_pip_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_active_features(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_active_features(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_active_features"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_refresh_active_features(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_refresh_active_features(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_refresh_active_features"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_lazy_deps_ensure_and_bind(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_lazy_deps_ensure_and_bind(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_lazy_deps_ensure_and_bind"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_get_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_get_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_get_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_get_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_get_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_get_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_filter_and_summarize(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_filter_and_summarize(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_filter_and_summarize"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_async_list_entities(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_async_list_entities(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_async_list_entities"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_async_get_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_async_get_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_async_get_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_build_service_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_build_service_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_build_service_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_parse_service_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_parse_service_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_parse_service_response"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_async_call_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_async_call_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_async_call_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_handle_list_entities(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_handle_list_entities(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_handle_list_entities"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_handle_get_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_handle_get_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_handle_get_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_handle_call_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_handle_call_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_handle_call_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_async_list_services(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_async_list_services(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_async_list_services"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_handle_list_services(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_handle_list_services(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_handle_list_services"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_homeassistant_tool_u_check_ha_available(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_homeassistant_tool_u_check_ha_available(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_homeassistant_tool_u_check_ha_available"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_is_registry_register_call(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_is_registry_register_call(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_is_registry_register_call"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_module_registers_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_module_registers_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_module_registers_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_discover_builtin_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_discover_builtin_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_discover_builtin_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_check_fn_cached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_check_fn_cached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_check_fn_cached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_invalidate_check_fn_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_invalidate_check_fn_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_invalidate_check_fn_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_snapshot_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_snapshot_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_snapshot_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_snapshot_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_snapshot_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_snapshot_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_get_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_get_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_get_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_register_plugin_override_policy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_register_plugin_override_policy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_register_plugin_override_policy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_plugin_owner_of(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_plugin_owner_of(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_plugin_owner_of"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_caller_module(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_caller_module(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_caller_module"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_get_definitions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_get_definitions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_get_definitions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_u_normalize_handler_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_u_normalize_handler_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_u_normalize_handler_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_registry_check_tool_availability(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_registry_check_tool_availability(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_registry_check_tool_availability"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_load_x_search_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_load_x_search_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_load_x_search_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_get_x_search_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_get_x_search_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_get_x_search_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_get_x_search_reasoning_effort(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_get_x_search_reasoning_effort(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_get_x_search_reasoning_effort"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_get_x_search_timeout_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_get_x_search_timeout_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_get_x_search_timeout_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_get_x_search_retries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_get_x_search_retries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_get_x_search_retries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_resolve_xai_bearer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_resolve_xai_bearer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_resolve_xai_bearer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_check_x_search_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_check_x_search_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_check_x_search_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_normalize_handles(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_normalize_handles(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_normalize_handles"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_parse_iso_date(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_parse_iso_date(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_parse_iso_date"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_validate_date_range(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_validate_date_range(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_validate_date_range"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_extract_inline_citations(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_extract_inline_citations(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_extract_inline_citations"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_http_error_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_http_error_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_http_error_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_x_search_tool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_x_search_tool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_x_search_tool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_x_search_tool_u_handle_x_search(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_x_search_tool_u_handle_x_search(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_x_search_tool_u_handle_x_search"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_blocked_toolsets_for_role(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_blocked_toolsets_for_role(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_blocked_toolsets_for_role"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_emit_parent_console(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_emit_parent_console(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_emit_parent_console"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_build_child_progress_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_build_child_progress_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_build_child_progress_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_inherit_parent_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_inherit_parent_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_inherit_parent_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_dump_subagent_timeout_diagnostic(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_dump_subagent_timeout_diagnostic(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_dump_subagent_timeout_diagnostic"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_spill_summary_to_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_spill_summary_to_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_spill_summary_to_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_parent_summary_char_budget(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_parent_summary_char_budget(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_parent_summary_char_budget"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_apply_summary_budget(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_apply_summary_budget(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_apply_summary_budget"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_run_single_child(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_run_single_child(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_run_single_child"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_resolve_child_credential_pool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_resolve_child_credential_pool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_resolve_child_credential_pool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_resolve_delegation_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_resolve_delegation_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_resolve_delegation_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_build_dynamic_schema_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_build_dynamic_schema_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_build_dynamic_schema_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegate_tool_u_strip_model_hidden_task_fields(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegate_tool_u_strip_model_hidden_task_fields(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegate_tool_u_strip_model_hidden_task_fields"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_live_transcript_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_live_transcript_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_live_transcript_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_new_live_delegation_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_new_live_delegation_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_new_live_delegation_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_u_one_line(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_u_one_line(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_u_one_line"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_assistant_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_assistant_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_assistant_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_tool_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_tool_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_tool_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_tool_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_tool_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_tool_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_add_stream_delta(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_add_stream_delta(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_add_stream_delta"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_observe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_observe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_observe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_wrap_progress_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_wrap_progress_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_wrap_progress_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_create_live_transcripts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_create_live_transcripts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_create_live_transcripts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_u_manifest_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_u_manifest_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_u_manifest_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_update_manifest_statuses(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_update_manifest_statuses(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_update_manifest_statuses"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_delegation_live_log_prune_stale_live_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_delegation_live_log_prune_stale_live_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_delegation_live_log_prune_stale_live_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_direct_snapshot_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_direct_snapshot_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_direct_snapshot_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_get_snapshot_restore_candidate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_get_snapshot_restore_candidate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_get_snapshot_restore_candidate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_store_direct_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_store_direct_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_store_direct_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_delete_direct_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_delete_direct_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_delete_direct_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_ensure_modal_sdk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_ensure_modal_sdk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_ensure_modal_sdk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_resolve_modal_image(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_resolve_modal_image(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_resolve_modal_image"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_run_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_run_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_run_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_run_coroutine(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_run_coroutine(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_run_coroutine"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_modal_upload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_modal_upload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_modal_upload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_modal_bulk_upload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_modal_bulk_upload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_modal_bulk_upload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_modal_bulk_download(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_modal_bulk_download(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_modal_bulk_download"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_modal_u_modal_delete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_modal_u_modal_delete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_modal_u_modal_delete"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_u_lock_for(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_u_lock_for(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_u_lock_for"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_record_read(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_record_read(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_record_read"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_note_write(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_note_write(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_note_write"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_check_stale(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_check_stale(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_check_stale"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_writes_since(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_writes_since(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_writes_since"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_known_reads(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_known_reads(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_known_reads"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_record_read_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_record_read_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_record_read_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_note_write_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_note_write_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_note_write_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_check_stale_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_check_stale_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_check_stale_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_writes_since_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_writes_since_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_writes_since_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_file_state_known_reads_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_file_state_known_reads_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_file_state_known_reads_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_publish_authorization_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_publish_authorization_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_publish_authorization_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_wait_for_authorization_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_wait_for_authorization_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_wait_for_authorization_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_deliver_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_deliver_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_deliver_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_wait_for_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_wait_for_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_wait_for_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_mark_approved(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_mark_approved(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_mark_approved"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_mark_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_mark_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_mark_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_mark_worker_done(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_mark_worker_done(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_mark_worker_done"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_worker_done(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_worker_done(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_worker_done"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_dashboard_oauth_flow(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_dashboard_oauth_flow(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_dashboard_oauth_flow"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_dashboard_oauth_get_dashboard_oauth_flow"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_clear_expired(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_clear_expired(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_clear_expired"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_u__aenter__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_u__aenter__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_u__aenter__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_u__aexit__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_u__aexit__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_u__aexit__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_search_duckduckgo(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_search_duckduckgo(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_search_duckduckgo"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_search_brave(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_search_brave(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_search_brave"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_search_google_cse(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_search_google_cse(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_search_google_cse"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_get_researcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_get_researcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_get_researcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_close_researcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_close_researcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_close_researcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_research_model_benchmarks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_research_model_benchmarks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_research_model_benchmarks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_online_research_research_general(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_online_research_research_general(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_online_research_research_general"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_resolve_image_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_resolve_image_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_resolve_image_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_resolve_data_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_resolve_data_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_resolve_data_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_http_block_reason(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_http_block_reason(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_http_block_reason"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_download_to_bytes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_download_to_bytes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_download_to_bytes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_is_local_terminal_backend(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_is_local_terminal_backend(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_is_local_terminal_backend"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_media_cache_roots(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_media_cache_roots(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_media_cache_roots"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_permitted_host_read_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_permitted_host_read_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_permitted_host_read_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_source_u_resolve_container_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_source_u_resolve_container_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_source_u_resolve_container_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_is_delegated_child_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_is_delegated_child_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_is_delegated_child_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_reject_delegated_child_mutation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_reject_delegated_child_mutation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_reject_delegated_child_mutation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_connect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_connect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_connect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_heartbeat_current_worker_from_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_heartbeat_current_worker_from_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_heartbeat_current_worker_from_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_handle_attach(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_handle_attach(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_handle_attach"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_download_url_with_cap(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_download_url_with_cap(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_download_url_with_cap"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_handle_attach_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_handle_attach_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_handle_attach_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_kanban_tools_u_handle_attachments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_kanban_tools_u_handle_attachments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_kanban_tools_u_handle_attachments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_managed_nous_tools_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_managed_nous_tools_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_managed_nous_tools_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_normalize_browser_cloud_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_normalize_browser_cloud_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_normalize_browser_cloud_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_coerce_modal_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_coerce_modal_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_coerce_modal_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_normalize_modal_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_normalize_modal_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_normalize_modal_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_has_direct_modal_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_has_direct_modal_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_has_direct_modal_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_resolve_modal_backend_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_resolve_modal_backend_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_resolve_modal_backend_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_resolve_openai_audio_api_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_resolve_openai_audio_api_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_resolve_openai_audio_api_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_backend_helpers_prefers_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_backend_helpers_prefers_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_backend_helpers_prefers_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_u_referenced_support_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_u_referenced_support_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_u_referenced_support_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_source_url_for_bundle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_source_url_for_bundle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_source_url_for_bundle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_u_ssrf_safe_http_get(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_u_ssrf_safe_http_get(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_u_ssrf_safe_http_get"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_u_fetch_file_bytes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_u_fetch_file_bytes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_u_fetch_file_bytes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_u_fetch_bytes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_u_fetch_bytes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_u_fetch_bytes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_u_find_skill_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_u_find_skill_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_u_find_skill_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_hub_u_parse_frontmatter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_hub_u_parse_frontmatter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_hub_u_parse_frontmatter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_configured_for_xai_video(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_configured_for_xai_video(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_configured_for_xai_video"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_check_xai_video_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_check_xai_video_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_check_xai_video_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_clean_string(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_clean_string(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_clean_string"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_provider_not_configured_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_provider_not_configured_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_provider_not_configured_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_normalize_public_video_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_normalize_public_video_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_normalize_public_video_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_handle_xai_video_edit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_handle_xai_video_edit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_handle_xai_video_edit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_xai_video_tools_u_handle_xai_video_extend(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_xai_video_tools_u_handle_xai_video_extend(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_xai_video_tools_u_handle_xai_video_extend"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_permissions_u_resolve_driver_cmd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_permissions_u_resolve_driver_cmd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_permissions_u_resolve_driver_cmd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_permissions_u_child_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_permissions_u_child_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_permissions_u_child_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_permissions_u_json_out(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_permissions_u_json_out(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_permissions_u_json_out"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_permissions_u_mac_permissions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_permissions_u_mac_permissions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_permissions_u_mac_permissions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_permissions_computer_use_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_permissions_computer_use_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_permissions_computer_use_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_permissions_request_permissions_grant(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_permissions_request_permissions_grant(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_permissions_request_permissions_grant"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_project_tools_set_project_workspace_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_project_tools_set_project_workspace_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_project_tools_set_project_workspace_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_project_tools_u_primary_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_project_tools_u_primary_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_project_tools_u_primary_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_project_tools_u_apply_workspace(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_project_tools_u_apply_workspace(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_project_tools_u_apply_workspace"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_project_tools_project_list(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_project_tools_project_list(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_project_tools_project_list"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_project_tools_project_create(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_project_tools_project_create(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_project_tools_project_create"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_project_tools_project_switch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_project_tools_project_switch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_project_tools_project_switch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_credential_files_register_credential_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_credential_files_register_credential_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_credential_files_register_credential_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_credential_files_register_credential_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_credential_files_register_credential_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_credential_files_register_credential_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_credential_files_iter_skills_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_credential_files_iter_skills_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_credential_files_iter_skills_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_credential_files_from_agent_visible_cache_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_credential_files_from_agent_visible_cache_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_credential_files_from_agent_visible_cache_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_credential_files_iter_cache_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_credential_files_iter_cache_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_credential_files_iter_cache_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_hook_output_spill_u_coerce_non_negative_int(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_hook_output_spill_u_coerce_non_negative_int(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_hook_output_spill_u_coerce_non_negative_int"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_hook_output_spill_get_spill_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_hook_output_spill_get_spill_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_hook_output_spill_get_spill_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_hook_output_spill_u_resolve_spill_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_hook_output_spill_u_resolve_spill_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_hook_output_spill_u_resolve_spill_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_hook_output_spill_u_build_preview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_hook_output_spill_u_build_preview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_hook_output_spill_u_build_preview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_hook_output_spill_spill_if_oversized(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_hook_output_spill_spill_if_oversized(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_hook_output_spill_spill_if_oversized"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_session_search_tool_u_is_compaction_summary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_session_search_tool_u_is_compaction_summary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_session_search_tool_u_is_compaction_summary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_session_search_tool_u_resolve_lineage(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_session_search_tool_u_resolve_lineage(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_session_search_tool_u_resolve_lineage"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_session_search_tool_u_is_compression_ended(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_session_search_tool_u_is_compression_ended(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_session_search_tool_u_is_compression_ended"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_session_search_tool_u_is_compacted_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_session_search_tool_u_is_compacted_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_session_search_tool_u_is_compacted_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_session_search_tool_u_annotate_rebuild_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_session_search_tool_u_annotate_rebuild_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_session_search_tool_u_annotate_rebuild_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_browser_tool_u_is_headed_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_browser_tool_u_is_headed_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_browser_tool_u_is_headed_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_browser_tool_u_store_full_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_browser_tool_u_store_full_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_browser_tool_u_store_full_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_browser_tool_u_restrict_browser_evaluate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_browser_tool_u_restrict_browser_evaluate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_browser_tool_u_restrict_browser_evaluate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_browser_tool_u_camofox_current_page_private_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_browser_tool_u_camofox_current_page_private_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_browser_tool_u_camofox_current_page_private_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_checkpoint_manager_u_volume_evidence(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_checkpoint_manager_u_volume_evidence(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_checkpoint_manager_u_volume_evidence"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_checkpoint_manager_u_pre_v2_shadow_repos(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_checkpoint_manager_u_pre_v2_shadow_repos(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_checkpoint_manager_u_pre_v2_shadow_repos"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_checkpoint_manager_u_workdir_is_observably_gone(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_checkpoint_manager_u_workdir_is_observably_gone(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_checkpoint_manager_u_workdir_is_observably_gone"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_checkpoint_manager_u_dir_has_any_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_checkpoint_manager_u_dir_has_any_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_checkpoint_manager_u_dir_has_any_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_doctor_u_cua_child_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_doctor_u_cua_child_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_doctor_u_cua_child_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_doctor_u_sanitized_cua_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_doctor_u_sanitized_cua_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_doctor_u_sanitized_cua_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_doctor_u_drive_health_report(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_doctor_u_drive_health_report(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_doctor_u_drive_health_report"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_doctor_u_print_text_report(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_doctor_u_print_text_report(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_doctor_u_print_text_report"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skill_provenance_set_current_write_origin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skill_provenance_set_current_write_origin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skill_provenance_set_current_write_origin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skill_provenance_reset_current_write_origin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skill_provenance_reset_current_write_origin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skill_provenance_reset_current_write_origin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skill_provenance_get_current_write_origin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skill_provenance_get_current_write_origin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skill_provenance_get_current_write_origin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skill_provenance_is_background_review(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skill_provenance_is_background_review(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skill_provenance_is_background_review"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_web_tools_u_web_extract_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_web_tools_u_web_extract_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_web_tools_u_web_extract_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_web_tools_u_registered_web_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_web_tools_u_registered_web_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_web_tools_u_registered_web_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_web_tools_u_registered_web_provider_available(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_web_tools_u_registered_web_provider_available(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_web_tools_u_registered_web_provider_available"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_web_tools_u_list_registered_web_providers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_web_tools_u_list_registered_web_providers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_web_tools_u_list_registered_web_providers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_env_probe_u_probe_worker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_env_probe_u_probe_worker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_env_probe_u_probe_worker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_env_probe_u_ensure_probe_started(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_env_probe_u_ensure_probe_started(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_env_probe_u_ensure_probe_started"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_env_probe_warm_environment_probe_async(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_env_probe_warm_environment_probe_async(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_env_probe_warm_environment_probe_async"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_stdio_watchdog_u_is_orphaned(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_stdio_watchdog_u_is_orphaned(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_stdio_watchdog_u_is_orphaned"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_stdio_watchdog_u_terminate_process_group(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_stdio_watchdog_u_terminate_process_group(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_stdio_watchdog_u_terminate_process_group"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mcp_stdio_watchdog_u_watchdog_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mcp_stdio_watchdog_u_watchdog_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mcp_stdio_watchdog_u_watchdog_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_open_preview_tool_u_normalize_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_open_preview_tool_u_normalize_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_open_preview_tool_u_normalize_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_open_preview_tool_open_preview_tool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_open_preview_tool_open_preview_tool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_open_preview_tool_open_preview_tool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_open_preview_tool_check_open_preview_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_open_preview_tool_check_open_preview_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_open_preview_tool_check_open_preview_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tts_streaming_mark_speech_interrupted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tts_streaming_mark_speech_interrupted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tts_streaming_mark_speech_interrupted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tts_streaming_take_speech_interrupted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tts_streaming_take_speech_interrupted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tts_streaming_take_speech_interrupted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tts_streaming_resolve_streaming_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tts_streaming_resolve_streaming_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tts_streaming_resolve_streaming_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_backend_list_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_backend_list_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_backend_list_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_computer_use_backend_set_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_computer_use_backend_set_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_computer_use_backend_set_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_focus_pane_tool_focus_pane_tool(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_focus_pane_tool_focus_pane_tool(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_focus_pane_tool_focus_pane_tool"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_focus_pane_tool_check_focus_pane_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_focus_pane_tool_check_focus_pane_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_focus_pane_tool_check_focus_pane_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_generation_tool_check_image_generation_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_generation_tool_check_image_generation_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_generation_tool_check_image_generation_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_image_generation_tool_u_dispatch_to_plugin_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_image_generation_tool_u_dispatch_to_plugin_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_image_generation_tool_u_dispatch_to_plugin_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_microsoft_graph_client_post_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_microsoft_graph_client_post_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_microsoft_graph_client_post_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_microsoft_graph_client_u_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_microsoft_graph_client_u_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_microsoft_graph_client_u_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_send_message_tool_u_media_caption_split(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_send_message_tool_u_media_caption_split(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_send_message_tool_u_media_caption_split"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_send_message_tool_u_resolve_slack_user_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_send_message_tool_u_resolve_slack_user_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_send_message_tool_u_resolve_slack_user_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_thread_context_u_callback_api(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_thread_context_u_callback_api(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_thread_context_u_callback_api"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_thread_context_propagate_context_to_thread(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_thread_context_propagate_context_to_thread(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_thread_context_propagate_context_to_thread"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_video_generation_tool_u_resolve_active_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_video_generation_tool_u_resolve_active_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_video_generation_tool_u_resolve_active_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_video_generation_tool_u_handle_video_generate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_video_generation_tool_u_handle_video_generate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_video_generation_tool_u_handle_video_generate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_voice_mode_cancel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_voice_mode_cancel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_voice_mode_cancel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_voice_mode_cancel_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_voice_mode_cancel_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_voice_mode_cancel_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_ansi_strip_sanitize_display_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_ansi_strip_sanitize_display_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_ansi_strip_sanitize_display_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_binary_extensions_has_binary_extension(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_binary_extensions_has_binary_extension(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_binary_extensions_has_binary_extension"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_browser_camofox_state_get_camofox_state_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_browser_camofox_state_get_camofox_state_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_browser_camofox_state_get_camofox_state_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_budget_config_resolve_threshold(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_budget_config_resolve_threshold(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_budget_config_resolve_threshold"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_clarify_gateway_resolve_clarify_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_clarify_gateway_resolve_clarify_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_clarify_gateway_resolve_clarify_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_daemon_pool_u_adjust_thread_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_daemon_pool_u_adjust_thread_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_daemon_pool_u_adjust_thread_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_debug_helpers_log_call(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_debug_helpers_log_call(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_debug_helpers_log_call"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_desktop_ui_set_emitter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_desktop_ui_set_emitter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_desktop_ui_set_emitter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_file_sync_u_resolve_host_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_file_sync_u_resolve_host_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_file_sync_u_resolve_host_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_environments_managed_mod_u_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_environments_managed_mod_u_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_environments_managed_mod_u_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_feishu_drive_tool_u_do_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_feishu_drive_tool_u_do_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_feishu_drive_tool_u_do_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_interrupt_clear_current_thread_interrupt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_interrupt_clear_current_thread_interrupt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_interrupt_clear_current_thread_interrupt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_mixture_of_agents_tool_u_build_auth_header(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_mixture_of_agents_tool_u_build_auth_header(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_mixture_of_agents_tool_u_build_auth_header"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_moa_performance_u_build_auth_header(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_moa_performance_u_build_auth_header(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_moa_performance_u_build_auth_header"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_skills_guard_scan_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_skills_guard_scan_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_skills_guard_scan_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tirith_security_u_record_tirith_crash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tirith_security_u_record_tirith_crash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tirith_security_u_record_tirith_crash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_tool_search_u_safe_float(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_tool_search_u_safe_float(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_tool_search_u_safe_float"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_tools_website_policy_check_website_access(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)tools_website_policy_check_website_access(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("tools_website_policy_check_website_access"));
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
        if (strcmp(op, "tools_computer_use_tool_u_canon_key_combo") == 0) o = emit_tools_computer_use_tool_u_canon_key_combo(c);
        if (strcmp(op, "tools_computer_use_tool_reset_backend_for_tests") == 0) o = emit_tools_computer_use_tool_reset_backend_for_tests(c);
        if (strcmp(op, "tools_computer_use_tool_type_text") == 0) o = emit_tools_computer_use_tool_type_text(c);
        if (strcmp(op, "tools_computer_use_tool_list_apps") == 0) o = emit_tools_computer_use_tool_list_apps(c);
        if (strcmp(op, "tools_computer_use_tool_list_windows") == 0) o = emit_tools_computer_use_tool_list_windows(c);
        if (strcmp(op, "tools_computer_use_tool_set_value") == 0) o = emit_tools_computer_use_tool_set_value(c);
        if (strcmp(op, "tools_computer_use_tool_u_request_approval") == 0) o = emit_tools_computer_use_tool_u_request_approval(c);
        if (strcmp(op, "tools_computer_use_tool_u_summarize_action") == 0) o = emit_tools_computer_use_tool_u_summarize_action(c);
        if (strcmp(op, "tools_computer_use_tool_u_image_dimensions_from_b64") == 0) o = emit_tools_computer_use_tool_u_image_dimensions_from_b64(c);
        if (strcmp(op, "tools_computer_use_tool_u_coerce_max_elements") == 0) o = emit_tools_computer_use_tool_u_coerce_max_elements(c);
        if (strcmp(op, "tools_computer_use_tool_u_shrink_capture_for_vision") == 0) o = emit_tools_computer_use_tool_u_shrink_capture_for_vision(c);
        if (strcmp(op, "tools_computer_use_tool_u_should_route_through_aux_vision") == 0) o = emit_tools_computer_use_tool_u_should_route_through_aux_vision(c);
        if (strcmp(op, "tools_computer_use_tool_u_capture_after_mode") == 0) o = emit_tools_computer_use_tool_u_capture_after_mode(c);
        if (strcmp(op, "tools_computer_use_tool_u_route_capture_through_aux_vision") == 0) o = emit_tools_computer_use_tool_u_route_capture_through_aux_vision(c);
        if (strcmp(op, "tools_computer_use_tool_u_maybe_follow_capture") == 0) o = emit_tools_computer_use_tool_u_maybe_follow_capture(c);
        if (strcmp(op, "tools_computer_use_tool_u_format_elements") == 0) o = emit_tools_computer_use_tool_u_format_elements(c);
        if (strcmp(op, "tools_computer_use_tool_u_element_to_dict") == 0) o = emit_tools_computer_use_tool_u_element_to_dict(c);
        if (strcmp(op, "tools_computer_use_tool_check_computer_use_requirements") == 0) o = emit_tools_computer_use_tool_check_computer_use_requirements(c);
        if (strcmp(op, "tools_lazy_deps_u_format") == 0) o = emit_tools_lazy_deps_u_format(c);
        if (strcmp(op, "tools_lazy_deps_u_python_abi_tag") == 0) o = emit_tools_lazy_deps_u_python_abi_tag(c);
        if (strcmp(op, "tools_lazy_deps_u_lazy_install_target") == 0) o = emit_tools_lazy_deps_u_lazy_install_target(c);
        if (strcmp(op, "tools_lazy_deps_u_ensure_target_ready") == 0) o = emit_tools_lazy_deps_u_ensure_target_ready(c);
        if (strcmp(op, "tools_lazy_deps_u_activate_target_on_syspath") == 0) o = emit_tools_lazy_deps_u_activate_target_on_syspath(c);
        if (strcmp(op, "tools_lazy_deps_activate_durable_lazy_target") == 0) o = emit_tools_lazy_deps_activate_durable_lazy_target(c);
        if (strcmp(op, "tools_lazy_deps_u_allow_lazy_installs") == 0) o = emit_tools_lazy_deps_u_allow_lazy_installs(c);
        if (strcmp(op, "tools_lazy_deps_u_unsupported_feature_reason") == 0) o = emit_tools_lazy_deps_u_unsupported_feature_reason(c);
        if (strcmp(op, "tools_lazy_deps_u_is_satisfied") == 0) o = emit_tools_lazy_deps_u_is_satisfied(c);
        if (strcmp(op, "tools_lazy_deps_u_is_present") == 0) o = emit_tools_lazy_deps_u_is_present(c);
        if (strcmp(op, "tools_lazy_deps_u_core_constraints_file") == 0) o = emit_tools_lazy_deps_u_core_constraints_file(c);
        if (strcmp(op, "tools_lazy_deps_u_venv_pip_install") == 0) o = emit_tools_lazy_deps_u_venv_pip_install(c);
        if (strcmp(op, "tools_lazy_deps_active_features") == 0) o = emit_tools_lazy_deps_active_features(c);
        if (strcmp(op, "tools_lazy_deps_refresh_active_features") == 0) o = emit_tools_lazy_deps_refresh_active_features(c);
        if (strcmp(op, "tools_lazy_deps_ensure_and_bind") == 0) o = emit_tools_lazy_deps_ensure_and_bind(c);
        if (strcmp(op, "tools_homeassistant_tool_u_get_config") == 0) o = emit_tools_homeassistant_tool_u_get_config(c);
        if (strcmp(op, "tools_homeassistant_tool_u_get_headers") == 0) o = emit_tools_homeassistant_tool_u_get_headers(c);
        if (strcmp(op, "tools_homeassistant_tool_u_filter_and_summarize") == 0) o = emit_tools_homeassistant_tool_u_filter_and_summarize(c);
        if (strcmp(op, "tools_homeassistant_tool_u_async_list_entities") == 0) o = emit_tools_homeassistant_tool_u_async_list_entities(c);
        if (strcmp(op, "tools_homeassistant_tool_u_async_get_state") == 0) o = emit_tools_homeassistant_tool_u_async_get_state(c);
        if (strcmp(op, "tools_homeassistant_tool_u_build_service_payload") == 0) o = emit_tools_homeassistant_tool_u_build_service_payload(c);
        if (strcmp(op, "tools_homeassistant_tool_u_parse_service_response") == 0) o = emit_tools_homeassistant_tool_u_parse_service_response(c);
        if (strcmp(op, "tools_homeassistant_tool_u_async_call_service") == 0) o = emit_tools_homeassistant_tool_u_async_call_service(c);
        if (strcmp(op, "tools_homeassistant_tool_u_handle_list_entities") == 0) o = emit_tools_homeassistant_tool_u_handle_list_entities(c);
        if (strcmp(op, "tools_homeassistant_tool_u_handle_get_state") == 0) o = emit_tools_homeassistant_tool_u_handle_get_state(c);
        if (strcmp(op, "tools_homeassistant_tool_u_handle_call_service") == 0) o = emit_tools_homeassistant_tool_u_handle_call_service(c);
        if (strcmp(op, "tools_homeassistant_tool_u_async_list_services") == 0) o = emit_tools_homeassistant_tool_u_async_list_services(c);
        if (strcmp(op, "tools_homeassistant_tool_u_handle_list_services") == 0) o = emit_tools_homeassistant_tool_u_handle_list_services(c);
        if (strcmp(op, "tools_homeassistant_tool_u_check_ha_available") == 0) o = emit_tools_homeassistant_tool_u_check_ha_available(c);
        if (strcmp(op, "tools_registry_u_is_registry_register_call") == 0) o = emit_tools_registry_u_is_registry_register_call(c);
        if (strcmp(op, "tools_registry_u_module_registers_tools") == 0) o = emit_tools_registry_u_module_registers_tools(c);
        if (strcmp(op, "tools_registry_discover_builtin_tools") == 0) o = emit_tools_registry_discover_builtin_tools(c);
        if (strcmp(op, "tools_registry_u_check_fn_cached") == 0) o = emit_tools_registry_u_check_fn_cached(c);
        if (strcmp(op, "tools_registry_invalidate_check_fn_cache") == 0) o = emit_tools_registry_invalidate_check_fn_cache(c);
        if (strcmp(op, "tools_registry_u_snapshot_state") == 0) o = emit_tools_registry_u_snapshot_state(c);
        if (strcmp(op, "tools_registry_u_snapshot_entries") == 0) o = emit_tools_registry_u_snapshot_entries(c);
        if (strcmp(op, "tools_registry_get_entry") == 0) o = emit_tools_registry_get_entry(c);
        if (strcmp(op, "tools_registry_register_plugin_override_policy") == 0) o = emit_tools_registry_register_plugin_override_policy(c);
        if (strcmp(op, "tools_registry_u_plugin_owner_of") == 0) o = emit_tools_registry_u_plugin_owner_of(c);
        if (strcmp(op, "tools_registry_u_caller_module") == 0) o = emit_tools_registry_u_caller_module(c);
        if (strcmp(op, "tools_registry_get_definitions") == 0) o = emit_tools_registry_get_definitions(c);
        if (strcmp(op, "tools_registry_u_normalize_handler_result") == 0) o = emit_tools_registry_u_normalize_handler_result(c);
        if (strcmp(op, "tools_registry_check_tool_availability") == 0) o = emit_tools_registry_check_tool_availability(c);
        if (strcmp(op, "tools_x_search_tool_u_load_x_search_config") == 0) o = emit_tools_x_search_tool_u_load_x_search_config(c);
        if (strcmp(op, "tools_x_search_tool_u_get_x_search_model") == 0) o = emit_tools_x_search_tool_u_get_x_search_model(c);
        if (strcmp(op, "tools_x_search_tool_u_get_x_search_reasoning_effort") == 0) o = emit_tools_x_search_tool_u_get_x_search_reasoning_effort(c);
        if (strcmp(op, "tools_x_search_tool_u_get_x_search_timeout_seconds") == 0) o = emit_tools_x_search_tool_u_get_x_search_timeout_seconds(c);
        if (strcmp(op, "tools_x_search_tool_u_get_x_search_retries") == 0) o = emit_tools_x_search_tool_u_get_x_search_retries(c);
        if (strcmp(op, "tools_x_search_tool_u_resolve_xai_bearer") == 0) o = emit_tools_x_search_tool_u_resolve_xai_bearer(c);
        if (strcmp(op, "tools_x_search_tool_check_x_search_requirements") == 0) o = emit_tools_x_search_tool_check_x_search_requirements(c);
        if (strcmp(op, "tools_x_search_tool_u_normalize_handles") == 0) o = emit_tools_x_search_tool_u_normalize_handles(c);
        if (strcmp(op, "tools_x_search_tool_u_parse_iso_date") == 0) o = emit_tools_x_search_tool_u_parse_iso_date(c);
        if (strcmp(op, "tools_x_search_tool_u_validate_date_range") == 0) o = emit_tools_x_search_tool_u_validate_date_range(c);
        if (strcmp(op, "tools_x_search_tool_u_extract_inline_citations") == 0) o = emit_tools_x_search_tool_u_extract_inline_citations(c);
        if (strcmp(op, "tools_x_search_tool_u_http_error_message") == 0) o = emit_tools_x_search_tool_u_http_error_message(c);
        if (strcmp(op, "tools_x_search_tool_x_search_tool") == 0) o = emit_tools_x_search_tool_x_search_tool(c);
        if (strcmp(op, "tools_x_search_tool_u_handle_x_search") == 0) o = emit_tools_x_search_tool_u_handle_x_search(c);
        if (strcmp(op, "tools_delegate_tool_u_blocked_toolsets_for_role") == 0) o = emit_tools_delegate_tool_u_blocked_toolsets_for_role(c);
        if (strcmp(op, "tools_delegate_tool_u_emit_parent_console") == 0) o = emit_tools_delegate_tool_u_emit_parent_console(c);
        if (strcmp(op, "tools_delegate_tool_u_build_child_progress_callback") == 0) o = emit_tools_delegate_tool_u_build_child_progress_callback(c);
        if (strcmp(op, "tools_delegate_tool_u_inherit_parent_base_url") == 0) o = emit_tools_delegate_tool_u_inherit_parent_base_url(c);
        if (strcmp(op, "tools_delegate_tool_u_dump_subagent_timeout_diagnostic") == 0) o = emit_tools_delegate_tool_u_dump_subagent_timeout_diagnostic(c);
        if (strcmp(op, "tools_delegate_tool_u_spill_summary_to_file") == 0) o = emit_tools_delegate_tool_u_spill_summary_to_file(c);
        if (strcmp(op, "tools_delegate_tool_u_parent_summary_char_budget") == 0) o = emit_tools_delegate_tool_u_parent_summary_char_budget(c);
        if (strcmp(op, "tools_delegate_tool_u_apply_summary_budget") == 0) o = emit_tools_delegate_tool_u_apply_summary_budget(c);
        if (strcmp(op, "tools_delegate_tool_u_run_single_child") == 0) o = emit_tools_delegate_tool_u_run_single_child(c);
        if (strcmp(op, "tools_delegate_tool_u_resolve_child_credential_pool") == 0) o = emit_tools_delegate_tool_u_resolve_child_credential_pool(c);
        if (strcmp(op, "tools_delegate_tool_u_resolve_delegation_credentials") == 0) o = emit_tools_delegate_tool_u_resolve_delegation_credentials(c);
        if (strcmp(op, "tools_delegate_tool_u_build_dynamic_schema_overrides") == 0) o = emit_tools_delegate_tool_u_build_dynamic_schema_overrides(c);
        if (strcmp(op, "tools_delegate_tool_u_strip_model_hidden_task_fields") == 0) o = emit_tools_delegate_tool_u_strip_model_hidden_task_fields(c);
        if (strcmp(op, "tools_delegation_live_log_live_transcript_root") == 0) o = emit_tools_delegation_live_log_live_transcript_root(c);
        if (strcmp(op, "tools_delegation_live_log_new_live_delegation_id") == 0) o = emit_tools_delegation_live_log_new_live_delegation_id(c);
        if (strcmp(op, "tools_delegation_live_log_u_one_line") == 0) o = emit_tools_delegation_live_log_u_one_line(c);
        if (strcmp(op, "tools_delegation_live_log_assistant_text") == 0) o = emit_tools_delegation_live_log_assistant_text(c);
        if (strcmp(op, "tools_delegation_live_log_tool_start") == 0) o = emit_tools_delegation_live_log_tool_start(c);
        if (strcmp(op, "tools_delegation_live_log_tool_result") == 0) o = emit_tools_delegation_live_log_tool_result(c);
        if (strcmp(op, "tools_delegation_live_log_add_stream_delta") == 0) o = emit_tools_delegation_live_log_add_stream_delta(c);
        if (strcmp(op, "tools_delegation_live_log_observe") == 0) o = emit_tools_delegation_live_log_observe(c);
        if (strcmp(op, "tools_delegation_live_log_wrap_progress_callback") == 0) o = emit_tools_delegation_live_log_wrap_progress_callback(c);
        if (strcmp(op, "tools_delegation_live_log_create_live_transcripts") == 0) o = emit_tools_delegation_live_log_create_live_transcripts(c);
        if (strcmp(op, "tools_delegation_live_log_u_manifest_path") == 0) o = emit_tools_delegation_live_log_u_manifest_path(c);
        if (strcmp(op, "tools_delegation_live_log_update_manifest_statuses") == 0) o = emit_tools_delegation_live_log_update_manifest_statuses(c);
        if (strcmp(op, "tools_delegation_live_log_prune_stale_live_dirs") == 0) o = emit_tools_delegation_live_log_prune_stale_live_dirs(c);
        if (strcmp(op, "tools_environments_modal_u_direct_snapshot_key") == 0) o = emit_tools_environments_modal_u_direct_snapshot_key(c);
        if (strcmp(op, "tools_environments_modal_u_get_snapshot_restore_candidate") == 0) o = emit_tools_environments_modal_u_get_snapshot_restore_candidate(c);
        if (strcmp(op, "tools_environments_modal_u_store_direct_snapshot") == 0) o = emit_tools_environments_modal_u_store_direct_snapshot(c);
        if (strcmp(op, "tools_environments_modal_u_delete_direct_snapshot") == 0) o = emit_tools_environments_modal_u_delete_direct_snapshot(c);
        if (strcmp(op, "tools_environments_modal_u_ensure_modal_sdk") == 0) o = emit_tools_environments_modal_u_ensure_modal_sdk(c);
        if (strcmp(op, "tools_environments_modal_u_resolve_modal_image") == 0) o = emit_tools_environments_modal_u_resolve_modal_image(c);
        if (strcmp(op, "tools_environments_modal_u_run_loop") == 0) o = emit_tools_environments_modal_u_run_loop(c);
        if (strcmp(op, "tools_environments_modal_run_coroutine") == 0) o = emit_tools_environments_modal_run_coroutine(c);
        if (strcmp(op, "tools_environments_modal_u_modal_upload") == 0) o = emit_tools_environments_modal_u_modal_upload(c);
        if (strcmp(op, "tools_environments_modal_u_modal_bulk_upload") == 0) o = emit_tools_environments_modal_u_modal_bulk_upload(c);
        if (strcmp(op, "tools_environments_modal_u_modal_bulk_download") == 0) o = emit_tools_environments_modal_u_modal_bulk_download(c);
        if (strcmp(op, "tools_environments_modal_u_modal_delete") == 0) o = emit_tools_environments_modal_u_modal_delete(c);
        if (strcmp(op, "tools_file_state_u_lock_for") == 0) o = emit_tools_file_state_u_lock_for(c);
        if (strcmp(op, "tools_file_state_record_read") == 0) o = emit_tools_file_state_record_read(c);
        if (strcmp(op, "tools_file_state_note_write") == 0) o = emit_tools_file_state_note_write(c);
        if (strcmp(op, "tools_file_state_check_stale") == 0) o = emit_tools_file_state_check_stale(c);
        if (strcmp(op, "tools_file_state_writes_since") == 0) o = emit_tools_file_state_writes_since(c);
        if (strcmp(op, "tools_file_state_known_reads") == 0) o = emit_tools_file_state_known_reads(c);
        if (strcmp(op, "tools_file_state_record_read_2") == 0) o = emit_tools_file_state_record_read_2(c);
        if (strcmp(op, "tools_file_state_note_write_2") == 0) o = emit_tools_file_state_note_write_2(c);
        if (strcmp(op, "tools_file_state_check_stale_2") == 0) o = emit_tools_file_state_check_stale_2(c);
        if (strcmp(op, "tools_file_state_writes_since_2") == 0) o = emit_tools_file_state_writes_since_2(c);
        if (strcmp(op, "tools_file_state_known_reads_2") == 0) o = emit_tools_file_state_known_reads_2(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_publish_authorization_url") == 0) o = emit_tools_mcp_dashboard_oauth_publish_authorization_url(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_wait_for_authorization_url") == 0) o = emit_tools_mcp_dashboard_oauth_wait_for_authorization_url(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_deliver_callback") == 0) o = emit_tools_mcp_dashboard_oauth_deliver_callback(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_wait_for_callback") == 0) o = emit_tools_mcp_dashboard_oauth_wait_for_callback(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_mark_approved") == 0) o = emit_tools_mcp_dashboard_oauth_mark_approved(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_mark_error") == 0) o = emit_tools_mcp_dashboard_oauth_mark_error(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_mark_worker_done") == 0) o = emit_tools_mcp_dashboard_oauth_mark_worker_done(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_worker_done") == 0) o = emit_tools_mcp_dashboard_oauth_worker_done(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_dashboard_oauth_flow") == 0) o = emit_tools_mcp_dashboard_oauth_dashboard_oauth_flow(c);
        if (strcmp(op, "tools_mcp_dashboard_oauth_get_dashboard_oauth_flow") == 0) o = emit_tools_mcp_dashboard_oauth_get_dashboard_oauth_flow(c);
        if (strcmp(op, "tools_online_research_clear_expired") == 0) o = emit_tools_online_research_clear_expired(c);
        if (strcmp(op, "tools_online_research_u__aenter__") == 0) o = emit_tools_online_research_u__aenter__(c);
        if (strcmp(op, "tools_online_research_u__aexit__") == 0) o = emit_tools_online_research_u__aexit__(c);
        if (strcmp(op, "tools_online_research_search_duckduckgo") == 0) o = emit_tools_online_research_search_duckduckgo(c);
        if (strcmp(op, "tools_online_research_search_brave") == 0) o = emit_tools_online_research_search_brave(c);
        if (strcmp(op, "tools_online_research_search_google_cse") == 0) o = emit_tools_online_research_search_google_cse(c);
        if (strcmp(op, "tools_online_research_get_researcher") == 0) o = emit_tools_online_research_get_researcher(c);
        if (strcmp(op, "tools_online_research_close_researcher") == 0) o = emit_tools_online_research_close_researcher(c);
        if (strcmp(op, "tools_online_research_research_model_benchmarks") == 0) o = emit_tools_online_research_research_model_benchmarks(c);
        if (strcmp(op, "tools_online_research_research_general") == 0) o = emit_tools_online_research_research_general(c);
        if (strcmp(op, "tools_image_source_resolve_image_source") == 0) o = emit_tools_image_source_resolve_image_source(c);
        if (strcmp(op, "tools_image_source_u_resolve_data_url") == 0) o = emit_tools_image_source_u_resolve_data_url(c);
        if (strcmp(op, "tools_image_source_u_http_block_reason") == 0) o = emit_tools_image_source_u_http_block_reason(c);
        if (strcmp(op, "tools_image_source_u_download_to_bytes") == 0) o = emit_tools_image_source_u_download_to_bytes(c);
        if (strcmp(op, "tools_image_source_u_is_local_terminal_backend") == 0) o = emit_tools_image_source_u_is_local_terminal_backend(c);
        if (strcmp(op, "tools_image_source_u_media_cache_roots") == 0) o = emit_tools_image_source_u_media_cache_roots(c);
        if (strcmp(op, "tools_image_source_u_permitted_host_read_target") == 0) o = emit_tools_image_source_u_permitted_host_read_target(c);
        if (strcmp(op, "tools_image_source_u_resolve_container_fallback") == 0) o = emit_tools_image_source_u_resolve_container_fallback(c);
        if (strcmp(op, "tools_kanban_tools_u_is_delegated_child_context") == 0) o = emit_tools_kanban_tools_u_is_delegated_child_context(c);
        if (strcmp(op, "tools_kanban_tools_u_reject_delegated_child_mutation") == 0) o = emit_tools_kanban_tools_u_reject_delegated_child_mutation(c);
        if (strcmp(op, "tools_kanban_tools_u_connect") == 0) o = emit_tools_kanban_tools_u_connect(c);
        if (strcmp(op, "tools_kanban_tools_heartbeat_current_worker_from_env") == 0) o = emit_tools_kanban_tools_heartbeat_current_worker_from_env(c);
        if (strcmp(op, "tools_kanban_tools_u_handle_attach") == 0) o = emit_tools_kanban_tools_u_handle_attach(c);
        if (strcmp(op, "tools_kanban_tools_u_download_url_with_cap") == 0) o = emit_tools_kanban_tools_u_download_url_with_cap(c);
        if (strcmp(op, "tools_kanban_tools_u_handle_attach_url") == 0) o = emit_tools_kanban_tools_u_handle_attach_url(c);
        if (strcmp(op, "tools_kanban_tools_u_handle_attachments") == 0) o = emit_tools_kanban_tools_u_handle_attachments(c);
        if (strcmp(op, "tools_tool_backend_helpers_managed_nous_tools_enabled") == 0) o = emit_tools_tool_backend_helpers_managed_nous_tools_enabled(c);
        if (strcmp(op, "tools_tool_backend_helpers_normalize_browser_cloud_provider") == 0) o = emit_tools_tool_backend_helpers_normalize_browser_cloud_provider(c);
        if (strcmp(op, "tools_tool_backend_helpers_coerce_modal_mode") == 0) o = emit_tools_tool_backend_helpers_coerce_modal_mode(c);
        if (strcmp(op, "tools_tool_backend_helpers_normalize_modal_mode") == 0) o = emit_tools_tool_backend_helpers_normalize_modal_mode(c);
        if (strcmp(op, "tools_tool_backend_helpers_has_direct_modal_credentials") == 0) o = emit_tools_tool_backend_helpers_has_direct_modal_credentials(c);
        if (strcmp(op, "tools_tool_backend_helpers_resolve_modal_backend_state") == 0) o = emit_tools_tool_backend_helpers_resolve_modal_backend_state(c);
        if (strcmp(op, "tools_tool_backend_helpers_resolve_openai_audio_api_key") == 0) o = emit_tools_tool_backend_helpers_resolve_openai_audio_api_key(c);
        if (strcmp(op, "tools_tool_backend_helpers_prefers_gateway") == 0) o = emit_tools_tool_backend_helpers_prefers_gateway(c);
        if (strcmp(op, "tools_skills_hub_u_referenced_support_paths") == 0) o = emit_tools_skills_hub_u_referenced_support_paths(c);
        if (strcmp(op, "tools_skills_hub_source_url_for_bundle") == 0) o = emit_tools_skills_hub_source_url_for_bundle(c);
        if (strcmp(op, "tools_skills_hub_u_ssrf_safe_http_get") == 0) o = emit_tools_skills_hub_u_ssrf_safe_http_get(c);
        if (strcmp(op, "tools_skills_hub_u_fetch_file_bytes") == 0) o = emit_tools_skills_hub_u_fetch_file_bytes(c);
        if (strcmp(op, "tools_skills_hub_u_fetch_bytes") == 0) o = emit_tools_skills_hub_u_fetch_bytes(c);
        if (strcmp(op, "tools_skills_hub_u_find_skill_dir") == 0) o = emit_tools_skills_hub_u_find_skill_dir(c);
        if (strcmp(op, "tools_skills_hub_u_parse_frontmatter") == 0) o = emit_tools_skills_hub_u_parse_frontmatter(c);
        if (strcmp(op, "tools_xai_video_tools_u_configured_for_xai_video") == 0) o = emit_tools_xai_video_tools_u_configured_for_xai_video(c);
        if (strcmp(op, "tools_xai_video_tools_u_check_xai_video_requirements") == 0) o = emit_tools_xai_video_tools_u_check_xai_video_requirements(c);
        if (strcmp(op, "tools_xai_video_tools_u_clean_string") == 0) o = emit_tools_xai_video_tools_u_clean_string(c);
        if (strcmp(op, "tools_xai_video_tools_u_provider_not_configured_error") == 0) o = emit_tools_xai_video_tools_u_provider_not_configured_error(c);
        if (strcmp(op, "tools_xai_video_tools_u_normalize_public_video_url") == 0) o = emit_tools_xai_video_tools_u_normalize_public_video_url(c);
        if (strcmp(op, "tools_xai_video_tools_u_handle_xai_video_edit") == 0) o = emit_tools_xai_video_tools_u_handle_xai_video_edit(c);
        if (strcmp(op, "tools_xai_video_tools_u_handle_xai_video_extend") == 0) o = emit_tools_xai_video_tools_u_handle_xai_video_extend(c);
        if (strcmp(op, "tools_computer_use_permissions_u_resolve_driver_cmd") == 0) o = emit_tools_computer_use_permissions_u_resolve_driver_cmd(c);
        if (strcmp(op, "tools_computer_use_permissions_u_child_env") == 0) o = emit_tools_computer_use_permissions_u_child_env(c);
        if (strcmp(op, "tools_computer_use_permissions_u_json_out") == 0) o = emit_tools_computer_use_permissions_u_json_out(c);
        if (strcmp(op, "tools_computer_use_permissions_u_mac_permissions") == 0) o = emit_tools_computer_use_permissions_u_mac_permissions(c);
        if (strcmp(op, "tools_computer_use_permissions_computer_use_status") == 0) o = emit_tools_computer_use_permissions_computer_use_status(c);
        if (strcmp(op, "tools_computer_use_permissions_request_permissions_grant") == 0) o = emit_tools_computer_use_permissions_request_permissions_grant(c);
        if (strcmp(op, "tools_project_tools_set_project_workspace_callback") == 0) o = emit_tools_project_tools_set_project_workspace_callback(c);
        if (strcmp(op, "tools_project_tools_u_primary_path") == 0) o = emit_tools_project_tools_u_primary_path(c);
        if (strcmp(op, "tools_project_tools_u_apply_workspace") == 0) o = emit_tools_project_tools_u_apply_workspace(c);
        if (strcmp(op, "tools_project_tools_project_list") == 0) o = emit_tools_project_tools_project_list(c);
        if (strcmp(op, "tools_project_tools_project_create") == 0) o = emit_tools_project_tools_project_create(c);
        if (strcmp(op, "tools_project_tools_project_switch") == 0) o = emit_tools_project_tools_project_switch(c);
        if (strcmp(op, "tools_credential_files_register_credential_file") == 0) o = emit_tools_credential_files_register_credential_file(c);
        if (strcmp(op, "tools_credential_files_register_credential_files") == 0) o = emit_tools_credential_files_register_credential_files(c);
        if (strcmp(op, "tools_credential_files_iter_skills_files") == 0) o = emit_tools_credential_files_iter_skills_files(c);
        if (strcmp(op, "tools_credential_files_from_agent_visible_cache_path") == 0) o = emit_tools_credential_files_from_agent_visible_cache_path(c);
        if (strcmp(op, "tools_credential_files_iter_cache_files") == 0) o = emit_tools_credential_files_iter_cache_files(c);
        if (strcmp(op, "tools_hook_output_spill_u_coerce_non_negative_int") == 0) o = emit_tools_hook_output_spill_u_coerce_non_negative_int(c);
        if (strcmp(op, "tools_hook_output_spill_get_spill_config") == 0) o = emit_tools_hook_output_spill_get_spill_config(c);
        if (strcmp(op, "tools_hook_output_spill_u_resolve_spill_dir") == 0) o = emit_tools_hook_output_spill_u_resolve_spill_dir(c);
        if (strcmp(op, "tools_hook_output_spill_u_build_preview") == 0) o = emit_tools_hook_output_spill_u_build_preview(c);
        if (strcmp(op, "tools_hook_output_spill_spill_if_oversized") == 0) o = emit_tools_hook_output_spill_spill_if_oversized(c);
        if (strcmp(op, "tools_session_search_tool_u_is_compaction_summary") == 0) o = emit_tools_session_search_tool_u_is_compaction_summary(c);
        if (strcmp(op, "tools_session_search_tool_u_resolve_lineage") == 0) o = emit_tools_session_search_tool_u_resolve_lineage(c);
        if (strcmp(op, "tools_session_search_tool_u_is_compression_ended") == 0) o = emit_tools_session_search_tool_u_is_compression_ended(c);
        if (strcmp(op, "tools_session_search_tool_u_is_compacted_message") == 0) o = emit_tools_session_search_tool_u_is_compacted_message(c);
        if (strcmp(op, "tools_session_search_tool_u_annotate_rebuild_status") == 0) o = emit_tools_session_search_tool_u_annotate_rebuild_status(c);
        if (strcmp(op, "tools_browser_tool_u_is_headed_mode") == 0) o = emit_tools_browser_tool_u_is_headed_mode(c);
        if (strcmp(op, "tools_browser_tool_u_store_full_snapshot") == 0) o = emit_tools_browser_tool_u_store_full_snapshot(c);
        if (strcmp(op, "tools_browser_tool_u_restrict_browser_evaluate") == 0) o = emit_tools_browser_tool_u_restrict_browser_evaluate(c);
        if (strcmp(op, "tools_browser_tool_u_camofox_current_page_private_url") == 0) o = emit_tools_browser_tool_u_camofox_current_page_private_url(c);
        if (strcmp(op, "tools_checkpoint_manager_u_volume_evidence") == 0) o = emit_tools_checkpoint_manager_u_volume_evidence(c);
        if (strcmp(op, "tools_checkpoint_manager_u_pre_v2_shadow_repos") == 0) o = emit_tools_checkpoint_manager_u_pre_v2_shadow_repos(c);
        if (strcmp(op, "tools_checkpoint_manager_u_workdir_is_observably_gone") == 0) o = emit_tools_checkpoint_manager_u_workdir_is_observably_gone(c);
        if (strcmp(op, "tools_checkpoint_manager_u_dir_has_any_entry") == 0) o = emit_tools_checkpoint_manager_u_dir_has_any_entry(c);
        if (strcmp(op, "tools_computer_use_doctor_u_cua_child_env") == 0) o = emit_tools_computer_use_doctor_u_cua_child_env(c);
        if (strcmp(op, "tools_computer_use_doctor_u_sanitized_cua_env") == 0) o = emit_tools_computer_use_doctor_u_sanitized_cua_env(c);
        if (strcmp(op, "tools_computer_use_doctor_u_drive_health_report") == 0) o = emit_tools_computer_use_doctor_u_drive_health_report(c);
        if (strcmp(op, "tools_computer_use_doctor_u_print_text_report") == 0) o = emit_tools_computer_use_doctor_u_print_text_report(c);
        if (strcmp(op, "tools_skill_provenance_set_current_write_origin") == 0) o = emit_tools_skill_provenance_set_current_write_origin(c);
        if (strcmp(op, "tools_skill_provenance_reset_current_write_origin") == 0) o = emit_tools_skill_provenance_reset_current_write_origin(c);
        if (strcmp(op, "tools_skill_provenance_get_current_write_origin") == 0) o = emit_tools_skill_provenance_get_current_write_origin(c);
        if (strcmp(op, "tools_skill_provenance_is_background_review") == 0) o = emit_tools_skill_provenance_is_background_review(c);
        if (strcmp(op, "tools_web_tools_u_web_extract_url") == 0) o = emit_tools_web_tools_u_web_extract_url(c);
        if (strcmp(op, "tools_web_tools_u_registered_web_provider") == 0) o = emit_tools_web_tools_u_registered_web_provider(c);
        if (strcmp(op, "tools_web_tools_u_registered_web_provider_available") == 0) o = emit_tools_web_tools_u_registered_web_provider_available(c);
        if (strcmp(op, "tools_web_tools_u_list_registered_web_providers") == 0) o = emit_tools_web_tools_u_list_registered_web_providers(c);
        if (strcmp(op, "tools_env_probe_u_probe_worker") == 0) o = emit_tools_env_probe_u_probe_worker(c);
        if (strcmp(op, "tools_env_probe_u_ensure_probe_started") == 0) o = emit_tools_env_probe_u_ensure_probe_started(c);
        if (strcmp(op, "tools_env_probe_warm_environment_probe_async") == 0) o = emit_tools_env_probe_warm_environment_probe_async(c);
        if (strcmp(op, "tools_mcp_stdio_watchdog_u_is_orphaned") == 0) o = emit_tools_mcp_stdio_watchdog_u_is_orphaned(c);
        if (strcmp(op, "tools_mcp_stdio_watchdog_u_terminate_process_group") == 0) o = emit_tools_mcp_stdio_watchdog_u_terminate_process_group(c);
        if (strcmp(op, "tools_mcp_stdio_watchdog_u_watchdog_loop") == 0) o = emit_tools_mcp_stdio_watchdog_u_watchdog_loop(c);
        if (strcmp(op, "tools_open_preview_tool_u_normalize_target") == 0) o = emit_tools_open_preview_tool_u_normalize_target(c);
        if (strcmp(op, "tools_open_preview_tool_open_preview_tool") == 0) o = emit_tools_open_preview_tool_open_preview_tool(c);
        if (strcmp(op, "tools_open_preview_tool_check_open_preview_requirements") == 0) o = emit_tools_open_preview_tool_check_open_preview_requirements(c);
        if (strcmp(op, "tools_tts_streaming_mark_speech_interrupted") == 0) o = emit_tools_tts_streaming_mark_speech_interrupted(c);
        if (strcmp(op, "tools_tts_streaming_take_speech_interrupted") == 0) o = emit_tools_tts_streaming_take_speech_interrupted(c);
        if (strcmp(op, "tools_tts_streaming_resolve_streaming_provider") == 0) o = emit_tools_tts_streaming_resolve_streaming_provider(c);
        if (strcmp(op, "tools_computer_use_backend_list_windows") == 0) o = emit_tools_computer_use_backend_list_windows(c);
        if (strcmp(op, "tools_computer_use_backend_set_value") == 0) o = emit_tools_computer_use_backend_set_value(c);
        if (strcmp(op, "tools_focus_pane_tool_focus_pane_tool") == 0) o = emit_tools_focus_pane_tool_focus_pane_tool(c);
        if (strcmp(op, "tools_focus_pane_tool_check_focus_pane_requirements") == 0) o = emit_tools_focus_pane_tool_check_focus_pane_requirements(c);
        if (strcmp(op, "tools_image_generation_tool_check_image_generation_requirements") == 0) o = emit_tools_image_generation_tool_check_image_generation_requirements(c);
        if (strcmp(op, "tools_image_generation_tool_u_dispatch_to_plugin_provider") == 0) o = emit_tools_image_generation_tool_u_dispatch_to_plugin_provider(c);
        if (strcmp(op, "tools_microsoft_graph_client_post_json") == 0) o = emit_tools_microsoft_graph_client_post_json(c);
        if (strcmp(op, "tools_microsoft_graph_client_u_request") == 0) o = emit_tools_microsoft_graph_client_u_request(c);
        if (strcmp(op, "tools_send_message_tool_u_media_caption_split") == 0) o = emit_tools_send_message_tool_u_media_caption_split(c);
        if (strcmp(op, "tools_send_message_tool_u_resolve_slack_user_target") == 0) o = emit_tools_send_message_tool_u_resolve_slack_user_target(c);
        if (strcmp(op, "tools_thread_context_u_callback_api") == 0) o = emit_tools_thread_context_u_callback_api(c);
        if (strcmp(op, "tools_thread_context_propagate_context_to_thread") == 0) o = emit_tools_thread_context_propagate_context_to_thread(c);
        if (strcmp(op, "tools_video_generation_tool_u_resolve_active_provider") == 0) o = emit_tools_video_generation_tool_u_resolve_active_provider(c);
        if (strcmp(op, "tools_video_generation_tool_u_handle_video_generate") == 0) o = emit_tools_video_generation_tool_u_handle_video_generate(c);
        if (strcmp(op, "tools_voice_mode_cancel") == 0) o = emit_tools_voice_mode_cancel(c);
        if (strcmp(op, "tools_voice_mode_cancel_2") == 0) o = emit_tools_voice_mode_cancel_2(c);
        if (strcmp(op, "tools_ansi_strip_sanitize_display_text") == 0) o = emit_tools_ansi_strip_sanitize_display_text(c);
        if (strcmp(op, "tools_binary_extensions_has_binary_extension") == 0) o = emit_tools_binary_extensions_has_binary_extension(c);
        if (strcmp(op, "tools_browser_camofox_state_get_camofox_state_dir") == 0) o = emit_tools_browser_camofox_state_get_camofox_state_dir(c);
        if (strcmp(op, "tools_budget_config_resolve_threshold") == 0) o = emit_tools_budget_config_resolve_threshold(c);
        if (strcmp(op, "tools_clarify_gateway_resolve_clarify_timeout") == 0) o = emit_tools_clarify_gateway_resolve_clarify_timeout(c);
        if (strcmp(op, "tools_daemon_pool_u_adjust_thread_count") == 0) o = emit_tools_daemon_pool_u_adjust_thread_count(c);
        if (strcmp(op, "tools_debug_helpers_log_call") == 0) o = emit_tools_debug_helpers_log_call(c);
        if (strcmp(op, "tools_desktop_ui_set_emitter") == 0) o = emit_tools_desktop_ui_set_emitter(c);
        if (strcmp(op, "tools_environments_file_sync_u_resolve_host_path") == 0) o = emit_tools_environments_file_sync_u_resolve_host_path(c);
        if (strcmp(op, "tools_environments_managed_mod_u_request") == 0) o = emit_tools_environments_managed_mod_u_request(c);
        if (strcmp(op, "tools_feishu_drive_tool_u_do_request") == 0) o = emit_tools_feishu_drive_tool_u_do_request(c);
        if (strcmp(op, "tools_interrupt_clear_current_thread_interrupt") == 0) o = emit_tools_interrupt_clear_current_thread_interrupt(c);
        if (strcmp(op, "tools_mixture_of_agents_tool_u_build_auth_header") == 0) o = emit_tools_mixture_of_agents_tool_u_build_auth_header(c);
        if (strcmp(op, "tools_moa_performance_u_build_auth_header") == 0) o = emit_tools_moa_performance_u_build_auth_header(c);
        if (strcmp(op, "tools_skills_guard_scan_file") == 0) o = emit_tools_skills_guard_scan_file(c);
        if (strcmp(op, "tools_tirith_security_u_record_tirith_crash") == 0) o = emit_tools_tirith_security_u_record_tirith_crash(c);
        if (strcmp(op, "tools_tool_search_u_safe_float") == 0) o = emit_tools_tool_search_u_safe_float(c);
        if (strcmp(op, "tools_website_policy_check_website_access") == 0) o = emit_tools_website_policy_check_website_access(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
