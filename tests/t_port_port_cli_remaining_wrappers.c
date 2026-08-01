/* AUTO-GENERATED integration oracle harness for port_cli_remaining_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_cli_remaining_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int hermes_cli_dashboard_auth_rout_u_redirect_uri(const char *);
extern int hermes_cli_dashboard_auth_rout_u_prefix(const char *);
extern int hermes_cli_dashboard_auth_rout_login_page(const char *);
extern int hermes_cli_dashboard_auth_rout_api_auth_providers(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_login(const char *);
extern int hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_native_authorize(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_callback(const char *);
extern int hermes_cli_dashboard_auth_rout_u_validate_post_login_target(const char *);
extern int hermes_cli_dashboard_auth_rout_u_password_rate_limited(const char *);
extern int hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_password_login(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_logout(const char *);
extern int hermes_cli_dashboard_auth_rout_api_auth_me(const char *);
extern int hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_native_token(const char *);
extern int hermes_cli_dashboard_auth_rout_auth_native_refresh(const char *);
extern int hermes_cli_debug_u_pending_file(const char *);
extern int hermes_cli_debug_u_best_effort_sweep_expired_pastes(const char *);
extern int hermes_cli_debug_delete_paste(const char *);
extern int hermes_cli_debug_u_schedule_auto_delete(const char *);
extern int hermes_cli_debug_u_upload_paste_rs(const char *);
extern int hermes_cli_debug_u_upload_dpaste_com(const char *);
extern int hermes_cli_debug_upload_to_pastebin(const char *);
extern int hermes_cli_debug_u_primary_log_path(const char *);
extern int hermes_cli_debug_u_resolve_log_path(const char *);
extern int hermes_cli_debug_u_capture_log_snapshot(const char *);
extern int hermes_cli_debug_u_capture_default_log_snapshots(const char *);
extern int hermes_cli_debug_u_capture_dump(const char *);
extern int hermes_cli_debug_collect_share_bundle(const char *);
extern int hermes_cli_debug_build_nous_bundle(const char *);
extern int hermes_cli_debug_u_confirm_upload(const char *);
extern int hermes_cli_debug_u_run_debug_share_nous(const char *);
extern int hermes_cli_debug_run_debug(const char *);
extern int hermes_cli_mcp_config_u_confirm(const char *);
extern int hermes_cli_mcp_config_u_get_mcp_servers(const char *);
extern int hermes_cli_mcp_config_u_save_mcp_server(const char *);
extern int hermes_cli_mcp_config_u_remove_mcp_server(const char *);
extern int hermes_cli_mcp_config_u_replace_mcp_servers(const char *);
extern int hermes_cli_mcp_config_u_env_key_for_server(const char *);
extern int hermes_cli_mcp_config_u_strip_bearer_prefix(const char *);
extern int hermes_cli_mcp_config_u_bearer_auth_headers(const char *);
extern int hermes_cli_mcp_config_u_save_bearer_auth_token(const char *);
extern int hermes_cli_mcp_config_u_parse_env_assignments(const char *);
extern int hermes_cli_mcp_config_u_apply_mcp_preset(const char *);
extern int hermes_cli_mcp_config_u_resolve_mcp_server_config(const char *);
extern int hermes_cli_mcp_config_u_probe_single_server(const char *);
extern int hermes_cli_mcp_config_u_oauth_tokens_present(const char *);
extern int hermes_cli_mcp_config_u_unwrap_exception_group(const char *);
extern int hermes_cli_mcp_config_u_reauth_oauth_server(const char *);
extern int hermes_cli_mcp_config_cmd_mcp_reauth(const char *);
extern int hermes_cli_cli_billing_mixin_u_print_usage_cta(const char *);
extern int hermes_cli_cli_billing_mixin_u_show_subscription(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_overview(const char *);
extern int hermes_cli_cli_billing_mixin_u_open_url_in_browser(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_free_catalog(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_open_portal(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_change_menu(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_pick_tier(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_apply(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_render_error(const char *);
extern int hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(const char *);
extern int hermes_cli_cli_billing_mixin_u_usage_bar_lines(const char *);
extern int hermes_cli_cli_billing_mixin_u_billing_add_card_flow(const char *);
extern int hermes_cli_pets_u_cmd_install(const char *);
extern int hermes_cli_pets_u_cmd_remove(const char *);
extern int hermes_cli_pets_u_cmd_select(const char *);
extern int hermes_cli_pets_u_cmd_off(const char *);
extern int hermes_cli_pets_u_cmd_scale(const char *);
extern int hermes_cli_pets_u_cmd_show(const char *);
extern int hermes_cli_pets_u_pet_config(const char *);
extern int hermes_cli_pets_u_has_active_pet(const char *);
extern int hermes_cli_pets_u_set_active(const char *);
extern int hermes_cli_pets_set_pet_scale(const char *);
extern int hermes_cli_pets_toggle_pet_display(const char *);
extern int hermes_cli_pets_print_pet_gallery(const char *);
extern int hermes_cli_pets_u_clear_active_if(const char *);
extern int hermes_cli_pets_u_rename_active_if(const char *);
extern int hermes_cli_pets_u_interactive_pick(const char *);
extern int hermes_cli_pets_register_cli(const char *);
extern int hermes_cli_curses_ui_radio_item_plain(const char *);
extern int hermes_cli_curses_ui_u_curses_style_attr(const char *);
extern int hermes_cli_curses_ui_u_draw_description_line(const char *);
extern int hermes_cli_curses_ui_u_draw_radio_item(const char *);
extern int hermes_cli_curses_ui_u_move_filtered_cursor(const char *);
extern int hermes_cli_curses_ui_u_scroll_for_cursor(const char *);
extern int hermes_cli_curses_ui_u_handle_active_search_key(const char *);
extern int hermes_cli_curses_ui_flush_stdin(const char *);
extern int hermes_cli_curses_ui_read_menu_key(const char *);
extern int hermes_cli_curses_ui_u_decode_menu_key(const char *);
extern int hermes_cli_curses_ui_u_run_curses_menu(const char *);
extern int hermes_cli_curses_ui_format_radio_item_ansi(const char *);
extern int hermes_cli_curses_ui_u_radio_numbered_fallback(const char *);
extern int hermes_cli_curses_ui_u_numbered_single_fallback(const char *);
extern int hermes_cli_curses_ui_u_numbered_fallback(const char *);
extern int hermes_cli_mcp_catalog_u_catalog_root(const char *);
extern int hermes_cli_mcp_catalog_u_parse_env_spec(const char *);
extern int hermes_cli_mcp_catalog_u_parse_manifest(const char *);
extern int hermes_cli_mcp_catalog_catalog_diagnostics(const char *);
extern int hermes_cli_mcp_catalog_get_entry(const char *);
extern int hermes_cli_mcp_catalog_u_install_root(const char *);
extern int hermes_cli_mcp_catalog_u_run_bootstrap(const char *);
extern int hermes_cli_mcp_catalog_u_do_git_install(const char *);
extern int hermes_cli_mcp_catalog_u_expand_install_dir(const char *);
extern int hermes_cli_mcp_catalog_u_prompt_env_vars(const char *);
extern int hermes_cli_mcp_catalog_u_build_server_config(const char *);
extern int hermes_cli_mcp_catalog_u_read_prior_tool_selection(const char *);
extern int hermes_cli_mcp_catalog_u_probe_tools(const char *);
extern int hermes_cli_mcp_catalog_u_write_tools_include(const char *);
extern int hermes_cli_mcp_catalog_u_apply_tool_selection(const char *);
extern int hermes_cli_projects_cmd_build_parser(const char *);
extern int hermes_cli_projects_cmd_projects_command(const char *);
extern int hermes_cli_projects_cmd_u_with_project(const char *);
extern int hermes_cli_projects_cmd_u_print_project(const char *);
extern int hermes_cli_projects_cmd_u_cmd_create(const char *);
extern int hermes_cli_projects_cmd_u_cmd_show(const char *);
extern int hermes_cli_projects_cmd_u_cmd_add_folder(const char *);
extern int hermes_cli_projects_cmd_u_cmd_remove_folder(const char *);
extern int hermes_cli_projects_cmd_u_cmd_rename(const char *);
extern int hermes_cli_projects_cmd_u_cmd_set_primary(const char *);
extern int hermes_cli_projects_cmd_u_cmd_use(const char *);
extern int hermes_cli_projects_cmd_u_cmd_archive(const char *);
extern int hermes_cli_projects_cmd_u_cmd_restore(const char *);
extern int hermes_cli_projects_cmd_u_cmd_bind_board(const char *);
extern int hermes_cli_projects_cmd_u_sync_board_default_workdir(const char *);
extern int hermes_cli_auth_commands_u_get_custom_provider_names(const char *);
extern int hermes_cli_auth_commands_u_resolve_custom_provider_input(const char *);
extern int hermes_cli_auth_commands_u_provider_base_url(const char *);
extern int hermes_cli_auth_commands_u_oauth_default_label(const char *);
extern int hermes_cli_auth_commands_u_api_key_default_label(const char *);
extern int hermes_cli_auth_commands_u_display_source(const char *);
extern int hermes_cli_auth_commands_u_classify_exhausted_status(const char *);
extern int hermes_cli_auth_commands_u_format_exhausted_status(const char *);
extern int hermes_cli_auth_commands_u_interactive_auth(const char *);
extern int hermes_cli_auth_commands_u_pick_provider(const char *);
extern int hermes_cli_auth_commands_u_interactive_add(const char *);
extern int hermes_cli_auth_commands_u_interactive_remove(const char *);
extern int hermes_cli_auth_commands_u_interactive_reset(const char *);
extern int hermes_cli_auth_commands_u_interactive_strategy(const char *);
extern int hermes_cli_profile_distributio_owned_paths(const char *);
extern int hermes_cli_profile_distributio_u_load_yaml(const char *);
extern int hermes_cli_profile_distributio_u_dump_yaml(const char *);
extern int hermes_cli_profile_distributio_u_parse_semver(const char *);
extern int hermes_cli_profile_distributio_check_hermes_requires(const char *);
extern int hermes_cli_profile_distributio_u_env_template_from_manifest(const char *);
extern int hermes_cli_profile_distributio_u_looks_like_git_url(const char *);
extern int hermes_cli_profile_distributio_u_git_clone(const char *);
extern int hermes_cli_profile_distributio_u_stage_source(const char *);
extern int hermes_cli_profile_distributio_u_reject_distribution_symlinks(const char *);
extern int hermes_cli_profile_distributio_u_has_cron_jobs(const char *);
extern int hermes_cli_profile_distributio_u_count_skills(const char *);
extern int hermes_cli_profile_distributio_u_copy_dist_payload(const char *);
extern int hermes_cli_profile_distributio_u_bootstrap_user_dirs(const char *);
extern int hermes_cli_security_audit_u_discover_venv(const char *);
extern int hermes_cli_security_audit_u_parse_requirements(const char *);
extern int hermes_cli_security_audit_u_parse_pyproject_pins(const char *);
extern int hermes_cli_security_audit_u_discover_plugins(const char *);
extern int hermes_cli_security_audit_u_extract_mcp_component(const char *);
extern int hermes_cli_security_audit_u_discover_mcp(const char *);
extern int hermes_cli_security_audit_u_http_post_json(const char *);
extern int hermes_cli_security_audit_u_http_get_json(const char *);
extern int hermes_cli_security_audit_u_osv_query_batch(const char *);
extern int hermes_cli_security_audit_u_osv_fetch_details(const char *);
extern int hermes_cli_security_audit_u_render_human(const char *);
extern int hermes_cli_security_audit_u_render_json(const char *);
extern int hermes_cli_security_audit_u_count_components(const char *);
extern int hermes_cli_security_audit_cmd_security_audit(const char *);
extern int hermes_cli_telegram_managed_bo_u_api_url(const char *);
extern int hermes_cli_telegram_managed_bo_u_parse_owner_user_id(const char *);
extern int hermes_cli_telegram_managed_bo_render_qr_terminal(const char *);
extern int hermes_cli_telegram_managed_bo_print_qr_code(const char *);
extern int hermes_cli_telegram_managed_bo_generate_username_slug(const char *);
extern int hermes_cli_telegram_managed_bo_generate_bot_username(const char *);
extern int hermes_cli_telegram_managed_bo_generate_deep_link(const char *);
extern int hermes_cli_telegram_managed_bo_generate_pairing_nonce(const char *);
extern int hermes_cli_telegram_managed_bo_create_pairing(const char *);
extern int hermes_cli_telegram_managed_bo_poll_pairing_result_once(const char *);
extern int hermes_cli_telegram_managed_bo_poll_pairing_once(const char *);
extern int hermes_cli_telegram_managed_bo_poll_for_setup_result(const char *);
extern int hermes_cli_telegram_managed_bo_poll_for_token(const char *);
extern int hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(const char *);
extern int hermes_cli_backup_u_collect_memory_provider_external_paths(const char *);
extern int hermes_cli_backup_u_iter_external_files(const char *);
extern int hermes_cli_backup_verify_sqlite_integrity(const char *);
extern int hermes_cli_backup_copy_db_and_verify(const char *);
extern int hermes_cli_backup_run_backup(const char *);
extern int hermes_cli_backup_run_import(const char *);
extern int hermes_cli_backup_create_quick_snapshot(const char *);
extern int hermes_cli_backup_list_quick_snapshots(const char *);
extern int hermes_cli_backup_restore_quick_snapshot(const char *);
extern int hermes_cli_backup_run_quick_backup(const char *);
extern int hermes_cli_backup_u_write_full_zip_backup(const char *);
extern int hermes_cli_backup_create_pre_update_backup(const char *);
extern int hermes_cli_backup_create_pre_migration_backup(const char *);
extern int hermes_cli_kanban_diagnostics_u_aux_slot_explicit(const char *);
extern int hermes_cli_kanban_diagnostics_u_main_model_visible(const char *);
extern int hermes_cli_kanban_diagnostics_triage_aux_status(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_repeated_failures(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_repeated_crashes(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling(const char *);
extern int hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready(const char *);
extern int hermes_cli_kanban_diagnostics_config_from_kanban_config(const char *);
extern int hermes_cli_kanban_diagnostics_config_from_runtime_config(const char *);
extern int hermes_cli_model_catalog_u_load_catalog_config(const char *);
extern int hermes_cli_model_catalog_u_cache_path(const char *);
extern int hermes_cli_model_catalog_u_fetch_manifest_with_fallback(const char *);
extern int hermes_cli_model_catalog_u_validate_manifest(const char *);
extern int hermes_cli_model_catalog_u_read_disk_cache(const char *);
extern int hermes_cli_model_catalog_u_write_disk_cache(const char *);
extern int hermes_cli_model_catalog_u_fetch_provider_override(const char *);
extern int hermes_cli_model_catalog_u_get_provider_block(const char *);
extern int hermes_cli_model_catalog_get_curated_openrouter_models(const char *);
extern int hermes_cli_model_catalog_get_curated_nous_models(const char *);
extern int hermes_cli_model_catalog_u_default_model_from_block(const char *);
extern int hermes_cli_model_catalog_get_default_model_from_cache(const char *);
extern int hermes_cli_model_catalog_reset_cache(const char *);
extern int hermes_cli_skills_hub_u_display_source(const char *);
extern int hermes_cli_skills_hub_u_resolve_short_name(const char *);
extern int hermes_cli_skills_hub_u_format_extra_metadata_lines(const char *);
extern int hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(const char *);
extern int hermes_cli_skills_hub_u_derive_category_from_install_path(const char *);
extern int hermes_cli_skills_hub_u_is_valid_installed_skill_name(const char *);
extern int hermes_cli_skills_hub_u_existing_categories(const char *);
extern int hermes_cli_skills_hub_u_prompt_for_skill_name(const char *);
extern int hermes_cli_skills_hub_u_prompt_for_category(const char *);
extern int hermes_cli_skills_hub_do_list_modified(const char *);
extern int hermes_cli_skills_hub_do_diff(const char *);
extern int hermes_cli_skills_hub_u_github_publish(const char *);
extern int hermes_cli_skills_hub_u_print_skills_help(const char *);
extern int hermes_cli_skin_engine_get_color(const char *);
extern int hermes_cli_skin_engine_get_spinner_wings(const char *);
extern int hermes_cli_skin_engine_get_branding(const char *);
extern int hermes_cli_skin_engine_u_skins_dir(const char *);
extern int hermes_cli_skin_engine_u_load_skin_from_yaml(const char *);
extern int hermes_cli_skin_engine_u_mapping_or_empty(const char *);
extern int hermes_cli_skin_engine_u_build_skin_config(const char *);
extern int hermes_cli_skin_engine_get_active_skin_name(const char *);
extern int hermes_cli_skin_engine_init_skin_from_config(const char *);
extern int hermes_cli_skin_engine_get_active_prompt_symbol(const char *);
extern int hermes_cli_skin_engine_get_active_help_header(const char *);
extern int hermes_cli_skin_engine_get_active_goodbye(const char *);
extern int hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(const char *);
extern int hermes_cli_claw_u_detect_openclaw_processes(const char *);
extern int hermes_cli_claw_u_warn_if_openclaw_running(const char *);
extern int hermes_cli_claw_u_warn_if_gateway_running(const char *);
extern int hermes_cli_claw_u_find_migration_script(const char *);
extern int hermes_cli_claw_u_load_migration_module(const char *);
extern int hermes_cli_claw_u_find_openclaw_dirs(const char *);
extern int hermes_cli_claw_u_scan_workspace_state(const char *);
extern int hermes_cli_claw_u_archive_directory(const char *);
extern int hermes_cli_claw_claw_command(const char *);
extern int hermes_cli_claw_u_cmd_migrate(const char *);
extern int hermes_cli_claw_u_cmd_cleanup(const char *);
extern int hermes_cli_claw_u_print_migration_report(const char *);
extern int hermes_cli_env_loader_get_secret_source(const char *);
extern int hermes_cli_env_loader_get_secret_source_values(const char *);
extern int hermes_cli_env_loader_reset_secret_source_cache(const char *);
extern int hermes_cli_env_loader_format_secret_source_suffix(const char *);
extern int hermes_cli_env_loader_u_format_offending_chars(const char *);
extern int hermes_cli_env_loader_u_sanitize_loaded_credentials(const char *);
extern int hermes_cli_env_loader_u_load_dotenv_with_fallback(const char *);
extern int hermes_cli_env_loader_u_sanitize_env_file_if_needed(const char *);
extern int hermes_cli_env_loader_u_apply_managed_env(const char *);
extern int hermes_cli_env_loader_u_apply_external_secret_sources(const char *);
extern int hermes_cli_env_loader_u_remediation_hint(const char *);
extern int hermes_cli_env_loader_u_load_secrets_config(const char *);
extern int hermes_cli_gui_uninstall_log_info(const char *);
extern int hermes_cli_gui_uninstall_log_success(const char *);
extern int hermes_cli_gui_uninstall_log_warn(const char *);
extern int hermes_cli_gui_uninstall_u_agent_root(const char *);
extern int hermes_cli_gui_uninstall_desktop_userdata_dir(const char *);
extern int hermes_cli_gui_uninstall_source_built_gui_artifacts(const char *);
extern int hermes_cli_gui_uninstall_packaged_gui_app_paths(const char *);
extern int hermes_cli_gui_uninstall_agent_is_installed(const char *);
extern int hermes_cli_gui_uninstall_gui_is_installed(const char *);
extern int hermes_cli_gui_uninstall_gui_install_summary(const char *);
extern int hermes_cli_gui_uninstall_u_remove_path(const char *);
extern int hermes_cli_gui_uninstall_uninstall_gui(const char *);
extern int hermes_cli_active_sessions_coerce_max_concurrent_sessions(const char *);
extern int hermes_cli_active_sessions_resolve_max_concurrent_sessions(const char *);
extern int hermes_cli_active_sessions_active_session_limit_message(const char *);
extern int hermes_cli_active_sessions_u__enter__(const char *);
extern int hermes_cli_active_sessions_u__exit__(const char *);
extern int hermes_cli_active_sessions_u_read_entries(const char *);
extern int hermes_cli_active_sessions_u_write_entries(const char *);
extern int hermes_cli_active_sessions_u_process_start_time(const char *);
extern int hermes_cli_active_sessions_u_optional_float(const char *);
extern int hermes_cli_active_sessions_u_prune_dead(const char *);
extern int hermes_cli_active_sessions_transfer_active_session(const char *);
extern int hermes_cli_codex_runtime_plugi_u_translate_one_server(const char *);
extern int hermes_cli_codex_runtime_plugi_u_format_toml_value(const char *);
extern int hermes_cli_codex_runtime_plugi_u_quote_key(const char *);
extern int hermes_cli_codex_runtime_plugi_render_codex_toml_section(const char *);
extern int hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(const char *);
extern int hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(const char *);
extern int hermes_cli_codex_runtime_plugi_u_looks_like_table_header(const char *);
extern int hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(const char *);
extern int hermes_cli_codex_runtime_plugi_u_query_codex_plugins(const char *);
extern int hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(const char *);
extern int hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(const char *);
extern int hermes_cli_inventory_with_overrides(const char *);
extern int hermes_cli_inventory_build_models_payload(const char *);
extern int hermes_cli_inventory_build_model_options_payload(const char *);
extern int hermes_cli_inventory_u_apply_capabilities(const char *);
extern int hermes_cli_inventory_u_append_unconfigured_rows(const char *);
extern int hermes_cli_inventory_u_filter_explicit_provider_rows(const char *);
extern int hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(const char *);
extern int hermes_cli_inventory_u_apply_picker_hints(const char *);
extern int hermes_cli_inventory_u_reorder_canonical(const char *);
extern int hermes_cli_inventory_u_apply_pricing(const char *);
extern int hermes_cli_inventory_u_moa_provider_row(const char *);
extern int hermes_cli_journey_u_primary_hex(const char *);
extern int hermes_cli_journey_u_fade(const char *);
extern int hermes_cli_journey_u_row_to_text(const char *);
extern int hermes_cli_journey_u_term_size(const char *);
extern int hermes_cli_journey_u_frame_renderable(const char *);
extern int hermes_cli_journey_u_cmd_show(const char *);
extern int hermes_cli_journey_u_cmd_delete(const char *);
extern int hermes_cli_journey_u_cmd_edit(const char *);
extern int hermes_cli_journey_u_open_in_editor(const char *);
extern int hermes_cli_journey_register_cli(const char *);
extern int hermes_cli_journey_cmd_journey(const char *);
extern int hermes_cli_middleware_u_safe_copy(const char *);
extern int hermes_cli_middleware_apply_llm_request_middleware(const char *);
extern int hermes_cli_middleware_apply_tool_request_middleware(const char *);
extern int hermes_cli_middleware_apply_api_request_middleware(const char *);
extern int hermes_cli_middleware_run_llm_execution_middleware(const char *);
extern int hermes_cli_middleware_run_tool_execution_middleware(const char *);
extern int hermes_cli_middleware_run_api_execution_middleware(const char *);
extern int hermes_cli_middleware_u_invoke_middleware(const char *);
extern int hermes_cli_middleware_u_has_middleware(const char *);
extern int hermes_cli_middleware_u_get_middleware_callbacks(const char *);
extern int hermes_cli_middleware_u_run_execution_chain(const char *);
extern int hermes_cli_service_manager_u_s6_running(const char *);
extern int hermes_cli_service_manager_u_profile_dir_for_gateway_service(const char *);
extern int hermes_cli_service_manager_u_write_gateway_desired_state(const char *);
extern int hermes_cli_service_manager_u_seed_supervise_skeleton(const char *);
extern int hermes_cli_service_manager_u_service_dir(const char *);
extern int hermes_cli_service_manager_u_service_name(const char *);
extern int hermes_cli_service_manager_u_render_run_script(const char *);
extern int hermes_cli_service_manager_u_render_finish_script(const char *);
extern int hermes_cli_service_manager_u_render_log_run(const char *);
extern int hermes_cli_service_manager_u_run_svc(const char *);
extern int hermes_cli_service_manager_u_supervised_pid(const char *);
extern int hermes_cli_browser_connect_chrome_debug_data_dir(const char *);
extern int hermes_cli_browser_connect_u_chrome_debug_args(const char *);
extern int hermes_cli_browser_connect_discover_local_cdp_url(const char *);
extern int hermes_cli_browser_connect_local_port_in_use(const char *);
extern int hermes_cli_browser_connect_find_free_debug_port(const char *);
extern int hermes_cli_browser_connect_manual_chrome_debug_command(const char *);
extern int hermes_cli_browser_connect_u_detach_kwargs(const char *);
extern int hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(const char *);
extern int hermes_cli_browser_connect_u_read_stderr_tail(const char *);
extern int hermes_cli_browser_connect_launch_chrome_debug(const char *);
extern int hermes_cli_dashboard_auth_midd_u_path_is_public(const char *);
extern int hermes_cli_dashboard_auth_midd_u_ordered_session_providers(const char *);
extern int hermes_cli_dashboard_auth_midd_u_unauth_response(const char *);
extern int hermes_cli_dashboard_auth_midd_u_auto_sso_response(const char *);
extern int hermes_cli_dashboard_auth_midd_u_safe_next_target(const char *);
extern int hermes_cli_dashboard_auth_midd_u_extract_bearer(const char *);
extern int hermes_cli_dashboard_auth_midd_u_verify_bearer(const char *);
extern int hermes_cli_dashboard_auth_midd_gated_auth_middleware(const char *);
extern int hermes_cli_dashboard_auth_midd_u_expires_in_seconds(const char *);
extern int hermes_cli_dashboard_auth_midd_u_attempt_refresh(const char *);
extern int hermes_cli_dump_u_dotenv_key_names(const char *);
extern int hermes_cli_dump_u_get_git_commit(const char *);
extern int hermes_cli_dump_u_count_skills(const char *);
extern int hermes_cli_dump_u_count_mcp_servers(const char *);
extern int hermes_cli_dump_u_cron_summary(const char *);
extern int hermes_cli_dump_u_configured_platforms(const char *);
extern int hermes_cli_dump_u_memory_provider(const char *);
extern int hermes_cli_dump_u_get_model_and_provider(const char *);
extern int hermes_cli_dump_u_config_overrides(const char *);
extern int hermes_cli_dump_run_dump(const char *);
extern int hermes_cli_projects_db_projects_db_path(const char *);
extern int hermes_cli_projects_db_u_new_project_id(const char *);
extern int hermes_cli_projects_db_u_now(const char *);
extern int hermes_cli_projects_db_connect_closing(const char *);
extern int hermes_cli_projects_db_u_migrate_add_optional_columns(const char *);
extern int hermes_cli_projects_db_u_project_from_row(const char *);
extern int hermes_cli_projects_db_u_attach_folders(const char *);
extern int hermes_cli_projects_db_get_discovery_policy_key(const char *);
extern int hermes_cli_projects_db_reconcile_discovered_repos_policy(const char *);
extern int hermes_cli_projects_db_clear_discovered_repos(const char *);
extern int hermes_cli_pty_session_append(const char *);
extern int hermes_cli_pty_session_truncated(const char *);
extern int hermes_cli_pty_session_u_drain(const char *);
extern int hermes_cli_pty_session_detach(const char *);
extern int hermes_cli_pty_session_run_reaper(const char *);
extern int hermes_cli_pty_session_attach_or_spawn(const char *);
extern int hermes_cli_pty_session_detach_2(const char *);
extern int hermes_cli_pty_session_reap_idle(const char *);
extern int hermes_cli_pty_session_u_reap_one_idle_or_raise(const char *);
extern int hermes_cli_pty_session_close_all(const char *);
extern int hermes_cli_webhook_u_subscriptions_path(const char *);
extern int hermes_cli_webhook_u_load_subscriptions(const char *);
extern int hermes_cli_webhook_u_save_subscriptions(const char *);
extern int hermes_cli_webhook_u_get_webhook_config(const char *);
extern int hermes_cli_webhook_u_is_webhook_enabled(const char *);
extern int hermes_cli_webhook_u_get_webhook_base_url(const char *);
extern int hermes_cli_webhook_u_setup_hint(const char *);
extern int hermes_cli_webhook_u_require_webhook_enabled(const char *);
extern int hermes_cli_webhook_u_cmd_subscribe(const char *);
extern int hermes_cli_webhook_u_cmd_remove(const char *);
extern int hermes_cli_curator_u_cmd_run(const char *);
extern int hermes_cli_curator_u_cmd_pause(const char *);
extern int hermes_cli_curator_u_cmd_pin(const char *);
extern int hermes_cli_curator_u_cmd_unpin(const char *);
extern int hermes_cli_curator_u_cmd_restore(const char *);
extern int hermes_cli_curator_u_cmd_archive(const char *);
extern int hermes_cli_curator_u_idle_days(const char *);
extern int hermes_cli_curator_u_cmd_prune(const char *);
extern int hermes_cli_curator_u_cmd_list_archived(const char *);
extern int hermes_cli_onepassword_secrets_register_cli(const char *);
extern int hermes_cli_onepassword_secrets_cmd_set(const char *);
extern int hermes_cli_onepassword_secrets_cmd_remove(const char *);
extern int hermes_cli_onepassword_secrets_cmd_token(const char *);
extern int hermes_cli_onepassword_secrets_cmd_sync(const char *);
extern int hermes_cli_onepassword_secrets_cmd_disable(const char *);
extern int hermes_cli_onepassword_secrets_u_yn(const char *);
extern int hermes_cli_onepassword_secrets_u_op_version(const char *);
extern int hermes_cli_onepassword_secrets_u_op_whoami(const char *);
extern int hermes_cli_security_audit_star_u_is_root(const char *);
extern int hermes_cli_security_audit_star_u_running_as_root(const char *);
extern int hermes_cli_security_audit_star_u_iter_sshd_config_lines(const char *);
extern int hermes_cli_security_audit_star_u_ssh_password_auth_enabled(const char *);
extern int hermes_cli_security_audit_star_u_path_is_mounted(const char *);
extern int hermes_cli_security_audit_star_u_container_no_volume_mount(const char *);
extern int hermes_cli_security_audit_star_u_network_listener_without_auth(const char *);
extern int hermes_cli_security_audit_star_run_security_audit(const char *);
extern int hermes_cli_security_audit_star_log_startup_security_warnings(const char *);
extern int hermes_cli_dashboard_auth_nati_u_b64url_no_pad(const char *);
extern int hermes_cli_dashboard_auth_nati_u_s256(const char *);
extern int hermes_cli_dashboard_auth_nati_u_gc_locked(const char *);
extern int hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(const char *);
extern int hermes_cli_dashboard_auth_nati_register_pending(const char *);
extern int hermes_cli_dashboard_auth_nati_get_pending(const char *);
extern int hermes_cli_dashboard_auth_nati_complete_pending(const char *);
extern int hermes_cli_dashboard_auth_nati_redeem_code(const char *);
extern int hermes_cli_mcp_picker_is_custom(const char *);
extern int hermes_cli_mcp_picker_u_build_rows(const char *);
extern int hermes_cli_mcp_picker_u_format_row(const char *);
extern int hermes_cli_mcp_picker_u_enable_disable(const char *);
extern int hermes_cli_mcp_picker_u_configure_tools(const char *);
extern int hermes_cli_mcp_picker_u_remove_custom(const char *);
extern int hermes_cli_mcp_picker_u_handle_row(const char *);
extern int hermes_cli_mcp_picker_u_print_rows_text(const char *);
extern int hermes_cli_proxy_cli_register_cli(const char *);
extern int hermes_cli_proxy_cli_cmd_install(const char *);
extern int hermes_cli_proxy_cli_cmd_start(const char *);
extern int hermes_cli_proxy_cli_format_status_text(const char *);
extern int hermes_cli_proxy_cli_cmd_disable(const char *);
extern int hermes_cli_proxy_cli_u_load_env_file_into_environ(const char *);
extern int hermes_cli_proxy_cli_u_yn(const char *);
extern int hermes_cli_proxy_cli_u_redact_token(const char *);
extern int hermes_cli__subprocess_compat_windows_detach_flags(const char *);
extern int hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const char *);
extern int hermes_cli__subprocess_compat_windows_hide_flags(const char *);
extern int hermes_cli__subprocess_compat_suppress_platform_ver_console(const char *);
extern int hermes_cli__subprocess_compat_windows_detach_popen_kwargs(const char *);
extern int hermes_cli__subprocess_compat_u_kill_git_process_tree(const char *);
extern int hermes_cli__subprocess_compat_bounded_git_probe(const char *);
extern int hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(const char *);
extern int hermes_cli_container_boot_u_read_container_argv(const char *);
extern int hermes_cli_container_boot_u_is_legacy_gateway_run_request(const char *);
extern int hermes_cli_container_boot_u_read_desired_state(const char *);
extern int hermes_cli_container_boot_u_cleanup_stale_runtime_files(const char *);
extern int hermes_cli_container_boot_u_register_service(const char *);
extern int hermes_cli_container_boot_u_write_reconcile_log(const char *);
extern int hermes_cli_copilot_auth_validate_copilot_token(const char *);
extern int hermes_cli_copilot_auth_resolve_copilot_token(const char *);
extern int hermes_cli_copilot_auth_u_gh_cli_candidates(const char *);
extern int hermes_cli_copilot_auth_u_try_gh_cli_token(const char *);
extern int hermes_cli_copilot_auth_exchange_copilot_token(const char *);
extern int hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(const char *);
extern int hermes_cli_copilot_auth_copilot_request_headers(const char *);
extern int hermes_cli_cron_u_normalize_skills(const char *);
extern int hermes_cli_cron_u_cron_api(const char *);
extern int hermes_cli_cron_u_active_cron_provider_name(const char *);
extern int hermes_cli_cron_u_warn_if_gateway_not_running(const char *);
extern int hermes_cli_cron_cron_runs(const char *);
extern int hermes_cli_cron_u_print_active_jobs_summary(const char *);
extern int hermes_cli_cron_u_job_action(const char *);
extern int hermes_cli_dashboard_auth_base_start_login(const char *);
extern int hermes_cli_dashboard_auth_base_complete_login(const char *);
extern int hermes_cli_dashboard_auth_base_verify_session(const char *);
extern int hermes_cli_dashboard_auth_base_refresh_session(const char *);
extern int hermes_cli_dashboard_auth_base_revoke_session(const char *);
extern int hermes_cli_dashboard_auth_base_complete_password_login(const char *);
extern int hermes_cli_dashboard_auth_base_assert_protocol_compliance(const char *);
extern int hermes_cli_nous_auth_keepalive_u_timeout_seconds(const char *);
extern int hermes_cli_nous_auth_keepalive_u_entry_state(const char *);
extern int hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(const char *);
extern int hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(const char *);
extern int hermes_cli_nous_auth_keepalive_u_keepalive_loop(const char *);
extern int hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(const char *);
extern int hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(const char *);
extern int hermes_cli_nous_billing_invalidate_cached_token(const char *);
extern int hermes_cli_nous_billing_u_request(const char *);
extern int hermes_cli_nous_billing_get_subscription_state(const char *);
extern int hermes_cli_nous_billing_post_subscription_preview(const char *);
extern int hermes_cli_nous_billing_put_subscription_pending_change(const char *);
extern int hermes_cli_nous_billing_delete_subscription_pending_change(const char *);
extern int hermes_cli_nous_billing_post_subscription_upgrade(const char *);
extern int hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(const char *);
extern int hermes_cli_setup_whatsapp_clou_u_validate_waba_id(const char *);
extern int hermes_cli_setup_whatsapp_clou_u_validate_app_id(const char *);
extern int hermes_cli_setup_whatsapp_clou_u_validate_app_secret(const char *);
extern int hermes_cli_setup_whatsapp_clou_u_validate_access_token(const char *);
extern int hermes_cli_setup_whatsapp_clou_u_prompt_validated(const char *);
extern int hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(const char *);
extern int hermes_cli__early_recovery_u_project_root(const char *);
extern int hermes_cli__early_recovery_u_pinned_specs(const char *);
extern int hermes_cli__early_recovery_u_certifi_bundle_broken(const char *);
extern int hermes_cli__early_recovery_u_probe_broken_packages(const char *);
extern int hermes_cli__early_recovery_u_run_repair_install(const char *);
extern int hermes_cli__early_recovery_recover_if_needed(const char *);
extern int hermes_cli_credential_lifecycl_u_providers_for_env_var(const char *);
extern int hermes_cli_credential_lifecycl_u_prune_env_pool_entries(const char *);
extern int hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors(const char *);
extern int hermes_cli_credential_lifecycl_purge_env_credential_references(const char *);
extern int hermes_cli_credential_lifecycl_save_provider_env_credential(const char *);
extern int hermes_cli_credential_lifecycl_remove_provider_env_credential(const char *);
extern int hermes_cli_dashboard_auth_toke_register_token_route(const char *);
extern int hermes_cli_dashboard_auth_toke_is_token_route(const char *);
extern int hermes_cli_dashboard_auth_toke_clear_token_routes(const char *);
extern int hermes_cli_dashboard_auth_toke_extract_bearer_token(const char *);
extern int hermes_cli_dashboard_auth_toke_authenticate_token(const char *);
extern int hermes_cli_dashboard_auth_toke_token_auth_middleware(const char *);
extern int hermes_cli_fallback_cmd_u_read_chain(const char *);
extern int hermes_cli_fallback_cmd_u_write_chain(const char *);
extern int hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(const char *);
extern int hermes_cli_fallback_cmd_u_restore_auth_active_provider(const char *);
extern int hermes_cli_fallback_cmd_u_restore_model_cfg(const char *);
extern int hermes_cli_fallback_cmd_u_numbered_pick(const char *);
extern int hermes_cli_managed_scope_get_managed_dir(const char *);
extern int hermes_cli_managed_scope_invalidate_managed_cache(const char *);
extern int hermes_cli_managed_scope_u_cached_read(const char *);
extern int hermes_cli_managed_scope_load_managed_config(const char *);
extern int hermes_cli_managed_scope_load_managed_env(const char *);
extern int hermes_cli_managed_scope_apply_managed_overlay(const char *);
extern int hermes_cli_oneshot_u_normalize_toolsets(const char *);
extern int hermes_cli_oneshot_u_validate_explicit_toolsets(const char *);
extern int hermes_cli_oneshot_u_write_usage_file(const char *);
extern int hermes_cli_oneshot_run_oneshot(const char *);
extern int hermes_cli_oneshot_u_create_session_db_for_oneshot(const char *);
extern int hermes_cli_oneshot_u_oneshot_clarify_callback(const char *);
extern int hermes_cli_providers_is_routing_aggregator(const char *);
extern int hermes_cli_providers_host_mandated_api_mode(const char *);
extern int hermes_cli_providers_determine_api_mode(const char *);
extern int hermes_cli_providers_resolve_user_provider(const char *);
extern int hermes_cli_providers_custom_provider_slug(const char *);
extern int hermes_cli_providers_resolve_custom_provider(const char *);
extern int hermes_cli_secrets_cli_register_cli(const char *);
extern int hermes_cli_secrets_cli_cmd_token(const char *);
extern int hermes_cli_secrets_cli_u_yn(const char *);
extern int hermes_cli_secrets_cli_u_bws_version(const char *);
extern int hermes_cli_secrets_cli_u_token_validation_status(const char *);
extern int hermes_cli_secrets_cli_u_resolve_server_url(const char *);
extern int hermes_cli_skin_cmd_u_skins_dir(const char *);
extern int hermes_cli_skin_cmd_u_active_skin(const char *);
extern int hermes_cli_skin_cmd_u_use(const char *);
extern int hermes_cli_skin_cmd_u_skin_set(const char *);
extern int hermes_cli_skin_cmd_u_skin_list(const char *);
extern int hermes_cli_skin_cmd_skin_command(const char *);
extern int hermes_cli_azure_detect_u_resolve_credential(const char *);
extern int hermes_cli_azure_detect_u_apply_auth_headers(const char *);
extern int hermes_cli_azure_detect_u_http_get_json(const char *);
extern int hermes_cli_azure_detect_u_probe_openai_models(const char *);
extern int hermes_cli_azure_detect_u_probe_anthropic_messages(const char *);
extern int hermes_cli_codex_models_u_add_forward_compat_models(const char *);
extern int hermes_cli_codex_models_u_extract_chatgpt_account_id(const char *);
extern int hermes_cli_codex_models_u_fetch_models_from_api(const char *);
extern int hermes_cli_codex_models_u_read_default_model(const char *);
extern int hermes_cli_codex_models_u_read_cache_models(const char *);
extern int hermes_cli_dashboard_auth_cook_set_session_provider_cookie(const char *);
extern int hermes_cli_dashboard_auth_cook_read_session_cookies(const char *);
extern int hermes_cli_dashboard_auth_cook_read_session_provider(const char *);
extern int hermes_cli_dashboard_auth_cook_read_pkce_cookie(const char *);
extern int hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(const char *);
extern int hermes_cli_dingtalk_auth_u_api_post(const char *);
extern int hermes_cli_dingtalk_auth_wait_for_registration_success(const char *);
extern int hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(const char *);
extern int hermes_cli_dingtalk_auth_render_qr_to_terminal(const char *);
extern int hermes_cli_dingtalk_auth_dingtalk_qr_auth(const char *);
extern int hermes_cli_gateway_enroll_u_default_gateway_id(const char *);
extern int hermes_cli_gateway_enroll_u_resolve_connector_url(const char *);
extern int hermes_cli_gateway_enroll_u_resolve_identity_token(const char *);
extern int hermes_cli_gateway_enroll_u_post_enroll(const char *);
extern int hermes_cli_gateway_enroll_cmd_gateway_enroll(const char *);
extern int hermes_cli_mcp_startup_u_has_configured_mcp_servers(const char *);
extern int hermes_cli_mcp_startup_u_resolve_discovery_timeout(const char *);
extern int hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(const char *);
extern int hermes_cli_mcp_startup_mcp_discovery_in_flight(const char *);
extern int hermes_cli_mcp_startup_join_mcp_discovery(const char *);
extern int hermes_cli_moa_config_u_default_reference_models(const char *);
extern int hermes_cli_moa_config_u_coerce_reference_timeout(const char *);
extern int hermes_cli_moa_config_u_coerce_degraded_reference_policy(const char *);
extern int hermes_cli_moa_config_coerce_privacy_filter(const char *);
extern int hermes_cli_moa_config_moa_usage(const char *);
extern int hermes_cli_proxy_cli_u_print_aiohttp_missing(const char *);
extern int hermes_cli_proxy_cli_cmd_proxy_start(const char *);
extern int hermes_cli_proxy_cli_cmd_proxy_status(const char *);
extern int hermes_cli_proxy_cli_cmd_proxy_list_providers(const char *);
extern int hermes_cli_proxy_cli_cmd_proxy(const char *);
extern int hermes_cli_session_filters_parse_duration_seconds(const char *);
extern int hermes_cli_session_filters_parse_point_in_time(const char *);
extern int hermes_cli_session_filters_format_epoch(const char *);
extern int hermes_cli_session_filters_build_prune_filters(const char *);
extern int hermes_cli_session_filters_describe_filters(const char *);
extern int hermes_cli_urllib_security_url_origin(const char *);
extern int hermes_cli_urllib_security_redirect_request(const char *);
extern int hermes_cli_urllib_security_u_sanitize(const char *);
extern int hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(const char *);
extern int hermes_cli_urllib_security_open_credentialed_url(const char *);
extern int hermes_cli_bundles_u_cmd_show(const char *);
extern int hermes_cli_bundles_u_cmd_create(const char *);
extern int hermes_cli_bundles_u_cmd_delete(const char *);
extern int hermes_cli_bundles_register_cli(const char *);
extern int hermes_cli_dashboard_register_u_generate_dashboard_name(const char *);
extern int hermes_cli_dashboard_register_u_register_self_hosted_client(const char *);
extern int hermes_cli_dashboard_register_u_print_post_register_hint(const char *);
extern int hermes_cli_dashboard_register_cmd_dashboard_register(const char *);
extern int hermes_cli_memory_oauth_u_resolve_flow(const char *);
extern int hermes_cli_memory_oauth_u_scope_to_profile(const char *);
extern int hermes_cli_memory_oauth_start_memory_oauth(const char *);
extern int hermes_cli_memory_oauth_memory_oauth_status(const char *);
extern int hermes_cli_moa_cmd_u_pick_slot(const char *);
extern int hermes_cli_moa_cmd_u_format_slot(const char *);
extern int hermes_cli_moa_cmd_u_print_config(const char *);
extern int hermes_cli_moa_cmd_cmd_moa(const char *);
extern int hermes_cli_proxy_server_u_filter_request_headers(const char *);
extern int hermes_cli_proxy_server_u_filter_response_headers(const char *);
extern int hermes_cli_proxy_server_create_app(const char *);
extern int hermes_cli_proxy_server_run_server(const char *);
extern int hermes_cli_secret_prompt_u_collect_masked_input(const char *);
extern int hermes_cli_secret_prompt_u_stream_is_tty(const char *);
extern int hermes_cli_secret_prompt_u_masked_secret_prompt_windows(const char *);
extern int hermes_cli_secret_prompt_u_masked_secret_prompt_posix(const char *);
extern int hermes_cli_send_cmd_u_read_message_body(const char *);
extern int hermes_cli_send_cmd_u_emit_result(const char *);
extern int hermes_cli_send_cmd_u_list_targets(const char *);
extern int hermes_cli_send_cmd_u_load_hermes_env(const char *);
extern int hermes_cli_session_export_html_u_escape_html(const char *);
extern int hermes_cli_session_export_html_u_generate_messages_html(const char *);
extern int hermes_cli_session_export_html_generate_multi_session_html_e_rt(const char *);
extern int hermes_cli_session_export_html_generate_html_export(const char *);
extern int hermes_cli_sqlite_runtime_u_version_tuple(const char *);
extern int hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(const char *);
extern int hermes_cli_sqlite_runtime_wal_reset_vulnerable(const char *);
extern int hermes_cli_sqlite_runtime_probe_sqlite_runtime(const char *);
extern int hermes_cli_stdio_u_flip_console_code_page_to_utf8(const char *);
extern int hermes_cli_stdio_u_reconfigure_stream(const char *);
extern int hermes_cli_stdio_u_default_windows_editor(const char *);
extern int hermes_cli_stdio_u_augment_path_with_known_tools(const char *);
extern int hermes_cli_dep_ensure_u_has_system_browser(const char *);
extern int hermes_cli_dep_ensure_u_has_hermes_agent_browser(const char *);
extern int hermes_cli_dep_ensure_u_find_install_script(const char *);
extern int hermes_cli_diagnostics_upload_request_upload_url(const char *);
extern int hermes_cli_diagnostics_upload_put_bundle(const char *);
extern int hermes_cli_diagnostics_upload_share_to_nous(const char *);
extern int hermes_cli_goals_draft_contract(const char *);
extern int hermes_cli_goals_evaluate_after_turn(const char *);
extern int hermes_cli_goals_run_kanban_goal_loop(const char *);
extern int hermes_cli_profiles_u_profile_bound_backend_pids(const char *);
extern int hermes_cli_profiles_u_stop_profile_backends(const char *);
extern int hermes_cli_profiles_u_rmtree_with_retry(const char *);
extern int hermes_cli_relaunch_u_build_inherited_flag_table(const char *);
extern int hermes_cli_relaunch_u_extract_inherited_flags(const char *);
extern int hermes_cli_relaunch_resolve_hermes_bin(const char *);
extern int hermes_cli_suggestions_cmd_u_fmt_pending(const char *);
extern int hermes_cli_suggestions_cmd_u_resolve_origin(const char *);
extern int hermes_cli_suggestions_cmd_handle_suggestions_command(const char *);
extern int hermes_cli_checkpoints_u_confirm(const char *);
extern int hermes_cli_checkpoints_cmd_clear_legacy(const char *);
extern int hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(const char *);
extern int hermes_cli_cli_agent_setup_mix_u_display_resumed_history(const char *);
extern int hermes_cli_dashboard_auth_logi_render_login_html(const char *);
extern int hermes_cli_dashboard_auth_logi_u_render_password_form(const char *);
extern int hermes_cli_pairing_pairing_command(const char *);
extern int hermes_cli_pairing_u_cmd_clear_pending(const char *);
extern int hermes_cli_partial_compress_extract_compress_flags(const char *);
extern int hermes_cli_partial_compress_summarize_compress_preview(const char *);
extern int hermes_cli_portal_cli_u_cmd_open(const char *);
extern int hermes_cli_portal_cli_u_cmd_login(const char *);
extern int hermes_cli_provider_catalog_provider_catalog(const char *);
extern int hermes_cli_provider_catalog_provider_catalog_by_slug(const char *);
extern int hermes_cli_psutil_android_u_normalize_member_parts(const char *);
extern int hermes_cli_psutil_android_u_safe_extract_tar_gz(const char *);
extern int hermes_cli_slack_cli_u_build_full_manifest(const char *);
extern int hermes_cli_slack_cli_slack_manifest_command(const char *);
extern int hermes_cli_subcommands_dashboa_u_add_server_runtime_args(const char *);
extern int hermes_cli_subcommands_dashboa_build_dashboard_parser(const char *);
extern int hermes_cli_subcommands_gateway_u_add_compat_platform_flag(const char *);
extern int hermes_cli_subcommands_gateway_build_gateway_parser(const char *);
extern int hermes_cli__parser_u_inherited_flag(const char *);
extern int hermes_cli_banner_u_skin_color(const char *);
extern int hermes_cli_codex_runtime_switc_check_codex_binary_ok(const char *);
extern int hermes_cli_config_custom_endpoint_key_env(const char *);
extern int hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(const char *);
extern int hermes_cli_fallback_config_resolve_entry_api_key(const char *);
extern int hermes_cli_kanban_specify_list_triage_ids(const char *);
extern int hermes_cli_memory_setup_u_env_line_safe(const char *);
extern int hermes_cli_profile_describer_u_collect_skills(const char *);
extern int hermes_cli_route_identity_should_clear_context_pin(const char *);
extern int hermes_cli_session_listing_query_session_listing(const char *);
extern int hermes_cli_session_recap_u_iter_assistant_tool_calls(const char *);
extern int hermes_cli_skills_config_u_normalize_skill_names(const char *);
extern int hermes_cli_subcommands__shared_add_accept_hooks_flag(const char *);
extern int hermes_cli_subcommands_acp_build_acp_parser(const char *);
extern int hermes_cli_subcommands_auth_build_auth_parser(const char *);
extern int hermes_cli_subcommands_backup_build_backup_parser(const char *);
extern int hermes_cli_subcommands_claw_build_claw_parser(const char *);
extern int hermes_cli_subcommands_config_build_config_parser(const char *);
extern int hermes_cli_subcommands_console_build_console_parser(const char *);
extern int hermes_cli_subcommands_cron_build_cron_parser(const char *);
extern int hermes_cli_subcommands_debug_build_debug_parser(const char *);
extern int hermes_cli_subcommands_doctor_build_doctor_parser(const char *);
extern int hermes_cli_subcommands_dump_build_dump_parser(const char *);
extern int hermes_cli_subcommands_gui_build_gui_parser(const char *);
extern int hermes_cli_subcommands_hooks_build_hooks_parser(const char *);
extern int hermes_cli_subcommands_import__build_import_cmd_parser(const char *);
extern int hermes_cli_subcommands_insight_build_insights_parser(const char *);
extern int hermes_cli_subcommands_login_build_login_parser(const char *);
extern int hermes_cli_subcommands_logout_build_logout_parser(const char *);
extern int hermes_cli_subcommands_logs_build_logs_parser(const char *);
extern int hermes_cli_subcommands_mcp_build_mcp_parser(const char *);
extern int hermes_cli_subcommands_memory_build_memory_parser(const char *);
extern int hermes_cli_subcommands_model_build_model_parser(const char *);
extern int hermes_cli_subcommands_pairing_build_pairing_parser(const char *);
extern int hermes_cli_subcommands_plugins_build_plugins_parser(const char *);
extern int hermes_cli_subcommands_profile_build_profile_parser(const char *);
extern int hermes_cli_subcommands_prompt__build_prompt_size_parser(const char *);
extern int hermes_cli_subcommands_securit_build_security_parser(const char *);
extern int hermes_cli_subcommands_setup_build_setup_parser(const char *);
extern int hermes_cli_subcommands_skills_build_skills_parser(const char *);
extern int hermes_cli_subcommands_skin_build_skin_parser(const char *);
extern int hermes_cli_subcommands_slack_build_slack_parser(const char *);
extern int hermes_cli_subcommands_status_build_status_parser(const char *);
extern int hermes_cli_subcommands_tools_build_tools_parser(const char *);
extern int hermes_cli_subcommands_uninsta_build_uninstall_parser(const char *);
extern int hermes_cli_subcommands_update_build_update_parser(const char *);
extern int hermes_cli_subcommands_version_build_version_parser(const char *);
extern int hermes_cli_subcommands_webhook_build_webhook_parser(const char *);
extern int hermes_cli_subcommands_whatsap_build_whatsapp_parser(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_u_redirect_uri(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_u_redirect_uri(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_u_redirect_uri"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_u_prefix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_u_prefix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_u_prefix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_login_page(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_login_page(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_login_page"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_api_auth_providers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_api_auth_providers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_api_auth_providers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_native_authorize(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_native_authorize(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_native_authorize"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_u_validate_post_login_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_u_validate_post_login_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_u_validate_post_login_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_u_password_rate_limited(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_u_password_rate_limited(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_u_password_rate_limited"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_password_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_password_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_password_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_logout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_logout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_logout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_api_auth_me(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_api_auth_me(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_api_auth_me"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_api_auth_ws_ticket"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_native_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_native_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_native_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_rout_auth_native_refresh(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_rout_auth_native_refresh(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_rout_auth_native_refresh"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_pending_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_pending_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_pending_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_best_effort_sweep_expired_pastes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_best_effort_sweep_expired_pastes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_best_effort_sweep_expired_pastes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_delete_paste(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_delete_paste(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_delete_paste"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_schedule_auto_delete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_schedule_auto_delete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_schedule_auto_delete"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_upload_paste_rs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_upload_paste_rs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_upload_paste_rs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_upload_dpaste_com(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_upload_dpaste_com(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_upload_dpaste_com"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_upload_to_pastebin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_upload_to_pastebin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_upload_to_pastebin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_primary_log_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_primary_log_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_primary_log_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_resolve_log_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_resolve_log_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_resolve_log_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_capture_log_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_capture_log_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_capture_log_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_capture_default_log_snapshots(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_capture_default_log_snapshots(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_capture_default_log_snapshots"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_capture_dump(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_capture_dump(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_capture_dump"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_collect_share_bundle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_collect_share_bundle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_collect_share_bundle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_build_nous_bundle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_build_nous_bundle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_build_nous_bundle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_confirm_upload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_confirm_upload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_confirm_upload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_u_run_debug_share_nous(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_u_run_debug_share_nous(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_u_run_debug_share_nous"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_debug_run_debug(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_debug_run_debug(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_debug_run_debug"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_confirm(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_confirm(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_confirm"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_get_mcp_servers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_get_mcp_servers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_get_mcp_servers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_save_mcp_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_save_mcp_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_save_mcp_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_remove_mcp_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_remove_mcp_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_remove_mcp_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_replace_mcp_servers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_replace_mcp_servers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_replace_mcp_servers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_env_key_for_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_env_key_for_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_env_key_for_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_strip_bearer_prefix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_strip_bearer_prefix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_strip_bearer_prefix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_bearer_auth_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_bearer_auth_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_bearer_auth_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_save_bearer_auth_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_save_bearer_auth_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_save_bearer_auth_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_parse_env_assignments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_parse_env_assignments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_parse_env_assignments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_apply_mcp_preset(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_apply_mcp_preset(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_apply_mcp_preset"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_resolve_mcp_server_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_resolve_mcp_server_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_resolve_mcp_server_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_probe_single_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_probe_single_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_probe_single_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_oauth_tokens_present(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_oauth_tokens_present(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_oauth_tokens_present"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_unwrap_exception_group(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_unwrap_exception_group(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_unwrap_exception_group"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_u_reauth_oauth_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_u_reauth_oauth_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_u_reauth_oauth_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_config_cmd_mcp_reauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_config_cmd_mcp_reauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_config_cmd_mcp_reauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_print_usage_cta(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_print_usage_cta(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_print_usage_cta"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_show_subscription(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_show_subscription(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_show_subscription"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_overview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_overview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_overview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_open_url_in_browser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_open_url_in_browser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_open_url_in_browser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_free_catalog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_free_catalog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_free_catalog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_open_portal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_open_portal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_open_portal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_change_menu(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_change_menu(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_change_menu"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_pick_tier(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_pick_tier(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_pick_tier"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_apply(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_apply(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_apply"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_render_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_render_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_render_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_usage_bar_lines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_usage_bar_lines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_usage_bar_lines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_billing_mixin_u_billing_add_card_flow(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_billing_mixin_u_billing_add_card_flow(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_billing_mixin_u_billing_add_card_flow"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_cmd_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_cmd_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_cmd_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_cmd_remove(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_cmd_remove(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_cmd_remove"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_cmd_select(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_cmd_select(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_cmd_select"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_cmd_off(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_cmd_off(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_cmd_off"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_cmd_scale(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_cmd_scale(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_cmd_scale"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_cmd_show(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_cmd_show(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_cmd_show"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_pet_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_pet_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_pet_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_has_active_pet(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_has_active_pet(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_has_active_pet"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_set_active(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_set_active(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_set_active"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_set_pet_scale(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_set_pet_scale(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_set_pet_scale"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_toggle_pet_display(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_toggle_pet_display(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_toggle_pet_display"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_print_pet_gallery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_print_pet_gallery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_print_pet_gallery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_clear_active_if(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_clear_active_if(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_clear_active_if"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_rename_active_if(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_rename_active_if(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_rename_active_if"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_u_interactive_pick(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_u_interactive_pick(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_u_interactive_pick"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pets_register_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pets_register_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pets_register_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_radio_item_plain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_radio_item_plain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_radio_item_plain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_curses_style_attr(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_curses_style_attr(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_curses_style_attr"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_draw_description_line(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_draw_description_line(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_draw_description_line"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_draw_radio_item(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_draw_radio_item(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_draw_radio_item"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_move_filtered_cursor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_move_filtered_cursor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_move_filtered_cursor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_scroll_for_cursor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_scroll_for_cursor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_scroll_for_cursor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_handle_active_search_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_handle_active_search_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_handle_active_search_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_flush_stdin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_flush_stdin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_flush_stdin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_read_menu_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_read_menu_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_read_menu_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_decode_menu_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_decode_menu_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_decode_menu_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_run_curses_menu(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_run_curses_menu(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_run_curses_menu"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_format_radio_item_ansi(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_format_radio_item_ansi(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_format_radio_item_ansi"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_radio_numbered_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_radio_numbered_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_radio_numbered_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_numbered_single_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_numbered_single_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_numbered_single_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curses_ui_u_numbered_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curses_ui_u_numbered_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curses_ui_u_numbered_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_catalog_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_catalog_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_catalog_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_parse_env_spec(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_parse_env_spec(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_parse_env_spec"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_parse_manifest(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_parse_manifest(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_parse_manifest"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_catalog_diagnostics(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_catalog_diagnostics(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_catalog_diagnostics"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_get_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_get_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_get_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_install_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_install_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_install_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_run_bootstrap(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_run_bootstrap(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_run_bootstrap"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_do_git_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_do_git_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_do_git_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_expand_install_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_expand_install_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_expand_install_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_prompt_env_vars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_prompt_env_vars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_prompt_env_vars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_build_server_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_build_server_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_build_server_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_read_prior_tool_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_read_prior_tool_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_read_prior_tool_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_probe_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_probe_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_probe_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_write_tools_include(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_write_tools_include(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_write_tools_include"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_catalog_u_apply_tool_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_catalog_u_apply_tool_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_catalog_u_apply_tool_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_build_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_build_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_build_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_projects_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_projects_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_projects_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_with_project(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_with_project(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_with_project"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_print_project(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_print_project(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_print_project"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_create(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_create(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_create"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_show(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_show(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_show"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_add_folder(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_add_folder(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_add_folder"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_remove_folder(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_remove_folder(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_remove_folder"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_rename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_rename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_rename"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_set_primary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_set_primary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_set_primary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_use(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_use(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_use"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_archive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_archive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_archive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_restore(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_restore(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_restore"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_cmd_bind_board(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_cmd_bind_board(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_cmd_bind_board"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_cmd_u_sync_board_default_workdir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_cmd_u_sync_board_default_workdir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_cmd_u_sync_board_default_workdir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_get_custom_provider_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_get_custom_provider_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_get_custom_provider_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_resolve_custom_provider_input(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_resolve_custom_provider_input(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_resolve_custom_provider_input"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_provider_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_provider_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_provider_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_oauth_default_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_oauth_default_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_oauth_default_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_api_key_default_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_api_key_default_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_api_key_default_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_display_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_display_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_display_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_classify_exhausted_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_classify_exhausted_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_classify_exhausted_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_format_exhausted_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_format_exhausted_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_format_exhausted_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_interactive_auth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_interactive_auth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_interactive_auth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_pick_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_pick_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_pick_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_interactive_add(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_interactive_add(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_interactive_add"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_interactive_remove(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_interactive_remove(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_interactive_remove"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_interactive_reset(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_interactive_reset(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_interactive_reset"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_auth_commands_u_interactive_strategy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_auth_commands_u_interactive_strategy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_auth_commands_u_interactive_strategy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_owned_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_owned_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_owned_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_load_yaml(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_load_yaml(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_load_yaml"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_dump_yaml(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_dump_yaml(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_dump_yaml"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_parse_semver(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_parse_semver(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_parse_semver"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_check_hermes_requires(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_check_hermes_requires(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_check_hermes_requires"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_env_template_from_manifest(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_env_template_from_manifest(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_env_template_from_manifest"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_looks_like_git_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_looks_like_git_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_looks_like_git_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_git_clone(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_git_clone(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_git_clone"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_stage_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_stage_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_stage_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_reject_distribution_symlinks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_reject_distribution_symlinks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_reject_distribution_symlinks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_has_cron_jobs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_has_cron_jobs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_has_cron_jobs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_count_skills(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_count_skills(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_count_skills"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_copy_dist_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_copy_dist_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_copy_dist_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_distributio_u_bootstrap_user_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_distributio_u_bootstrap_user_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_distributio_u_bootstrap_user_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_discover_venv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_discover_venv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_discover_venv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_parse_requirements(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_parse_requirements(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_parse_requirements"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_parse_pyproject_pins(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_parse_pyproject_pins(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_parse_pyproject_pins"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_discover_plugins(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_discover_plugins(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_discover_plugins"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_extract_mcp_component(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_extract_mcp_component(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_extract_mcp_component"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_discover_mcp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_discover_mcp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_discover_mcp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_http_post_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_http_post_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_http_post_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_http_get_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_http_get_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_http_get_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_osv_query_batch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_osv_query_batch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_osv_query_batch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_osv_fetch_details(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_osv_fetch_details(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_osv_fetch_details"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_render_human(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_render_human(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_render_human"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_render_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_render_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_render_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_u_count_components(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_u_count_components(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_u_count_components"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_cmd_security_audit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_cmd_security_audit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_cmd_security_audit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_u_api_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_u_api_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_u_api_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_u_parse_owner_user_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_u_parse_owner_user_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_u_parse_owner_user_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_render_qr_terminal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_render_qr_terminal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_render_qr_terminal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_print_qr_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_print_qr_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_print_qr_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_generate_username_slug(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_generate_username_slug(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_generate_username_slug"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_generate_bot_username(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_generate_bot_username(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_generate_bot_username"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_generate_deep_link(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_generate_deep_link(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_generate_deep_link"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_generate_pairing_nonce(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_generate_pairing_nonce(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_generate_pairing_nonce"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_create_pairing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_create_pairing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_create_pairing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_poll_pairing_result_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_poll_pairing_result_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_poll_pairing_result_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_poll_pairing_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_poll_pairing_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_poll_pairing_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_poll_for_setup_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_poll_for_setup_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_poll_for_setup_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_poll_for_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_poll_for_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_poll_for_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_u_collect_memory_provider_external_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_u_collect_memory_provider_external_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_u_collect_memory_provider_external_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_u_iter_external_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_u_iter_external_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_u_iter_external_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_verify_sqlite_integrity(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_verify_sqlite_integrity(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_verify_sqlite_integrity"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_copy_db_and_verify(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_copy_db_and_verify(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_copy_db_and_verify"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_run_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_run_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_run_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_run_import(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_run_import(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_run_import"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_create_quick_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_create_quick_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_create_quick_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_list_quick_snapshots(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_list_quick_snapshots(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_list_quick_snapshots"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_restore_quick_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_restore_quick_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_restore_quick_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_run_quick_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_run_quick_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_run_quick_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_u_write_full_zip_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_u_write_full_zip_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_u_write_full_zip_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_create_pre_update_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_create_pre_update_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_create_pre_update_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_backup_create_pre_migration_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_backup_create_pre_migration_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_backup_create_pre_migration_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_aux_slot_explicit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_aux_slot_explicit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_aux_slot_explicit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_main_model_visible(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_main_model_visible(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_main_model_visible"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_triage_aux_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_triage_aux_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_triage_aux_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_repeated_failures(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_repeated_failures(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_repeated_failures"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_repeated_crashes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_repeated_crashes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_repeated_crashes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_config_from_kanban_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_config_from_kanban_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_config_from_kanban_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_diagnostics_config_from_runtime_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_diagnostics_config_from_runtime_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_diagnostics_config_from_runtime_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_load_catalog_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_load_catalog_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_load_catalog_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_cache_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_cache_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_cache_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_fetch_manifest_with_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_fetch_manifest_with_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_fetch_manifest_with_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_validate_manifest(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_validate_manifest(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_validate_manifest"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_read_disk_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_read_disk_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_read_disk_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_write_disk_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_write_disk_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_write_disk_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_fetch_provider_override(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_fetch_provider_override(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_fetch_provider_override"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_get_provider_block(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_get_provider_block(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_get_provider_block"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_get_curated_openrouter_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_get_curated_openrouter_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_get_curated_openrouter_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_get_curated_nous_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_get_curated_nous_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_get_curated_nous_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_u_default_model_from_block(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_u_default_model_from_block(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_u_default_model_from_block"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_get_default_model_from_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_get_default_model_from_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_get_default_model_from_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_model_catalog_reset_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_model_catalog_reset_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_model_catalog_reset_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_display_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_display_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_display_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_resolve_short_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_resolve_short_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_resolve_short_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_format_extra_metadata_lines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_format_extra_metadata_lines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_format_extra_metadata_lines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_resolve_source_meta_and_bundle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_derive_category_from_install_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_derive_category_from_install_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_derive_category_from_install_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_is_valid_installed_skill_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_is_valid_installed_skill_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_is_valid_installed_skill_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_existing_categories(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_existing_categories(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_existing_categories"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_prompt_for_skill_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_prompt_for_skill_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_prompt_for_skill_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_prompt_for_category(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_prompt_for_category(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_prompt_for_category"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_do_list_modified(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_do_list_modified(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_do_list_modified"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_do_diff(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_do_diff(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_do_diff"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_github_publish(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_github_publish(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_github_publish"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_hub_u_print_skills_help(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_hub_u_print_skills_help(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_hub_u_print_skills_help"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_color(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_color(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_color"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_spinner_wings(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_spinner_wings(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_spinner_wings"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_branding(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_branding(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_branding"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_u_skins_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_u_skins_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_u_skins_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_u_load_skin_from_yaml(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_u_load_skin_from_yaml(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_u_load_skin_from_yaml"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_u_mapping_or_empty(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_u_mapping_or_empty(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_u_mapping_or_empty"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_u_build_skin_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_u_build_skin_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_u_build_skin_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_active_skin_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_active_skin_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_active_skin_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_init_skin_from_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_init_skin_from_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_init_skin_from_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_active_prompt_symbol(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_active_prompt_symbol(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_active_prompt_symbol"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_active_help_header(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_active_help_header(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_active_help_header"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_active_goodbye(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_active_goodbye(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_active_goodbye"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_engine_get_prompt_toolkit_style_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_detect_openclaw_processes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_detect_openclaw_processes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_detect_openclaw_processes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_warn_if_openclaw_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_warn_if_openclaw_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_warn_if_openclaw_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_warn_if_gateway_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_warn_if_gateway_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_warn_if_gateway_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_find_migration_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_find_migration_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_find_migration_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_load_migration_module(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_load_migration_module(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_load_migration_module"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_find_openclaw_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_find_openclaw_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_find_openclaw_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_scan_workspace_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_scan_workspace_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_scan_workspace_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_archive_directory(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_archive_directory(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_archive_directory"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_claw_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_claw_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_claw_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_cmd_migrate(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_cmd_migrate(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_cmd_migrate"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_cmd_cleanup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_cmd_cleanup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_cmd_cleanup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_claw_u_print_migration_report(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_claw_u_print_migration_report(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_claw_u_print_migration_report"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_get_secret_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_get_secret_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_get_secret_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_get_secret_source_values(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_get_secret_source_values(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_get_secret_source_values"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_reset_secret_source_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_reset_secret_source_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_reset_secret_source_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_format_secret_source_suffix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_format_secret_source_suffix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_format_secret_source_suffix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_format_offending_chars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_format_offending_chars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_format_offending_chars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_sanitize_loaded_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_sanitize_loaded_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_sanitize_loaded_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_load_dotenv_with_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_load_dotenv_with_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_load_dotenv_with_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_sanitize_env_file_if_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_sanitize_env_file_if_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_sanitize_env_file_if_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_apply_managed_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_apply_managed_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_apply_managed_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_apply_external_secret_sources(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_apply_external_secret_sources(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_apply_external_secret_sources"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_remediation_hint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_remediation_hint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_remediation_hint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_env_loader_u_load_secrets_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_env_loader_u_load_secrets_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_env_loader_u_load_secrets_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_log_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_log_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_log_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_log_success(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_log_success(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_log_success"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_log_warn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_log_warn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_log_warn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_u_agent_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_u_agent_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_u_agent_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_desktop_userdata_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_desktop_userdata_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_desktop_userdata_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_source_built_gui_artifacts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_source_built_gui_artifacts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_source_built_gui_artifacts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_packaged_gui_app_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_packaged_gui_app_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_packaged_gui_app_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_agent_is_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_agent_is_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_agent_is_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_gui_is_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_gui_is_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_gui_is_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_gui_install_summary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_gui_install_summary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_gui_install_summary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_u_remove_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_u_remove_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_u_remove_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gui_uninstall_uninstall_gui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gui_uninstall_uninstall_gui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gui_uninstall_uninstall_gui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_coerce_max_concurrent_sessions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_coerce_max_concurrent_sessions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_coerce_max_concurrent_sessions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_resolve_max_concurrent_sessions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_resolve_max_concurrent_sessions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_resolve_max_concurrent_sessions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_active_session_limit_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_active_session_limit_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_active_session_limit_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u__enter__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u__enter__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u__enter__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u__exit__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u__exit__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u__exit__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u_read_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u_read_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u_read_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u_write_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u_write_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u_write_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u_process_start_time(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u_process_start_time(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u_process_start_time"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u_optional_float(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u_optional_float(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u_optional_float"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_u_prune_dead(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_u_prune_dead(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_u_prune_dead"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_active_sessions_transfer_active_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_active_sessions_transfer_active_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_active_sessions_transfer_active_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_translate_one_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_translate_one_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_translate_one_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_format_toml_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_format_toml_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_format_toml_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_quote_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_quote_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_quote_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_render_codex_toml_section(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_render_codex_toml_section(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_render_codex_toml_section"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_looks_like_table_header(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_looks_like_table_header(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_looks_like_table_header"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_query_codex_plugins(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_query_codex_plugins(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_query_codex_plugins"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_with_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_with_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_with_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_build_models_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_build_models_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_build_models_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_build_model_options_payload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_build_model_options_payload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_build_model_options_payload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_apply_capabilities(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_apply_capabilities(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_apply_capabilities"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_append_unconfigured_rows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_append_unconfigured_rows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_append_unconfigured_rows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_filter_explicit_provider_rows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_filter_explicit_provider_rows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_filter_explicit_provider_rows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_raw_config_has_enabled_moa_preset"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_apply_picker_hints(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_apply_picker_hints(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_apply_picker_hints"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_reorder_canonical(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_reorder_canonical(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_reorder_canonical"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_apply_pricing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_apply_pricing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_apply_pricing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_inventory_u_moa_provider_row(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_inventory_u_moa_provider_row(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_inventory_u_moa_provider_row"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_primary_hex(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_primary_hex(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_primary_hex"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_fade(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_fade(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_fade"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_row_to_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_row_to_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_row_to_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_term_size(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_term_size(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_term_size"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_frame_renderable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_frame_renderable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_frame_renderable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_cmd_show(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_cmd_show(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_cmd_show"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_cmd_delete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_cmd_delete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_cmd_delete"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_cmd_edit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_cmd_edit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_cmd_edit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_u_open_in_editor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_u_open_in_editor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_u_open_in_editor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_register_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_register_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_register_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_journey_cmd_journey(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_journey_cmd_journey(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_journey_cmd_journey"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_u_safe_copy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_u_safe_copy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_u_safe_copy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_apply_llm_request_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_apply_llm_request_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_apply_llm_request_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_apply_tool_request_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_apply_tool_request_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_apply_tool_request_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_apply_api_request_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_apply_api_request_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_apply_api_request_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_run_llm_execution_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_run_llm_execution_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_run_llm_execution_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_run_tool_execution_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_run_tool_execution_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_run_tool_execution_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_run_api_execution_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_run_api_execution_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_run_api_execution_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_u_invoke_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_u_invoke_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_u_invoke_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_u_has_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_u_has_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_u_has_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_u_get_middleware_callbacks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_u_get_middleware_callbacks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_u_get_middleware_callbacks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_middleware_u_run_execution_chain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_middleware_u_run_execution_chain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_middleware_u_run_execution_chain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_s6_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_s6_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_s6_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_profile_dir_for_gateway_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_profile_dir_for_gateway_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_profile_dir_for_gateway_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_write_gateway_desired_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_write_gateway_desired_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_write_gateway_desired_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_seed_supervise_skeleton(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_seed_supervise_skeleton(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_seed_supervise_skeleton"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_service_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_service_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_service_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_service_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_service_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_service_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_render_run_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_render_run_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_render_run_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_render_finish_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_render_finish_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_render_finish_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_render_log_run(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_render_log_run(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_render_log_run"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_run_svc(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_run_svc(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_run_svc"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_service_manager_u_supervised_pid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_service_manager_u_supervised_pid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_service_manager_u_supervised_pid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_chrome_debug_data_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_chrome_debug_data_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_chrome_debug_data_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_u_chrome_debug_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_u_chrome_debug_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_u_chrome_debug_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_discover_local_cdp_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_discover_local_cdp_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_discover_local_cdp_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_local_port_in_use(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_local_port_in_use(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_local_port_in_use"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_find_free_debug_port(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_find_free_debug_port(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_find_free_debug_port"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_manual_chrome_debug_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_manual_chrome_debug_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_manual_chrome_debug_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_u_detach_kwargs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_u_detach_kwargs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_u_detach_kwargs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_u_read_stderr_tail(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_u_read_stderr_tail(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_u_read_stderr_tail"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_browser_connect_launch_chrome_debug(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_browser_connect_launch_chrome_debug(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_browser_connect_launch_chrome_debug"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_path_is_public(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_path_is_public(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_path_is_public"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_ordered_session_providers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_ordered_session_providers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_ordered_session_providers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_unauth_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_unauth_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_unauth_response"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_auto_sso_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_auto_sso_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_auto_sso_response"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_safe_next_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_safe_next_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_safe_next_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_extract_bearer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_extract_bearer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_extract_bearer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_verify_bearer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_verify_bearer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_verify_bearer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_gated_auth_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_gated_auth_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_gated_auth_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_expires_in_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_expires_in_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_expires_in_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_midd_u_attempt_refresh(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_midd_u_attempt_refresh(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_midd_u_attempt_refresh"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_dotenv_key_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_dotenv_key_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_dotenv_key_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_get_git_commit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_get_git_commit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_get_git_commit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_count_skills(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_count_skills(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_count_skills"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_count_mcp_servers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_count_mcp_servers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_count_mcp_servers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_cron_summary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_cron_summary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_cron_summary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_configured_platforms(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_configured_platforms(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_configured_platforms"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_memory_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_memory_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_memory_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_get_model_and_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_get_model_and_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_get_model_and_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_u_config_overrides(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_u_config_overrides(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_u_config_overrides"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dump_run_dump(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dump_run_dump(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dump_run_dump"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_projects_db_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_projects_db_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_projects_db_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_u_new_project_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_u_new_project_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_u_new_project_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_u_now(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_u_now(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_u_now"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_connect_closing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_connect_closing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_connect_closing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_u_migrate_add_optional_columns(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_u_migrate_add_optional_columns(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_u_migrate_add_optional_columns"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_u_project_from_row(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_u_project_from_row(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_u_project_from_row"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_u_attach_folders(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_u_attach_folders(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_u_attach_folders"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_get_discovery_policy_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_get_discovery_policy_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_get_discovery_policy_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_reconcile_discovered_repos_policy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_reconcile_discovered_repos_policy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_reconcile_discovered_repos_policy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_projects_db_clear_discovered_repos(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_projects_db_clear_discovered_repos(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_projects_db_clear_discovered_repos"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_append(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_append(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_append"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_truncated(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_truncated(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_truncated"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_u_drain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_u_drain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_u_drain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_detach(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_detach(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_detach"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_run_reaper(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_run_reaper(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_run_reaper"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_attach_or_spawn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_attach_or_spawn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_attach_or_spawn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_detach_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_detach_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_detach_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_reap_idle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_reap_idle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_reap_idle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_u_reap_one_idle_or_raise(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_u_reap_one_idle_or_raise(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_u_reap_one_idle_or_raise"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pty_session_close_all(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pty_session_close_all(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pty_session_close_all"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_subscriptions_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_subscriptions_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_subscriptions_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_load_subscriptions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_load_subscriptions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_load_subscriptions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_save_subscriptions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_save_subscriptions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_save_subscriptions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_get_webhook_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_get_webhook_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_get_webhook_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_is_webhook_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_is_webhook_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_is_webhook_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_get_webhook_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_get_webhook_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_get_webhook_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_setup_hint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_setup_hint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_setup_hint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_require_webhook_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_require_webhook_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_require_webhook_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_cmd_subscribe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_cmd_subscribe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_cmd_subscribe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_webhook_u_cmd_remove(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_webhook_u_cmd_remove(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_webhook_u_cmd_remove"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_run(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_run(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_run"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_pause(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_pause(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_pause"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_pin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_pin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_pin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_unpin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_unpin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_unpin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_restore(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_restore(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_restore"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_archive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_archive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_archive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_idle_days(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_idle_days(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_idle_days"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_prune(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_prune(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_prune"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_curator_u_cmd_list_archived(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_curator_u_cmd_list_archived(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_curator_u_cmd_list_archived"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_register_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_register_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_register_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_cmd_set(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_cmd_set(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_cmd_set"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_cmd_remove(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_cmd_remove(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_cmd_remove"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_cmd_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_cmd_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_cmd_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_cmd_sync(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_cmd_sync(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_cmd_sync"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_cmd_disable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_cmd_disable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_cmd_disable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_u_yn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_u_yn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_u_yn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_u_op_version(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_u_op_version(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_u_op_version"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_onepassword_secrets_u_op_whoami(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_onepassword_secrets_u_op_whoami(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_onepassword_secrets_u_op_whoami"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_is_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_is_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_is_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_running_as_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_running_as_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_running_as_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_iter_sshd_config_lines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_iter_sshd_config_lines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_iter_sshd_config_lines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_ssh_password_auth_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_ssh_password_auth_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_ssh_password_auth_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_path_is_mounted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_path_is_mounted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_path_is_mounted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_container_no_volume_mount(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_container_no_volume_mount(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_container_no_volume_mount"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_u_network_listener_without_auth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_u_network_listener_without_auth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_u_network_listener_without_auth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_run_security_audit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_run_security_audit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_run_security_audit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_security_audit_star_log_startup_security_warnings(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_security_audit_star_log_startup_security_warnings(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_security_audit_star_log_startup_security_warnings"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_u_b64url_no_pad(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_u_b64url_no_pad(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_u_b64url_no_pad"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_u_s256(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_u_s256(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_u_s256"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_u_gc_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_u_gc_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_u_gc_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_u_capacity_ok_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_register_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_register_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_register_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_get_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_get_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_get_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_complete_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_complete_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_complete_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_nati_redeem_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_nati_redeem_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_nati_redeem_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_is_custom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_is_custom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_is_custom"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_build_rows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_build_rows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_build_rows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_format_row(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_format_row(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_format_row"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_enable_disable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_enable_disable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_enable_disable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_configure_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_configure_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_configure_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_remove_custom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_remove_custom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_remove_custom"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_handle_row(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_handle_row(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_handle_row"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_picker_u_print_rows_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_picker_u_print_rows_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_picker_u_print_rows_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_register_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_register_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_register_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_format_status_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_format_status_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_format_status_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_disable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_disable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_disable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_u_load_env_file_into_environ(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_u_load_env_file_into_environ(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_u_load_env_file_into_environ"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_u_yn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_u_yn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_u_yn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_u_redact_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_u_redact_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_u_redact_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_windows_detach_flags(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_windows_detach_flags(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_windows_detach_flags"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_windows_hide_flags(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_windows_hide_flags(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_windows_hide_flags"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_suppress_platform_ver_console(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_suppress_platform_ver_console(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_suppress_platform_ver_console"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_windows_detach_popen_kwargs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_windows_detach_popen_kwargs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_windows_detach_popen_kwargs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_u_kill_git_process_tree(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_u_kill_git_process_tree(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_u_kill_git_process_tree"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__subprocess_compat_bounded_git_probe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__subprocess_compat_bounded_git_probe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__subprocess_compat_bounded_git_probe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_read_container_argv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_read_container_argv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_read_container_argv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_is_legacy_gateway_run_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_is_legacy_gateway_run_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_is_legacy_gateway_run_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_read_desired_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_read_desired_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_read_desired_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_cleanup_stale_runtime_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_cleanup_stale_runtime_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_cleanup_stale_runtime_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_register_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_register_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_register_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_container_boot_u_write_reconcile_log(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_container_boot_u_write_reconcile_log(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_container_boot_u_write_reconcile_log"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_validate_copilot_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_validate_copilot_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_validate_copilot_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_resolve_copilot_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_resolve_copilot_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_resolve_copilot_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_u_gh_cli_candidates(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_u_gh_cli_candidates(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_u_gh_cli_candidates"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_u_try_gh_cli_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_u_try_gh_cli_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_u_try_gh_cli_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_exchange_copilot_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_exchange_copilot_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_exchange_copilot_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_copilot_auth_copilot_request_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_copilot_auth_copilot_request_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_copilot_auth_copilot_request_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_u_normalize_skills(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_u_normalize_skills(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_u_normalize_skills"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_u_cron_api(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_u_cron_api(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_u_cron_api"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_u_active_cron_provider_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_u_active_cron_provider_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_u_active_cron_provider_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_u_warn_if_gateway_not_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_u_warn_if_gateway_not_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_u_warn_if_gateway_not_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_cron_runs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_cron_runs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_cron_runs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_u_print_active_jobs_summary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_u_print_active_jobs_summary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_u_print_active_jobs_summary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cron_u_job_action(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cron_u_job_action(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cron_u_job_action"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_start_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_start_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_start_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_complete_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_complete_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_complete_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_verify_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_verify_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_verify_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_refresh_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_refresh_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_refresh_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_revoke_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_revoke_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_revoke_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_complete_password_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_complete_password_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_complete_password_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_base_assert_protocol_compliance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_base_assert_protocol_compliance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_base_assert_protocol_compliance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_u_timeout_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_u_timeout_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_u_timeout_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_u_entry_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_u_entry_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_u_entry_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_u_keepalive_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_u_keepalive_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_u_keepalive_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_invalidate_cached_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_invalidate_cached_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_invalidate_cached_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_u_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_u_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_u_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_get_subscription_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_get_subscription_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_get_subscription_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_post_subscription_preview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_post_subscription_preview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_post_subscription_preview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_put_subscription_pending_change(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_put_subscription_pending_change(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_put_subscription_pending_change"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_delete_subscription_pending_change(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_delete_subscription_pending_change(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_delete_subscription_pending_change"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_nous_billing_post_subscription_upgrade(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_nous_billing_post_subscription_upgrade(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_nous_billing_post_subscription_upgrade"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_u_validate_waba_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_u_validate_waba_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_u_validate_waba_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_u_validate_app_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_u_validate_app_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_u_validate_app_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_u_validate_app_secret(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_u_validate_app_secret(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_u_validate_app_secret"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_u_validate_access_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_u_validate_access_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_u_validate_access_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_u_prompt_validated(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_u_prompt_validated(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_u_prompt_validated"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__early_recovery_u_project_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__early_recovery_u_project_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__early_recovery_u_project_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__early_recovery_u_pinned_specs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__early_recovery_u_pinned_specs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__early_recovery_u_pinned_specs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__early_recovery_u_certifi_bundle_broken(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__early_recovery_u_certifi_bundle_broken(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__early_recovery_u_certifi_bundle_broken"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__early_recovery_u_probe_broken_packages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__early_recovery_u_probe_broken_packages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__early_recovery_u_probe_broken_packages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__early_recovery_u_run_repair_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__early_recovery_u_run_repair_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__early_recovery_u_run_repair_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__early_recovery_recover_if_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__early_recovery_recover_if_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__early_recovery_recover_if_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_credential_lifecycl_u_providers_for_env_var(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_credential_lifecycl_u_providers_for_env_var(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_credential_lifecycl_u_providers_for_env_var"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_credential_lifecycl_u_prune_env_pool_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_credential_lifecycl_u_prune_env_pool_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_credential_lifecycl_u_prune_env_pool_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_credential_lifecycl_purge_env_credential_references(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_credential_lifecycl_purge_env_credential_references(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_credential_lifecycl_purge_env_credential_references"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_credential_lifecycl_save_provider_env_credential(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_credential_lifecycl_save_provider_env_credential(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_credential_lifecycl_save_provider_env_credential"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_credential_lifecycl_remove_provider_env_credential(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_credential_lifecycl_remove_provider_env_credential(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_credential_lifecycl_remove_provider_env_credential"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_toke_register_token_route(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_toke_register_token_route(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_toke_register_token_route"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_toke_is_token_route(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_toke_is_token_route(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_toke_is_token_route"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_toke_clear_token_routes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_toke_clear_token_routes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_toke_clear_token_routes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_toke_extract_bearer_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_toke_extract_bearer_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_toke_extract_bearer_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_toke_authenticate_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_toke_authenticate_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_toke_authenticate_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_toke_token_auth_middleware(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_toke_token_auth_middleware(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_toke_token_auth_middleware"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_cmd_u_read_chain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_cmd_u_read_chain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_cmd_u_read_chain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_cmd_u_write_chain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_cmd_u_write_chain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_cmd_u_write_chain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_cmd_u_snapshot_auth_active_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_cmd_u_restore_auth_active_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_cmd_u_restore_auth_active_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_cmd_u_restore_auth_active_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_cmd_u_restore_model_cfg(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_cmd_u_restore_model_cfg(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_cmd_u_restore_model_cfg"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_cmd_u_numbered_pick(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_cmd_u_numbered_pick(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_cmd_u_numbered_pick"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_managed_scope_get_managed_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_managed_scope_get_managed_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_managed_scope_get_managed_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_managed_scope_invalidate_managed_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_managed_scope_invalidate_managed_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_managed_scope_invalidate_managed_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_managed_scope_u_cached_read(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_managed_scope_u_cached_read(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_managed_scope_u_cached_read"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_managed_scope_load_managed_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_managed_scope_load_managed_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_managed_scope_load_managed_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_managed_scope_load_managed_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_managed_scope_load_managed_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_managed_scope_load_managed_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_managed_scope_apply_managed_overlay(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_managed_scope_apply_managed_overlay(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_managed_scope_apply_managed_overlay"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_oneshot_u_normalize_toolsets(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_oneshot_u_normalize_toolsets(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_oneshot_u_normalize_toolsets"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_oneshot_u_validate_explicit_toolsets(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_oneshot_u_validate_explicit_toolsets(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_oneshot_u_validate_explicit_toolsets"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_oneshot_u_write_usage_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_oneshot_u_write_usage_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_oneshot_u_write_usage_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_oneshot_run_oneshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_oneshot_run_oneshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_oneshot_run_oneshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_oneshot_u_create_session_db_for_oneshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_oneshot_u_create_session_db_for_oneshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_oneshot_u_create_session_db_for_oneshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_oneshot_u_oneshot_clarify_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_oneshot_u_oneshot_clarify_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_oneshot_u_oneshot_clarify_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_providers_is_routing_aggregator(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_providers_is_routing_aggregator(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_providers_is_routing_aggregator"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_providers_host_mandated_api_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_providers_host_mandated_api_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_providers_host_mandated_api_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_providers_determine_api_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_providers_determine_api_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_providers_determine_api_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_providers_resolve_user_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_providers_resolve_user_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_providers_resolve_user_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_providers_custom_provider_slug(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_providers_custom_provider_slug(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_providers_custom_provider_slug"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_providers_resolve_custom_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_providers_resolve_custom_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_providers_resolve_custom_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secrets_cli_register_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secrets_cli_register_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secrets_cli_register_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secrets_cli_cmd_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secrets_cli_cmd_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secrets_cli_cmd_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secrets_cli_u_yn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secrets_cli_u_yn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secrets_cli_u_yn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secrets_cli_u_bws_version(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secrets_cli_u_bws_version(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secrets_cli_u_bws_version"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secrets_cli_u_token_validation_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secrets_cli_u_token_validation_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secrets_cli_u_token_validation_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secrets_cli_u_resolve_server_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secrets_cli_u_resolve_server_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secrets_cli_u_resolve_server_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_cmd_u_skins_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_cmd_u_skins_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_cmd_u_skins_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_cmd_u_active_skin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_cmd_u_active_skin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_cmd_u_active_skin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_cmd_u_use(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_cmd_u_use(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_cmd_u_use"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_cmd_u_skin_set(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_cmd_u_skin_set(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_cmd_u_skin_set"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_cmd_u_skin_list(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_cmd_u_skin_list(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_cmd_u_skin_list"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skin_cmd_skin_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skin_cmd_skin_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skin_cmd_skin_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_azure_detect_u_resolve_credential(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_azure_detect_u_resolve_credential(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_azure_detect_u_resolve_credential"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_azure_detect_u_apply_auth_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_azure_detect_u_apply_auth_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_azure_detect_u_apply_auth_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_azure_detect_u_http_get_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_azure_detect_u_http_get_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_azure_detect_u_http_get_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_azure_detect_u_probe_openai_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_azure_detect_u_probe_openai_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_azure_detect_u_probe_openai_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_azure_detect_u_probe_anthropic_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_azure_detect_u_probe_anthropic_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_azure_detect_u_probe_anthropic_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_models_u_add_forward_compat_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_models_u_add_forward_compat_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_models_u_add_forward_compat_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_models_u_extract_chatgpt_account_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_models_u_extract_chatgpt_account_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_models_u_extract_chatgpt_account_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_models_u_fetch_models_from_api(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_models_u_fetch_models_from_api(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_models_u_fetch_models_from_api"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_models_u_read_default_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_models_u_read_default_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_models_u_read_default_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_models_u_read_cache_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_models_u_read_cache_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_models_u_read_cache_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_cook_set_session_provider_cookie(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_cook_set_session_provider_cookie(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_cook_set_session_provider_cookie"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_cook_read_session_cookies(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_cook_read_session_cookies(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_cook_read_session_cookies"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_cook_read_session_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_cook_read_session_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_cook_read_session_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_cook_read_pkce_cookie(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_cook_read_pkce_cookie(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_cook_read_pkce_cookie"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dingtalk_auth_u_api_post(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dingtalk_auth_u_api_post(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dingtalk_auth_u_api_post"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dingtalk_auth_wait_for_registration_success(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dingtalk_auth_wait_for_registration_success(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dingtalk_auth_wait_for_registration_success"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dingtalk_auth_u_ensure_qrcode_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dingtalk_auth_render_qr_to_terminal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dingtalk_auth_render_qr_to_terminal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dingtalk_auth_render_qr_to_terminal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dingtalk_auth_dingtalk_qr_auth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dingtalk_auth_dingtalk_qr_auth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dingtalk_auth_dingtalk_qr_auth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gateway_enroll_u_default_gateway_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gateway_enroll_u_default_gateway_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gateway_enroll_u_default_gateway_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gateway_enroll_u_resolve_connector_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gateway_enroll_u_resolve_connector_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gateway_enroll_u_resolve_connector_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gateway_enroll_u_resolve_identity_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gateway_enroll_u_resolve_identity_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gateway_enroll_u_resolve_identity_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gateway_enroll_u_post_enroll(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gateway_enroll_u_post_enroll(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gateway_enroll_u_post_enroll"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_gateway_enroll_cmd_gateway_enroll(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_gateway_enroll_cmd_gateway_enroll(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_gateway_enroll_cmd_gateway_enroll"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_startup_u_has_configured_mcp_servers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_startup_u_has_configured_mcp_servers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_startup_u_has_configured_mcp_servers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_startup_u_resolve_discovery_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_startup_u_resolve_discovery_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_startup_u_resolve_discovery_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_startup_mcp_discovery_in_flight(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_startup_mcp_discovery_in_flight(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_startup_mcp_discovery_in_flight"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_mcp_startup_join_mcp_discovery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_mcp_startup_join_mcp_discovery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_mcp_startup_join_mcp_discovery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_config_u_default_reference_models(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_config_u_default_reference_models(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_config_u_default_reference_models"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_config_u_coerce_reference_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_config_u_coerce_reference_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_config_u_coerce_reference_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_config_u_coerce_degraded_reference_policy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_config_u_coerce_degraded_reference_policy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_config_u_coerce_degraded_reference_policy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_config_coerce_privacy_filter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_config_coerce_privacy_filter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_config_coerce_privacy_filter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_config_moa_usage(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_config_moa_usage(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_config_moa_usage"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_u_print_aiohttp_missing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_u_print_aiohttp_missing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_u_print_aiohttp_missing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_proxy_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_proxy_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_proxy_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_proxy_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_proxy_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_proxy_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_proxy_list_providers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_proxy_list_providers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_proxy_list_providers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_cli_cmd_proxy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_cli_cmd_proxy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_cli_cmd_proxy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_filters_parse_duration_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_filters_parse_duration_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_filters_parse_duration_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_filters_parse_point_in_time(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_filters_parse_point_in_time(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_filters_parse_point_in_time"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_filters_format_epoch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_filters_format_epoch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_filters_format_epoch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_filters_build_prune_filters(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_filters_build_prune_filters(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_filters_build_prune_filters"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_filters_describe_filters(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_filters_describe_filters(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_filters_describe_filters"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_urllib_security_url_origin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_urllib_security_url_origin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_urllib_security_url_origin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_urllib_security_redirect_request(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_urllib_security_redirect_request(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_urllib_security_redirect_request"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_urllib_security_u_sanitize(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_urllib_security_u_sanitize(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_urllib_security_u_sanitize"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_urllib_security_open_credentialed_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_urllib_security_open_credentialed_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_urllib_security_open_credentialed_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_bundles_u_cmd_show(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_bundles_u_cmd_show(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_bundles_u_cmd_show"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_bundles_u_cmd_create(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_bundles_u_cmd_create(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_bundles_u_cmd_create"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_bundles_u_cmd_delete(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_bundles_u_cmd_delete(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_bundles_u_cmd_delete"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_bundles_register_cli(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_bundles_register_cli(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_bundles_register_cli"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_register_u_generate_dashboard_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_register_u_generate_dashboard_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_register_u_generate_dashboard_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_register_u_register_self_hosted_client(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_register_u_register_self_hosted_client(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_register_u_register_self_hosted_client"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_register_u_print_post_register_hint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_register_u_print_post_register_hint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_register_u_print_post_register_hint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_register_cmd_dashboard_register(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_register_cmd_dashboard_register(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_register_cmd_dashboard_register"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_memory_oauth_u_resolve_flow(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_memory_oauth_u_resolve_flow(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_memory_oauth_u_resolve_flow"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_memory_oauth_u_scope_to_profile(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_memory_oauth_u_scope_to_profile(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_memory_oauth_u_scope_to_profile"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_memory_oauth_start_memory_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_memory_oauth_start_memory_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_memory_oauth_start_memory_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_memory_oauth_memory_oauth_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_memory_oauth_memory_oauth_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_memory_oauth_memory_oauth_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_cmd_u_pick_slot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_cmd_u_pick_slot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_cmd_u_pick_slot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_cmd_u_format_slot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_cmd_u_format_slot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_cmd_u_format_slot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_cmd_u_print_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_cmd_u_print_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_cmd_u_print_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_moa_cmd_cmd_moa(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_moa_cmd_cmd_moa(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_moa_cmd_cmd_moa"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_server_u_filter_request_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_server_u_filter_request_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_server_u_filter_request_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_server_u_filter_response_headers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_server_u_filter_response_headers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_server_u_filter_response_headers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_server_create_app(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_server_create_app(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_server_create_app"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_proxy_server_run_server(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_proxy_server_run_server(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_proxy_server_run_server"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secret_prompt_u_collect_masked_input(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secret_prompt_u_collect_masked_input(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secret_prompt_u_collect_masked_input"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secret_prompt_u_stream_is_tty(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secret_prompt_u_stream_is_tty(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secret_prompt_u_stream_is_tty"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secret_prompt_u_masked_secret_prompt_windows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secret_prompt_u_masked_secret_prompt_windows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secret_prompt_u_masked_secret_prompt_windows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_secret_prompt_u_masked_secret_prompt_posix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_secret_prompt_u_masked_secret_prompt_posix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_secret_prompt_u_masked_secret_prompt_posix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_send_cmd_u_read_message_body(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_send_cmd_u_read_message_body(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_send_cmd_u_read_message_body"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_send_cmd_u_emit_result(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_send_cmd_u_emit_result(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_send_cmd_u_emit_result"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_send_cmd_u_list_targets(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_send_cmd_u_list_targets(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_send_cmd_u_list_targets"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_send_cmd_u_load_hermes_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_send_cmd_u_load_hermes_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_send_cmd_u_load_hermes_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_export_html_u_escape_html(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_export_html_u_escape_html(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_export_html_u_escape_html"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_export_html_u_generate_messages_html(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_export_html_u_generate_messages_html(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_export_html_u_generate_messages_html"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_export_html_generate_multi_session_html_e_rt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_export_html_generate_multi_session_html_e_rt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_export_html_generate_multi_session_html_e_rt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_export_html_generate_html_export(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_export_html_generate_html_export(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_export_html_generate_html_export"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_sqlite_runtime_u_version_tuple(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_sqlite_runtime_u_version_tuple(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_sqlite_runtime_u_version_tuple"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_sqlite_runtime_wal_reset_vulnerable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_sqlite_runtime_wal_reset_vulnerable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_sqlite_runtime_wal_reset_vulnerable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_sqlite_runtime_probe_sqlite_runtime(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_sqlite_runtime_probe_sqlite_runtime(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_sqlite_runtime_probe_sqlite_runtime"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_stdio_u_flip_console_code_page_to_utf8(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_stdio_u_flip_console_code_page_to_utf8(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_stdio_u_flip_console_code_page_to_utf8"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_stdio_u_reconfigure_stream(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_stdio_u_reconfigure_stream(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_stdio_u_reconfigure_stream"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_stdio_u_default_windows_editor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_stdio_u_default_windows_editor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_stdio_u_default_windows_editor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_stdio_u_augment_path_with_known_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_stdio_u_augment_path_with_known_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_stdio_u_augment_path_with_known_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dep_ensure_u_has_system_browser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dep_ensure_u_has_system_browser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dep_ensure_u_has_system_browser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dep_ensure_u_has_hermes_agent_browser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dep_ensure_u_has_hermes_agent_browser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dep_ensure_u_has_hermes_agent_browser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dep_ensure_u_find_install_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dep_ensure_u_find_install_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dep_ensure_u_find_install_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_diagnostics_upload_request_upload_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_diagnostics_upload_request_upload_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_diagnostics_upload_request_upload_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_diagnostics_upload_put_bundle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_diagnostics_upload_put_bundle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_diagnostics_upload_put_bundle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_diagnostics_upload_share_to_nous(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_diagnostics_upload_share_to_nous(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_diagnostics_upload_share_to_nous"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_goals_draft_contract(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_goals_draft_contract(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_goals_draft_contract"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_goals_evaluate_after_turn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_goals_evaluate_after_turn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_goals_evaluate_after_turn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_goals_run_kanban_goal_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_goals_run_kanban_goal_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_goals_run_kanban_goal_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profiles_u_profile_bound_backend_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profiles_u_profile_bound_backend_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profiles_u_profile_bound_backend_pids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profiles_u_stop_profile_backends(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profiles_u_stop_profile_backends(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profiles_u_stop_profile_backends"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profiles_u_rmtree_with_retry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profiles_u_rmtree_with_retry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profiles_u_rmtree_with_retry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_relaunch_u_build_inherited_flag_table(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_relaunch_u_build_inherited_flag_table(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_relaunch_u_build_inherited_flag_table"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_relaunch_u_extract_inherited_flags(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_relaunch_u_extract_inherited_flags(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_relaunch_u_extract_inherited_flags"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_relaunch_resolve_hermes_bin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_relaunch_resolve_hermes_bin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_relaunch_resolve_hermes_bin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_suggestions_cmd_u_fmt_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_suggestions_cmd_u_fmt_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_suggestions_cmd_u_fmt_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_suggestions_cmd_u_resolve_origin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_suggestions_cmd_u_resolve_origin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_suggestions_cmd_u_resolve_origin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_suggestions_cmd_handle_suggestions_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_suggestions_cmd_handle_suggestions_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_suggestions_cmd_handle_suggestions_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_checkpoints_u_confirm(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_checkpoints_u_confirm(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_checkpoints_u_confirm"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_checkpoints_cmd_clear_legacy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_checkpoints_cmd_clear_legacy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_checkpoints_cmd_clear_legacy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_agent_setup_mix_u_preload_resumed_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_cli_agent_setup_mix_u_display_resumed_history(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_cli_agent_setup_mix_u_display_resumed_history(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_cli_agent_setup_mix_u_display_resumed_history"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_logi_render_login_html(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_logi_render_login_html(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_logi_render_login_html"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_logi_u_render_password_form(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_logi_u_render_password_form(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_logi_u_render_password_form"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pairing_pairing_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pairing_pairing_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pairing_pairing_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_pairing_u_cmd_clear_pending(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_pairing_u_cmd_clear_pending(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_pairing_u_cmd_clear_pending"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_partial_compress_extract_compress_flags(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_partial_compress_extract_compress_flags(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_partial_compress_extract_compress_flags"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_partial_compress_summarize_compress_preview(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_partial_compress_summarize_compress_preview(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_partial_compress_summarize_compress_preview"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_portal_cli_u_cmd_open(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_portal_cli_u_cmd_open(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_portal_cli_u_cmd_open"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_portal_cli_u_cmd_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_portal_cli_u_cmd_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_portal_cli_u_cmd_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_provider_catalog_provider_catalog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_provider_catalog_provider_catalog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_provider_catalog_provider_catalog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_provider_catalog_provider_catalog_by_slug(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_provider_catalog_provider_catalog_by_slug(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_provider_catalog_provider_catalog_by_slug"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_psutil_android_u_normalize_member_parts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_psutil_android_u_normalize_member_parts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_psutil_android_u_normalize_member_parts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_psutil_android_u_safe_extract_tar_gz(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_psutil_android_u_safe_extract_tar_gz(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_psutil_android_u_safe_extract_tar_gz"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_slack_cli_u_build_full_manifest(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_slack_cli_u_build_full_manifest(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_slack_cli_u_build_full_manifest"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_slack_cli_slack_manifest_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_slack_cli_slack_manifest_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_slack_cli_slack_manifest_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_dashboa_u_add_server_runtime_args(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_dashboa_u_add_server_runtime_args(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_dashboa_u_add_server_runtime_args"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_dashboa_build_dashboard_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_dashboa_build_dashboard_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_dashboa_build_dashboard_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_gateway_u_add_compat_platform_flag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_gateway_u_add_compat_platform_flag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_gateway_u_add_compat_platform_flag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_gateway_build_gateway_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_gateway_build_gateway_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_gateway_build_gateway_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli__parser_u_inherited_flag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli__parser_u_inherited_flag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli__parser_u_inherited_flag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_banner_u_skin_color(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_banner_u_skin_color(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_banner_u_skin_color"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_codex_runtime_switc_check_codex_binary_ok(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_codex_runtime_switc_check_codex_binary_ok(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_codex_runtime_switc_check_codex_binary_ok"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_config_custom_endpoint_key_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_config_custom_endpoint_key_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_config_custom_endpoint_key_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_fallback_config_resolve_entry_api_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_fallback_config_resolve_entry_api_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_fallback_config_resolve_entry_api_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_kanban_specify_list_triage_ids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_kanban_specify_list_triage_ids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_kanban_specify_list_triage_ids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_memory_setup_u_env_line_safe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_memory_setup_u_env_line_safe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_memory_setup_u_env_line_safe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_profile_describer_u_collect_skills(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_profile_describer_u_collect_skills(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_profile_describer_u_collect_skills"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_route_identity_should_clear_context_pin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_route_identity_should_clear_context_pin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_route_identity_should_clear_context_pin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_listing_query_session_listing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_listing_query_session_listing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_listing_query_session_listing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_session_recap_u_iter_assistant_tool_calls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_session_recap_u_iter_assistant_tool_calls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_session_recap_u_iter_assistant_tool_calls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_skills_config_u_normalize_skill_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_skills_config_u_normalize_skill_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_skills_config_u_normalize_skill_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands__shared_add_accept_hooks_flag(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands__shared_add_accept_hooks_flag(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands__shared_add_accept_hooks_flag"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_acp_build_acp_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_acp_build_acp_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_acp_build_acp_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_auth_build_auth_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_auth_build_auth_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_auth_build_auth_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_backup_build_backup_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_backup_build_backup_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_backup_build_backup_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_claw_build_claw_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_claw_build_claw_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_claw_build_claw_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_config_build_config_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_config_build_config_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_config_build_config_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_console_build_console_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_console_build_console_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_console_build_console_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_cron_build_cron_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_cron_build_cron_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_cron_build_cron_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_debug_build_debug_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_debug_build_debug_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_debug_build_debug_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_doctor_build_doctor_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_doctor_build_doctor_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_doctor_build_doctor_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_dump_build_dump_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_dump_build_dump_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_dump_build_dump_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_gui_build_gui_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_gui_build_gui_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_gui_build_gui_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_hooks_build_hooks_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_hooks_build_hooks_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_hooks_build_hooks_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_import__build_import_cmd_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_import__build_import_cmd_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_import__build_import_cmd_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_insight_build_insights_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_insight_build_insights_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_insight_build_insights_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_login_build_login_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_login_build_login_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_login_build_login_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_logout_build_logout_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_logout_build_logout_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_logout_build_logout_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_logs_build_logs_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_logs_build_logs_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_logs_build_logs_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_mcp_build_mcp_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_mcp_build_mcp_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_mcp_build_mcp_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_memory_build_memory_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_memory_build_memory_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_memory_build_memory_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_model_build_model_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_model_build_model_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_model_build_model_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_pairing_build_pairing_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_pairing_build_pairing_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_pairing_build_pairing_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_plugins_build_plugins_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_plugins_build_plugins_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_plugins_build_plugins_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_profile_build_profile_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_profile_build_profile_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_profile_build_profile_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_prompt__build_prompt_size_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_prompt__build_prompt_size_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_prompt__build_prompt_size_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_securit_build_security_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_securit_build_security_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_securit_build_security_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_setup_build_setup_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_setup_build_setup_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_setup_build_setup_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_skills_build_skills_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_skills_build_skills_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_skills_build_skills_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_skin_build_skin_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_skin_build_skin_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_skin_build_skin_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_slack_build_slack_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_slack_build_slack_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_slack_build_slack_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_status_build_status_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_status_build_status_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_status_build_status_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_tools_build_tools_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_tools_build_tools_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_tools_build_tools_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_uninsta_build_uninstall_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_uninsta_build_uninstall_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_uninsta_build_uninstall_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_update_build_update_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_update_build_update_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_update_build_update_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_version_build_version_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_version_build_version_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_version_build_version_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_webhook_build_webhook_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_webhook_build_webhook_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_webhook_build_webhook_parser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_hermes_cli_subcommands_whatsap_build_whatsapp_parser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)hermes_cli_subcommands_whatsap_build_whatsapp_parser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("hermes_cli_subcommands_whatsap_build_whatsapp_parser"));
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
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_u_redirect_uri") == 0) o = emit_hermes_cli_dashboard_auth_rout_u_redirect_uri(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_u_prefix") == 0) o = emit_hermes_cli_dashboard_auth_rout_u_prefix(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_login_page") == 0) o = emit_hermes_cli_dashboard_auth_rout_login_page(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_api_auth_providers") == 0) o = emit_hermes_cli_dashboard_auth_rout_api_auth_providers(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_login") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_login(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri") == 0) o = emit_hermes_cli_dashboard_auth_rout_u_validate_loopback_redirect_uri(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_native_authorize") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_native_authorize(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_callback") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_callback(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_u_validate_post_login_target") == 0) o = emit_hermes_cli_dashboard_auth_rout_u_validate_post_login_target(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_u_password_rate_limited") == 0) o = emit_hermes_cli_dashboard_auth_rout_u_password_rate_limited(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit") == 0) o = emit_hermes_cli_dashboard_auth_rout_u_reset_password_rate_limit(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_password_login") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_password_login(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_logout") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_logout(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_api_auth_me") == 0) o = emit_hermes_cli_dashboard_auth_rout_api_auth_me(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_api_auth_ws_ticket") == 0) o = emit_hermes_cli_dashboard_auth_rout_api_auth_ws_ticket(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_native_token") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_native_token(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_rout_auth_native_refresh") == 0) o = emit_hermes_cli_dashboard_auth_rout_auth_native_refresh(c);
        if (strcmp(op, "hermes_cli_debug_u_pending_file") == 0) o = emit_hermes_cli_debug_u_pending_file(c);
        if (strcmp(op, "hermes_cli_debug_u_best_effort_sweep_expired_pastes") == 0) o = emit_hermes_cli_debug_u_best_effort_sweep_expired_pastes(c);
        if (strcmp(op, "hermes_cli_debug_delete_paste") == 0) o = emit_hermes_cli_debug_delete_paste(c);
        if (strcmp(op, "hermes_cli_debug_u_schedule_auto_delete") == 0) o = emit_hermes_cli_debug_u_schedule_auto_delete(c);
        if (strcmp(op, "hermes_cli_debug_u_upload_paste_rs") == 0) o = emit_hermes_cli_debug_u_upload_paste_rs(c);
        if (strcmp(op, "hermes_cli_debug_u_upload_dpaste_com") == 0) o = emit_hermes_cli_debug_u_upload_dpaste_com(c);
        if (strcmp(op, "hermes_cli_debug_upload_to_pastebin") == 0) o = emit_hermes_cli_debug_upload_to_pastebin(c);
        if (strcmp(op, "hermes_cli_debug_u_primary_log_path") == 0) o = emit_hermes_cli_debug_u_primary_log_path(c);
        if (strcmp(op, "hermes_cli_debug_u_resolve_log_path") == 0) o = emit_hermes_cli_debug_u_resolve_log_path(c);
        if (strcmp(op, "hermes_cli_debug_u_capture_log_snapshot") == 0) o = emit_hermes_cli_debug_u_capture_log_snapshot(c);
        if (strcmp(op, "hermes_cli_debug_u_capture_default_log_snapshots") == 0) o = emit_hermes_cli_debug_u_capture_default_log_snapshots(c);
        if (strcmp(op, "hermes_cli_debug_u_capture_dump") == 0) o = emit_hermes_cli_debug_u_capture_dump(c);
        if (strcmp(op, "hermes_cli_debug_collect_share_bundle") == 0) o = emit_hermes_cli_debug_collect_share_bundle(c);
        if (strcmp(op, "hermes_cli_debug_build_nous_bundle") == 0) o = emit_hermes_cli_debug_build_nous_bundle(c);
        if (strcmp(op, "hermes_cli_debug_u_confirm_upload") == 0) o = emit_hermes_cli_debug_u_confirm_upload(c);
        if (strcmp(op, "hermes_cli_debug_u_run_debug_share_nous") == 0) o = emit_hermes_cli_debug_u_run_debug_share_nous(c);
        if (strcmp(op, "hermes_cli_debug_run_debug") == 0) o = emit_hermes_cli_debug_run_debug(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_confirm") == 0) o = emit_hermes_cli_mcp_config_u_confirm(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_get_mcp_servers") == 0) o = emit_hermes_cli_mcp_config_u_get_mcp_servers(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_save_mcp_server") == 0) o = emit_hermes_cli_mcp_config_u_save_mcp_server(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_remove_mcp_server") == 0) o = emit_hermes_cli_mcp_config_u_remove_mcp_server(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_replace_mcp_servers") == 0) o = emit_hermes_cli_mcp_config_u_replace_mcp_servers(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_env_key_for_server") == 0) o = emit_hermes_cli_mcp_config_u_env_key_for_server(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_strip_bearer_prefix") == 0) o = emit_hermes_cli_mcp_config_u_strip_bearer_prefix(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_bearer_auth_headers") == 0) o = emit_hermes_cli_mcp_config_u_bearer_auth_headers(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_save_bearer_auth_token") == 0) o = emit_hermes_cli_mcp_config_u_save_bearer_auth_token(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_parse_env_assignments") == 0) o = emit_hermes_cli_mcp_config_u_parse_env_assignments(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_apply_mcp_preset") == 0) o = emit_hermes_cli_mcp_config_u_apply_mcp_preset(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_resolve_mcp_server_config") == 0) o = emit_hermes_cli_mcp_config_u_resolve_mcp_server_config(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_probe_single_server") == 0) o = emit_hermes_cli_mcp_config_u_probe_single_server(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_oauth_tokens_present") == 0) o = emit_hermes_cli_mcp_config_u_oauth_tokens_present(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_unwrap_exception_group") == 0) o = emit_hermes_cli_mcp_config_u_unwrap_exception_group(c);
        if (strcmp(op, "hermes_cli_mcp_config_u_reauth_oauth_server") == 0) o = emit_hermes_cli_mcp_config_u_reauth_oauth_server(c);
        if (strcmp(op, "hermes_cli_mcp_config_cmd_mcp_reauth") == 0) o = emit_hermes_cli_mcp_config_cmd_mcp_reauth(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_print_usage_cta") == 0) o = emit_hermes_cli_cli_billing_mixin_u_print_usage_cta(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_show_subscription") == 0) o = emit_hermes_cli_cli_billing_mixin_u_show_subscription(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_overview") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_overview(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_open_url_in_browser") == 0) o = emit_hermes_cli_cli_billing_mixin_u_open_url_in_browser(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_free_catalog") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_free_catalog(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_open_portal") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_open_portal(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_change_menu") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_change_menu(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_pick_tier") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_pick_tier(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_preview_and_confirm(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_confirm_cancel(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_apply") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_apply(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_handle_scope_req_ed(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_render_error") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_render_error(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us") == 0) o = emit_hermes_cli_cli_billing_mixin_u_subscription_render_upgrade_a_us(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_usage_bar_lines") == 0) o = emit_hermes_cli_cli_billing_mixin_u_usage_bar_lines(c);
        if (strcmp(op, "hermes_cli_cli_billing_mixin_u_billing_add_card_flow") == 0) o = emit_hermes_cli_cli_billing_mixin_u_billing_add_card_flow(c);
        if (strcmp(op, "hermes_cli_pets_u_cmd_install") == 0) o = emit_hermes_cli_pets_u_cmd_install(c);
        if (strcmp(op, "hermes_cli_pets_u_cmd_remove") == 0) o = emit_hermes_cli_pets_u_cmd_remove(c);
        if (strcmp(op, "hermes_cli_pets_u_cmd_select") == 0) o = emit_hermes_cli_pets_u_cmd_select(c);
        if (strcmp(op, "hermes_cli_pets_u_cmd_off") == 0) o = emit_hermes_cli_pets_u_cmd_off(c);
        if (strcmp(op, "hermes_cli_pets_u_cmd_scale") == 0) o = emit_hermes_cli_pets_u_cmd_scale(c);
        if (strcmp(op, "hermes_cli_pets_u_cmd_show") == 0) o = emit_hermes_cli_pets_u_cmd_show(c);
        if (strcmp(op, "hermes_cli_pets_u_pet_config") == 0) o = emit_hermes_cli_pets_u_pet_config(c);
        if (strcmp(op, "hermes_cli_pets_u_has_active_pet") == 0) o = emit_hermes_cli_pets_u_has_active_pet(c);
        if (strcmp(op, "hermes_cli_pets_u_set_active") == 0) o = emit_hermes_cli_pets_u_set_active(c);
        if (strcmp(op, "hermes_cli_pets_set_pet_scale") == 0) o = emit_hermes_cli_pets_set_pet_scale(c);
        if (strcmp(op, "hermes_cli_pets_toggle_pet_display") == 0) o = emit_hermes_cli_pets_toggle_pet_display(c);
        if (strcmp(op, "hermes_cli_pets_print_pet_gallery") == 0) o = emit_hermes_cli_pets_print_pet_gallery(c);
        if (strcmp(op, "hermes_cli_pets_u_clear_active_if") == 0) o = emit_hermes_cli_pets_u_clear_active_if(c);
        if (strcmp(op, "hermes_cli_pets_u_rename_active_if") == 0) o = emit_hermes_cli_pets_u_rename_active_if(c);
        if (strcmp(op, "hermes_cli_pets_u_interactive_pick") == 0) o = emit_hermes_cli_pets_u_interactive_pick(c);
        if (strcmp(op, "hermes_cli_pets_register_cli") == 0) o = emit_hermes_cli_pets_register_cli(c);
        if (strcmp(op, "hermes_cli_curses_ui_radio_item_plain") == 0) o = emit_hermes_cli_curses_ui_radio_item_plain(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_curses_style_attr") == 0) o = emit_hermes_cli_curses_ui_u_curses_style_attr(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_draw_description_line") == 0) o = emit_hermes_cli_curses_ui_u_draw_description_line(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_draw_radio_item") == 0) o = emit_hermes_cli_curses_ui_u_draw_radio_item(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_move_filtered_cursor") == 0) o = emit_hermes_cli_curses_ui_u_move_filtered_cursor(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_scroll_for_cursor") == 0) o = emit_hermes_cli_curses_ui_u_scroll_for_cursor(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_handle_active_search_key") == 0) o = emit_hermes_cli_curses_ui_u_handle_active_search_key(c);
        if (strcmp(op, "hermes_cli_curses_ui_flush_stdin") == 0) o = emit_hermes_cli_curses_ui_flush_stdin(c);
        if (strcmp(op, "hermes_cli_curses_ui_read_menu_key") == 0) o = emit_hermes_cli_curses_ui_read_menu_key(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_decode_menu_key") == 0) o = emit_hermes_cli_curses_ui_u_decode_menu_key(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_run_curses_menu") == 0) o = emit_hermes_cli_curses_ui_u_run_curses_menu(c);
        if (strcmp(op, "hermes_cli_curses_ui_format_radio_item_ansi") == 0) o = emit_hermes_cli_curses_ui_format_radio_item_ansi(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_radio_numbered_fallback") == 0) o = emit_hermes_cli_curses_ui_u_radio_numbered_fallback(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_numbered_single_fallback") == 0) o = emit_hermes_cli_curses_ui_u_numbered_single_fallback(c);
        if (strcmp(op, "hermes_cli_curses_ui_u_numbered_fallback") == 0) o = emit_hermes_cli_curses_ui_u_numbered_fallback(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_catalog_root") == 0) o = emit_hermes_cli_mcp_catalog_u_catalog_root(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_parse_env_spec") == 0) o = emit_hermes_cli_mcp_catalog_u_parse_env_spec(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_parse_manifest") == 0) o = emit_hermes_cli_mcp_catalog_u_parse_manifest(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_catalog_diagnostics") == 0) o = emit_hermes_cli_mcp_catalog_catalog_diagnostics(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_get_entry") == 0) o = emit_hermes_cli_mcp_catalog_get_entry(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_install_root") == 0) o = emit_hermes_cli_mcp_catalog_u_install_root(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_run_bootstrap") == 0) o = emit_hermes_cli_mcp_catalog_u_run_bootstrap(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_do_git_install") == 0) o = emit_hermes_cli_mcp_catalog_u_do_git_install(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_expand_install_dir") == 0) o = emit_hermes_cli_mcp_catalog_u_expand_install_dir(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_prompt_env_vars") == 0) o = emit_hermes_cli_mcp_catalog_u_prompt_env_vars(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_build_server_config") == 0) o = emit_hermes_cli_mcp_catalog_u_build_server_config(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_read_prior_tool_selection") == 0) o = emit_hermes_cli_mcp_catalog_u_read_prior_tool_selection(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_probe_tools") == 0) o = emit_hermes_cli_mcp_catalog_u_probe_tools(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_write_tools_include") == 0) o = emit_hermes_cli_mcp_catalog_u_write_tools_include(c);
        if (strcmp(op, "hermes_cli_mcp_catalog_u_apply_tool_selection") == 0) o = emit_hermes_cli_mcp_catalog_u_apply_tool_selection(c);
        if (strcmp(op, "hermes_cli_projects_cmd_build_parser") == 0) o = emit_hermes_cli_projects_cmd_build_parser(c);
        if (strcmp(op, "hermes_cli_projects_cmd_projects_command") == 0) o = emit_hermes_cli_projects_cmd_projects_command(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_with_project") == 0) o = emit_hermes_cli_projects_cmd_u_with_project(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_print_project") == 0) o = emit_hermes_cli_projects_cmd_u_print_project(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_create") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_create(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_show") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_show(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_add_folder") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_add_folder(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_remove_folder") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_remove_folder(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_rename") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_rename(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_set_primary") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_set_primary(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_use") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_use(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_archive") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_archive(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_restore") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_restore(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_cmd_bind_board") == 0) o = emit_hermes_cli_projects_cmd_u_cmd_bind_board(c);
        if (strcmp(op, "hermes_cli_projects_cmd_u_sync_board_default_workdir") == 0) o = emit_hermes_cli_projects_cmd_u_sync_board_default_workdir(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_get_custom_provider_names") == 0) o = emit_hermes_cli_auth_commands_u_get_custom_provider_names(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_resolve_custom_provider_input") == 0) o = emit_hermes_cli_auth_commands_u_resolve_custom_provider_input(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_provider_base_url") == 0) o = emit_hermes_cli_auth_commands_u_provider_base_url(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_oauth_default_label") == 0) o = emit_hermes_cli_auth_commands_u_oauth_default_label(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_api_key_default_label") == 0) o = emit_hermes_cli_auth_commands_u_api_key_default_label(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_display_source") == 0) o = emit_hermes_cli_auth_commands_u_display_source(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_classify_exhausted_status") == 0) o = emit_hermes_cli_auth_commands_u_classify_exhausted_status(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_format_exhausted_status") == 0) o = emit_hermes_cli_auth_commands_u_format_exhausted_status(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_interactive_auth") == 0) o = emit_hermes_cli_auth_commands_u_interactive_auth(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_pick_provider") == 0) o = emit_hermes_cli_auth_commands_u_pick_provider(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_interactive_add") == 0) o = emit_hermes_cli_auth_commands_u_interactive_add(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_interactive_remove") == 0) o = emit_hermes_cli_auth_commands_u_interactive_remove(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_interactive_reset") == 0) o = emit_hermes_cli_auth_commands_u_interactive_reset(c);
        if (strcmp(op, "hermes_cli_auth_commands_u_interactive_strategy") == 0) o = emit_hermes_cli_auth_commands_u_interactive_strategy(c);
        if (strcmp(op, "hermes_cli_profile_distributio_owned_paths") == 0) o = emit_hermes_cli_profile_distributio_owned_paths(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_load_yaml") == 0) o = emit_hermes_cli_profile_distributio_u_load_yaml(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_dump_yaml") == 0) o = emit_hermes_cli_profile_distributio_u_dump_yaml(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_parse_semver") == 0) o = emit_hermes_cli_profile_distributio_u_parse_semver(c);
        if (strcmp(op, "hermes_cli_profile_distributio_check_hermes_requires") == 0) o = emit_hermes_cli_profile_distributio_check_hermes_requires(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_env_template_from_manifest") == 0) o = emit_hermes_cli_profile_distributio_u_env_template_from_manifest(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_looks_like_git_url") == 0) o = emit_hermes_cli_profile_distributio_u_looks_like_git_url(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_git_clone") == 0) o = emit_hermes_cli_profile_distributio_u_git_clone(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_stage_source") == 0) o = emit_hermes_cli_profile_distributio_u_stage_source(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_reject_distribution_symlinks") == 0) o = emit_hermes_cli_profile_distributio_u_reject_distribution_symlinks(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_has_cron_jobs") == 0) o = emit_hermes_cli_profile_distributio_u_has_cron_jobs(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_count_skills") == 0) o = emit_hermes_cli_profile_distributio_u_count_skills(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_copy_dist_payload") == 0) o = emit_hermes_cli_profile_distributio_u_copy_dist_payload(c);
        if (strcmp(op, "hermes_cli_profile_distributio_u_bootstrap_user_dirs") == 0) o = emit_hermes_cli_profile_distributio_u_bootstrap_user_dirs(c);
        if (strcmp(op, "hermes_cli_security_audit_u_discover_venv") == 0) o = emit_hermes_cli_security_audit_u_discover_venv(c);
        if (strcmp(op, "hermes_cli_security_audit_u_parse_requirements") == 0) o = emit_hermes_cli_security_audit_u_parse_requirements(c);
        if (strcmp(op, "hermes_cli_security_audit_u_parse_pyproject_pins") == 0) o = emit_hermes_cli_security_audit_u_parse_pyproject_pins(c);
        if (strcmp(op, "hermes_cli_security_audit_u_discover_plugins") == 0) o = emit_hermes_cli_security_audit_u_discover_plugins(c);
        if (strcmp(op, "hermes_cli_security_audit_u_extract_mcp_component") == 0) o = emit_hermes_cli_security_audit_u_extract_mcp_component(c);
        if (strcmp(op, "hermes_cli_security_audit_u_discover_mcp") == 0) o = emit_hermes_cli_security_audit_u_discover_mcp(c);
        if (strcmp(op, "hermes_cli_security_audit_u_http_post_json") == 0) o = emit_hermes_cli_security_audit_u_http_post_json(c);
        if (strcmp(op, "hermes_cli_security_audit_u_http_get_json") == 0) o = emit_hermes_cli_security_audit_u_http_get_json(c);
        if (strcmp(op, "hermes_cli_security_audit_u_osv_query_batch") == 0) o = emit_hermes_cli_security_audit_u_osv_query_batch(c);
        if (strcmp(op, "hermes_cli_security_audit_u_osv_fetch_details") == 0) o = emit_hermes_cli_security_audit_u_osv_fetch_details(c);
        if (strcmp(op, "hermes_cli_security_audit_u_render_human") == 0) o = emit_hermes_cli_security_audit_u_render_human(c);
        if (strcmp(op, "hermes_cli_security_audit_u_render_json") == 0) o = emit_hermes_cli_security_audit_u_render_json(c);
        if (strcmp(op, "hermes_cli_security_audit_u_count_components") == 0) o = emit_hermes_cli_security_audit_u_count_components(c);
        if (strcmp(op, "hermes_cli_security_audit_cmd_security_audit") == 0) o = emit_hermes_cli_security_audit_cmd_security_audit(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_u_api_url") == 0) o = emit_hermes_cli_telegram_managed_bo_u_api_url(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_u_parse_owner_user_id") == 0) o = emit_hermes_cli_telegram_managed_bo_u_parse_owner_user_id(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_render_qr_terminal") == 0) o = emit_hermes_cli_telegram_managed_bo_render_qr_terminal(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_print_qr_code") == 0) o = emit_hermes_cli_telegram_managed_bo_print_qr_code(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_generate_username_slug") == 0) o = emit_hermes_cli_telegram_managed_bo_generate_username_slug(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_generate_bot_username") == 0) o = emit_hermes_cli_telegram_managed_bo_generate_bot_username(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_generate_deep_link") == 0) o = emit_hermes_cli_telegram_managed_bo_generate_deep_link(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_generate_pairing_nonce") == 0) o = emit_hermes_cli_telegram_managed_bo_generate_pairing_nonce(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_create_pairing") == 0) o = emit_hermes_cli_telegram_managed_bo_create_pairing(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_poll_pairing_result_once") == 0) o = emit_hermes_cli_telegram_managed_bo_poll_pairing_result_once(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_poll_pairing_once") == 0) o = emit_hermes_cli_telegram_managed_bo_poll_pairing_once(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_poll_for_setup_result") == 0) o = emit_hermes_cli_telegram_managed_bo_poll_for_setup_result(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_poll_for_token") == 0) o = emit_hermes_cli_telegram_managed_bo_poll_for_token(c);
        if (strcmp(op, "hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result") == 0) o = emit_hermes_cli_telegram_managed_bo_auto_setup_telegram_bot_result(c);
        if (strcmp(op, "hermes_cli_backup_u_collect_memory_provider_external_paths") == 0) o = emit_hermes_cli_backup_u_collect_memory_provider_external_paths(c);
        if (strcmp(op, "hermes_cli_backup_u_iter_external_files") == 0) o = emit_hermes_cli_backup_u_iter_external_files(c);
        if (strcmp(op, "hermes_cli_backup_verify_sqlite_integrity") == 0) o = emit_hermes_cli_backup_verify_sqlite_integrity(c);
        if (strcmp(op, "hermes_cli_backup_copy_db_and_verify") == 0) o = emit_hermes_cli_backup_copy_db_and_verify(c);
        if (strcmp(op, "hermes_cli_backup_run_backup") == 0) o = emit_hermes_cli_backup_run_backup(c);
        if (strcmp(op, "hermes_cli_backup_run_import") == 0) o = emit_hermes_cli_backup_run_import(c);
        if (strcmp(op, "hermes_cli_backup_create_quick_snapshot") == 0) o = emit_hermes_cli_backup_create_quick_snapshot(c);
        if (strcmp(op, "hermes_cli_backup_list_quick_snapshots") == 0) o = emit_hermes_cli_backup_list_quick_snapshots(c);
        if (strcmp(op, "hermes_cli_backup_restore_quick_snapshot") == 0) o = emit_hermes_cli_backup_restore_quick_snapshot(c);
        if (strcmp(op, "hermes_cli_backup_run_quick_backup") == 0) o = emit_hermes_cli_backup_run_quick_backup(c);
        if (strcmp(op, "hermes_cli_backup_u_write_full_zip_backup") == 0) o = emit_hermes_cli_backup_u_write_full_zip_backup(c);
        if (strcmp(op, "hermes_cli_backup_create_pre_update_backup") == 0) o = emit_hermes_cli_backup_create_pre_update_backup(c);
        if (strcmp(op, "hermes_cli_backup_create_pre_migration_backup") == 0) o = emit_hermes_cli_backup_create_pre_migration_backup(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_aux_slot_explicit") == 0) o = emit_hermes_cli_kanban_diagnostics_u_aux_slot_explicit(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_main_model_visible") == 0) o = emit_hermes_cli_kanban_diagnostics_u_main_model_visible(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_triage_aux_status") == 0) o = emit_hermes_cli_kanban_diagnostics_triage_aux_status(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_hallucinated_cards(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_triage_aux_unavailable(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_prose_phantom_refs(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_repeated_failures") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_repeated_failures(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_repeated_crashes") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_repeated_crashes(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_stuck_in_blocked(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_block_unblock_cycling(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready") == 0) o = emit_hermes_cli_kanban_diagnostics_u_rule_stranded_in_ready(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_config_from_kanban_config") == 0) o = emit_hermes_cli_kanban_diagnostics_config_from_kanban_config(c);
        if (strcmp(op, "hermes_cli_kanban_diagnostics_config_from_runtime_config") == 0) o = emit_hermes_cli_kanban_diagnostics_config_from_runtime_config(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_load_catalog_config") == 0) o = emit_hermes_cli_model_catalog_u_load_catalog_config(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_cache_path") == 0) o = emit_hermes_cli_model_catalog_u_cache_path(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_fetch_manifest_with_fallback") == 0) o = emit_hermes_cli_model_catalog_u_fetch_manifest_with_fallback(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_validate_manifest") == 0) o = emit_hermes_cli_model_catalog_u_validate_manifest(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_read_disk_cache") == 0) o = emit_hermes_cli_model_catalog_u_read_disk_cache(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_write_disk_cache") == 0) o = emit_hermes_cli_model_catalog_u_write_disk_cache(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_fetch_provider_override") == 0) o = emit_hermes_cli_model_catalog_u_fetch_provider_override(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_get_provider_block") == 0) o = emit_hermes_cli_model_catalog_u_get_provider_block(c);
        if (strcmp(op, "hermes_cli_model_catalog_get_curated_openrouter_models") == 0) o = emit_hermes_cli_model_catalog_get_curated_openrouter_models(c);
        if (strcmp(op, "hermes_cli_model_catalog_get_curated_nous_models") == 0) o = emit_hermes_cli_model_catalog_get_curated_nous_models(c);
        if (strcmp(op, "hermes_cli_model_catalog_u_default_model_from_block") == 0) o = emit_hermes_cli_model_catalog_u_default_model_from_block(c);
        if (strcmp(op, "hermes_cli_model_catalog_get_default_model_from_cache") == 0) o = emit_hermes_cli_model_catalog_get_default_model_from_cache(c);
        if (strcmp(op, "hermes_cli_model_catalog_reset_cache") == 0) o = emit_hermes_cli_model_catalog_reset_cache(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_display_source") == 0) o = emit_hermes_cli_skills_hub_u_display_source(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_resolve_short_name") == 0) o = emit_hermes_cli_skills_hub_u_resolve_short_name(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_format_extra_metadata_lines") == 0) o = emit_hermes_cli_skills_hub_u_format_extra_metadata_lines(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_resolve_source_meta_and_bundle") == 0) o = emit_hermes_cli_skills_hub_u_resolve_source_meta_and_bundle(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_derive_category_from_install_path") == 0) o = emit_hermes_cli_skills_hub_u_derive_category_from_install_path(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_is_valid_installed_skill_name") == 0) o = emit_hermes_cli_skills_hub_u_is_valid_installed_skill_name(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_existing_categories") == 0) o = emit_hermes_cli_skills_hub_u_existing_categories(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_prompt_for_skill_name") == 0) o = emit_hermes_cli_skills_hub_u_prompt_for_skill_name(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_prompt_for_category") == 0) o = emit_hermes_cli_skills_hub_u_prompt_for_category(c);
        if (strcmp(op, "hermes_cli_skills_hub_do_list_modified") == 0) o = emit_hermes_cli_skills_hub_do_list_modified(c);
        if (strcmp(op, "hermes_cli_skills_hub_do_diff") == 0) o = emit_hermes_cli_skills_hub_do_diff(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_github_publish") == 0) o = emit_hermes_cli_skills_hub_u_github_publish(c);
        if (strcmp(op, "hermes_cli_skills_hub_u_print_skills_help") == 0) o = emit_hermes_cli_skills_hub_u_print_skills_help(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_color") == 0) o = emit_hermes_cli_skin_engine_get_color(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_spinner_wings") == 0) o = emit_hermes_cli_skin_engine_get_spinner_wings(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_branding") == 0) o = emit_hermes_cli_skin_engine_get_branding(c);
        if (strcmp(op, "hermes_cli_skin_engine_u_skins_dir") == 0) o = emit_hermes_cli_skin_engine_u_skins_dir(c);
        if (strcmp(op, "hermes_cli_skin_engine_u_load_skin_from_yaml") == 0) o = emit_hermes_cli_skin_engine_u_load_skin_from_yaml(c);
        if (strcmp(op, "hermes_cli_skin_engine_u_mapping_or_empty") == 0) o = emit_hermes_cli_skin_engine_u_mapping_or_empty(c);
        if (strcmp(op, "hermes_cli_skin_engine_u_build_skin_config") == 0) o = emit_hermes_cli_skin_engine_u_build_skin_config(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_active_skin_name") == 0) o = emit_hermes_cli_skin_engine_get_active_skin_name(c);
        if (strcmp(op, "hermes_cli_skin_engine_init_skin_from_config") == 0) o = emit_hermes_cli_skin_engine_init_skin_from_config(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_active_prompt_symbol") == 0) o = emit_hermes_cli_skin_engine_get_active_prompt_symbol(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_active_help_header") == 0) o = emit_hermes_cli_skin_engine_get_active_help_header(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_active_goodbye") == 0) o = emit_hermes_cli_skin_engine_get_active_goodbye(c);
        if (strcmp(op, "hermes_cli_skin_engine_get_prompt_toolkit_style_overrides") == 0) o = emit_hermes_cli_skin_engine_get_prompt_toolkit_style_overrides(c);
        if (strcmp(op, "hermes_cli_claw_u_detect_openclaw_processes") == 0) o = emit_hermes_cli_claw_u_detect_openclaw_processes(c);
        if (strcmp(op, "hermes_cli_claw_u_warn_if_openclaw_running") == 0) o = emit_hermes_cli_claw_u_warn_if_openclaw_running(c);
        if (strcmp(op, "hermes_cli_claw_u_warn_if_gateway_running") == 0) o = emit_hermes_cli_claw_u_warn_if_gateway_running(c);
        if (strcmp(op, "hermes_cli_claw_u_find_migration_script") == 0) o = emit_hermes_cli_claw_u_find_migration_script(c);
        if (strcmp(op, "hermes_cli_claw_u_load_migration_module") == 0) o = emit_hermes_cli_claw_u_load_migration_module(c);
        if (strcmp(op, "hermes_cli_claw_u_find_openclaw_dirs") == 0) o = emit_hermes_cli_claw_u_find_openclaw_dirs(c);
        if (strcmp(op, "hermes_cli_claw_u_scan_workspace_state") == 0) o = emit_hermes_cli_claw_u_scan_workspace_state(c);
        if (strcmp(op, "hermes_cli_claw_u_archive_directory") == 0) o = emit_hermes_cli_claw_u_archive_directory(c);
        if (strcmp(op, "hermes_cli_claw_claw_command") == 0) o = emit_hermes_cli_claw_claw_command(c);
        if (strcmp(op, "hermes_cli_claw_u_cmd_migrate") == 0) o = emit_hermes_cli_claw_u_cmd_migrate(c);
        if (strcmp(op, "hermes_cli_claw_u_cmd_cleanup") == 0) o = emit_hermes_cli_claw_u_cmd_cleanup(c);
        if (strcmp(op, "hermes_cli_claw_u_print_migration_report") == 0) o = emit_hermes_cli_claw_u_print_migration_report(c);
        if (strcmp(op, "hermes_cli_env_loader_get_secret_source") == 0) o = emit_hermes_cli_env_loader_get_secret_source(c);
        if (strcmp(op, "hermes_cli_env_loader_get_secret_source_values") == 0) o = emit_hermes_cli_env_loader_get_secret_source_values(c);
        if (strcmp(op, "hermes_cli_env_loader_reset_secret_source_cache") == 0) o = emit_hermes_cli_env_loader_reset_secret_source_cache(c);
        if (strcmp(op, "hermes_cli_env_loader_format_secret_source_suffix") == 0) o = emit_hermes_cli_env_loader_format_secret_source_suffix(c);
        if (strcmp(op, "hermes_cli_env_loader_u_format_offending_chars") == 0) o = emit_hermes_cli_env_loader_u_format_offending_chars(c);
        if (strcmp(op, "hermes_cli_env_loader_u_sanitize_loaded_credentials") == 0) o = emit_hermes_cli_env_loader_u_sanitize_loaded_credentials(c);
        if (strcmp(op, "hermes_cli_env_loader_u_load_dotenv_with_fallback") == 0) o = emit_hermes_cli_env_loader_u_load_dotenv_with_fallback(c);
        if (strcmp(op, "hermes_cli_env_loader_u_sanitize_env_file_if_needed") == 0) o = emit_hermes_cli_env_loader_u_sanitize_env_file_if_needed(c);
        if (strcmp(op, "hermes_cli_env_loader_u_apply_managed_env") == 0) o = emit_hermes_cli_env_loader_u_apply_managed_env(c);
        if (strcmp(op, "hermes_cli_env_loader_u_apply_external_secret_sources") == 0) o = emit_hermes_cli_env_loader_u_apply_external_secret_sources(c);
        if (strcmp(op, "hermes_cli_env_loader_u_remediation_hint") == 0) o = emit_hermes_cli_env_loader_u_remediation_hint(c);
        if (strcmp(op, "hermes_cli_env_loader_u_load_secrets_config") == 0) o = emit_hermes_cli_env_loader_u_load_secrets_config(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_log_info") == 0) o = emit_hermes_cli_gui_uninstall_log_info(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_log_success") == 0) o = emit_hermes_cli_gui_uninstall_log_success(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_log_warn") == 0) o = emit_hermes_cli_gui_uninstall_log_warn(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_u_agent_root") == 0) o = emit_hermes_cli_gui_uninstall_u_agent_root(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_desktop_userdata_dir") == 0) o = emit_hermes_cli_gui_uninstall_desktop_userdata_dir(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_source_built_gui_artifacts") == 0) o = emit_hermes_cli_gui_uninstall_source_built_gui_artifacts(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_packaged_gui_app_paths") == 0) o = emit_hermes_cli_gui_uninstall_packaged_gui_app_paths(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_agent_is_installed") == 0) o = emit_hermes_cli_gui_uninstall_agent_is_installed(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_gui_is_installed") == 0) o = emit_hermes_cli_gui_uninstall_gui_is_installed(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_gui_install_summary") == 0) o = emit_hermes_cli_gui_uninstall_gui_install_summary(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_u_remove_path") == 0) o = emit_hermes_cli_gui_uninstall_u_remove_path(c);
        if (strcmp(op, "hermes_cli_gui_uninstall_uninstall_gui") == 0) o = emit_hermes_cli_gui_uninstall_uninstall_gui(c);
        if (strcmp(op, "hermes_cli_active_sessions_coerce_max_concurrent_sessions") == 0) o = emit_hermes_cli_active_sessions_coerce_max_concurrent_sessions(c);
        if (strcmp(op, "hermes_cli_active_sessions_resolve_max_concurrent_sessions") == 0) o = emit_hermes_cli_active_sessions_resolve_max_concurrent_sessions(c);
        if (strcmp(op, "hermes_cli_active_sessions_active_session_limit_message") == 0) o = emit_hermes_cli_active_sessions_active_session_limit_message(c);
        if (strcmp(op, "hermes_cli_active_sessions_u__enter__") == 0) o = emit_hermes_cli_active_sessions_u__enter__(c);
        if (strcmp(op, "hermes_cli_active_sessions_u__exit__") == 0) o = emit_hermes_cli_active_sessions_u__exit__(c);
        if (strcmp(op, "hermes_cli_active_sessions_u_read_entries") == 0) o = emit_hermes_cli_active_sessions_u_read_entries(c);
        if (strcmp(op, "hermes_cli_active_sessions_u_write_entries") == 0) o = emit_hermes_cli_active_sessions_u_write_entries(c);
        if (strcmp(op, "hermes_cli_active_sessions_u_process_start_time") == 0) o = emit_hermes_cli_active_sessions_u_process_start_time(c);
        if (strcmp(op, "hermes_cli_active_sessions_u_optional_float") == 0) o = emit_hermes_cli_active_sessions_u_optional_float(c);
        if (strcmp(op, "hermes_cli_active_sessions_u_prune_dead") == 0) o = emit_hermes_cli_active_sessions_u_prune_dead(c);
        if (strcmp(op, "hermes_cli_active_sessions_transfer_active_session") == 0) o = emit_hermes_cli_active_sessions_transfer_active_session(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_translate_one_server") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_translate_one_server(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_format_toml_value") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_format_toml_value(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_quote_key") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_quote_key(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_render_codex_toml_section") == 0) o = emit_hermes_cli_codex_runtime_plugi_render_codex_toml_section(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_insert_managed_block_at_top_el(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_strip_unmanaged_plugin_tables(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_looks_like_table_header") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_looks_like_table_header(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_strip_existing_managed_block(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_query_codex_plugins") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_query_codex_plugins(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_looks_like_test_tempdir(c);
        if (strcmp(op, "hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry") == 0) o = emit_hermes_cli_codex_runtime_plugi_u_build_hermes_tools_mcp_entry(c);
        if (strcmp(op, "hermes_cli_inventory_with_overrides") == 0) o = emit_hermes_cli_inventory_with_overrides(c);
        if (strcmp(op, "hermes_cli_inventory_build_models_payload") == 0) o = emit_hermes_cli_inventory_build_models_payload(c);
        if (strcmp(op, "hermes_cli_inventory_build_model_options_payload") == 0) o = emit_hermes_cli_inventory_build_model_options_payload(c);
        if (strcmp(op, "hermes_cli_inventory_u_apply_capabilities") == 0) o = emit_hermes_cli_inventory_u_apply_capabilities(c);
        if (strcmp(op, "hermes_cli_inventory_u_append_unconfigured_rows") == 0) o = emit_hermes_cli_inventory_u_append_unconfigured_rows(c);
        if (strcmp(op, "hermes_cli_inventory_u_filter_explicit_provider_rows") == 0) o = emit_hermes_cli_inventory_u_filter_explicit_provider_rows(c);
        if (strcmp(op, "hermes_cli_inventory_u_raw_config_has_enabled_moa_preset") == 0) o = emit_hermes_cli_inventory_u_raw_config_has_enabled_moa_preset(c);
        if (strcmp(op, "hermes_cli_inventory_u_apply_picker_hints") == 0) o = emit_hermes_cli_inventory_u_apply_picker_hints(c);
        if (strcmp(op, "hermes_cli_inventory_u_reorder_canonical") == 0) o = emit_hermes_cli_inventory_u_reorder_canonical(c);
        if (strcmp(op, "hermes_cli_inventory_u_apply_pricing") == 0) o = emit_hermes_cli_inventory_u_apply_pricing(c);
        if (strcmp(op, "hermes_cli_inventory_u_moa_provider_row") == 0) o = emit_hermes_cli_inventory_u_moa_provider_row(c);
        if (strcmp(op, "hermes_cli_journey_u_primary_hex") == 0) o = emit_hermes_cli_journey_u_primary_hex(c);
        if (strcmp(op, "hermes_cli_journey_u_fade") == 0) o = emit_hermes_cli_journey_u_fade(c);
        if (strcmp(op, "hermes_cli_journey_u_row_to_text") == 0) o = emit_hermes_cli_journey_u_row_to_text(c);
        if (strcmp(op, "hermes_cli_journey_u_term_size") == 0) o = emit_hermes_cli_journey_u_term_size(c);
        if (strcmp(op, "hermes_cli_journey_u_frame_renderable") == 0) o = emit_hermes_cli_journey_u_frame_renderable(c);
        if (strcmp(op, "hermes_cli_journey_u_cmd_show") == 0) o = emit_hermes_cli_journey_u_cmd_show(c);
        if (strcmp(op, "hermes_cli_journey_u_cmd_delete") == 0) o = emit_hermes_cli_journey_u_cmd_delete(c);
        if (strcmp(op, "hermes_cli_journey_u_cmd_edit") == 0) o = emit_hermes_cli_journey_u_cmd_edit(c);
        if (strcmp(op, "hermes_cli_journey_u_open_in_editor") == 0) o = emit_hermes_cli_journey_u_open_in_editor(c);
        if (strcmp(op, "hermes_cli_journey_register_cli") == 0) o = emit_hermes_cli_journey_register_cli(c);
        if (strcmp(op, "hermes_cli_journey_cmd_journey") == 0) o = emit_hermes_cli_journey_cmd_journey(c);
        if (strcmp(op, "hermes_cli_middleware_u_safe_copy") == 0) o = emit_hermes_cli_middleware_u_safe_copy(c);
        if (strcmp(op, "hermes_cli_middleware_apply_llm_request_middleware") == 0) o = emit_hermes_cli_middleware_apply_llm_request_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_apply_tool_request_middleware") == 0) o = emit_hermes_cli_middleware_apply_tool_request_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_apply_api_request_middleware") == 0) o = emit_hermes_cli_middleware_apply_api_request_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_run_llm_execution_middleware") == 0) o = emit_hermes_cli_middleware_run_llm_execution_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_run_tool_execution_middleware") == 0) o = emit_hermes_cli_middleware_run_tool_execution_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_run_api_execution_middleware") == 0) o = emit_hermes_cli_middleware_run_api_execution_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_u_invoke_middleware") == 0) o = emit_hermes_cli_middleware_u_invoke_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_u_has_middleware") == 0) o = emit_hermes_cli_middleware_u_has_middleware(c);
        if (strcmp(op, "hermes_cli_middleware_u_get_middleware_callbacks") == 0) o = emit_hermes_cli_middleware_u_get_middleware_callbacks(c);
        if (strcmp(op, "hermes_cli_middleware_u_run_execution_chain") == 0) o = emit_hermes_cli_middleware_u_run_execution_chain(c);
        if (strcmp(op, "hermes_cli_service_manager_u_s6_running") == 0) o = emit_hermes_cli_service_manager_u_s6_running(c);
        if (strcmp(op, "hermes_cli_service_manager_u_profile_dir_for_gateway_service") == 0) o = emit_hermes_cli_service_manager_u_profile_dir_for_gateway_service(c);
        if (strcmp(op, "hermes_cli_service_manager_u_write_gateway_desired_state") == 0) o = emit_hermes_cli_service_manager_u_write_gateway_desired_state(c);
        if (strcmp(op, "hermes_cli_service_manager_u_seed_supervise_skeleton") == 0) o = emit_hermes_cli_service_manager_u_seed_supervise_skeleton(c);
        if (strcmp(op, "hermes_cli_service_manager_u_service_dir") == 0) o = emit_hermes_cli_service_manager_u_service_dir(c);
        if (strcmp(op, "hermes_cli_service_manager_u_service_name") == 0) o = emit_hermes_cli_service_manager_u_service_name(c);
        if (strcmp(op, "hermes_cli_service_manager_u_render_run_script") == 0) o = emit_hermes_cli_service_manager_u_render_run_script(c);
        if (strcmp(op, "hermes_cli_service_manager_u_render_finish_script") == 0) o = emit_hermes_cli_service_manager_u_render_finish_script(c);
        if (strcmp(op, "hermes_cli_service_manager_u_render_log_run") == 0) o = emit_hermes_cli_service_manager_u_render_log_run(c);
        if (strcmp(op, "hermes_cli_service_manager_u_run_svc") == 0) o = emit_hermes_cli_service_manager_u_run_svc(c);
        if (strcmp(op, "hermes_cli_service_manager_u_supervised_pid") == 0) o = emit_hermes_cli_service_manager_u_supervised_pid(c);
        if (strcmp(op, "hermes_cli_browser_connect_chrome_debug_data_dir") == 0) o = emit_hermes_cli_browser_connect_chrome_debug_data_dir(c);
        if (strcmp(op, "hermes_cli_browser_connect_u_chrome_debug_args") == 0) o = emit_hermes_cli_browser_connect_u_chrome_debug_args(c);
        if (strcmp(op, "hermes_cli_browser_connect_discover_local_cdp_url") == 0) o = emit_hermes_cli_browser_connect_discover_local_cdp_url(c);
        if (strcmp(op, "hermes_cli_browser_connect_local_port_in_use") == 0) o = emit_hermes_cli_browser_connect_local_port_in_use(c);
        if (strcmp(op, "hermes_cli_browser_connect_find_free_debug_port") == 0) o = emit_hermes_cli_browser_connect_find_free_debug_port(c);
        if (strcmp(op, "hermes_cli_browser_connect_manual_chrome_debug_command") == 0) o = emit_hermes_cli_browser_connect_manual_chrome_debug_command(c);
        if (strcmp(op, "hermes_cli_browser_connect_u_detach_kwargs") == 0) o = emit_hermes_cli_browser_connect_u_detach_kwargs(c);
        if (strcmp(op, "hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it") == 0) o = emit_hermes_cli_browser_connect_u_wait_for_browser_debug_ready_or_it(c);
        if (strcmp(op, "hermes_cli_browser_connect_u_read_stderr_tail") == 0) o = emit_hermes_cli_browser_connect_u_read_stderr_tail(c);
        if (strcmp(op, "hermes_cli_browser_connect_launch_chrome_debug") == 0) o = emit_hermes_cli_browser_connect_launch_chrome_debug(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_path_is_public") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_path_is_public(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_ordered_session_providers") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_ordered_session_providers(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_unauth_response") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_unauth_response(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_auto_sso_response") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_auto_sso_response(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_safe_next_target") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_safe_next_target(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_extract_bearer") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_extract_bearer(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_verify_bearer") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_verify_bearer(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_gated_auth_middleware") == 0) o = emit_hermes_cli_dashboard_auth_midd_gated_auth_middleware(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_expires_in_seconds") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_expires_in_seconds(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_midd_u_attempt_refresh") == 0) o = emit_hermes_cli_dashboard_auth_midd_u_attempt_refresh(c);
        if (strcmp(op, "hermes_cli_dump_u_dotenv_key_names") == 0) o = emit_hermes_cli_dump_u_dotenv_key_names(c);
        if (strcmp(op, "hermes_cli_dump_u_get_git_commit") == 0) o = emit_hermes_cli_dump_u_get_git_commit(c);
        if (strcmp(op, "hermes_cli_dump_u_count_skills") == 0) o = emit_hermes_cli_dump_u_count_skills(c);
        if (strcmp(op, "hermes_cli_dump_u_count_mcp_servers") == 0) o = emit_hermes_cli_dump_u_count_mcp_servers(c);
        if (strcmp(op, "hermes_cli_dump_u_cron_summary") == 0) o = emit_hermes_cli_dump_u_cron_summary(c);
        if (strcmp(op, "hermes_cli_dump_u_configured_platforms") == 0) o = emit_hermes_cli_dump_u_configured_platforms(c);
        if (strcmp(op, "hermes_cli_dump_u_memory_provider") == 0) o = emit_hermes_cli_dump_u_memory_provider(c);
        if (strcmp(op, "hermes_cli_dump_u_get_model_and_provider") == 0) o = emit_hermes_cli_dump_u_get_model_and_provider(c);
        if (strcmp(op, "hermes_cli_dump_u_config_overrides") == 0) o = emit_hermes_cli_dump_u_config_overrides(c);
        if (strcmp(op, "hermes_cli_dump_run_dump") == 0) o = emit_hermes_cli_dump_run_dump(c);
        if (strcmp(op, "hermes_cli_projects_db_projects_db_path") == 0) o = emit_hermes_cli_projects_db_projects_db_path(c);
        if (strcmp(op, "hermes_cli_projects_db_u_new_project_id") == 0) o = emit_hermes_cli_projects_db_u_new_project_id(c);
        if (strcmp(op, "hermes_cli_projects_db_u_now") == 0) o = emit_hermes_cli_projects_db_u_now(c);
        if (strcmp(op, "hermes_cli_projects_db_connect_closing") == 0) o = emit_hermes_cli_projects_db_connect_closing(c);
        if (strcmp(op, "hermes_cli_projects_db_u_migrate_add_optional_columns") == 0) o = emit_hermes_cli_projects_db_u_migrate_add_optional_columns(c);
        if (strcmp(op, "hermes_cli_projects_db_u_project_from_row") == 0) o = emit_hermes_cli_projects_db_u_project_from_row(c);
        if (strcmp(op, "hermes_cli_projects_db_u_attach_folders") == 0) o = emit_hermes_cli_projects_db_u_attach_folders(c);
        if (strcmp(op, "hermes_cli_projects_db_get_discovery_policy_key") == 0) o = emit_hermes_cli_projects_db_get_discovery_policy_key(c);
        if (strcmp(op, "hermes_cli_projects_db_reconcile_discovered_repos_policy") == 0) o = emit_hermes_cli_projects_db_reconcile_discovered_repos_policy(c);
        if (strcmp(op, "hermes_cli_projects_db_clear_discovered_repos") == 0) o = emit_hermes_cli_projects_db_clear_discovered_repos(c);
        if (strcmp(op, "hermes_cli_pty_session_append") == 0) o = emit_hermes_cli_pty_session_append(c);
        if (strcmp(op, "hermes_cli_pty_session_truncated") == 0) o = emit_hermes_cli_pty_session_truncated(c);
        if (strcmp(op, "hermes_cli_pty_session_u_drain") == 0) o = emit_hermes_cli_pty_session_u_drain(c);
        if (strcmp(op, "hermes_cli_pty_session_detach") == 0) o = emit_hermes_cli_pty_session_detach(c);
        if (strcmp(op, "hermes_cli_pty_session_run_reaper") == 0) o = emit_hermes_cli_pty_session_run_reaper(c);
        if (strcmp(op, "hermes_cli_pty_session_attach_or_spawn") == 0) o = emit_hermes_cli_pty_session_attach_or_spawn(c);
        if (strcmp(op, "hermes_cli_pty_session_detach_2") == 0) o = emit_hermes_cli_pty_session_detach_2(c);
        if (strcmp(op, "hermes_cli_pty_session_reap_idle") == 0) o = emit_hermes_cli_pty_session_reap_idle(c);
        if (strcmp(op, "hermes_cli_pty_session_u_reap_one_idle_or_raise") == 0) o = emit_hermes_cli_pty_session_u_reap_one_idle_or_raise(c);
        if (strcmp(op, "hermes_cli_pty_session_close_all") == 0) o = emit_hermes_cli_pty_session_close_all(c);
        if (strcmp(op, "hermes_cli_webhook_u_subscriptions_path") == 0) o = emit_hermes_cli_webhook_u_subscriptions_path(c);
        if (strcmp(op, "hermes_cli_webhook_u_load_subscriptions") == 0) o = emit_hermes_cli_webhook_u_load_subscriptions(c);
        if (strcmp(op, "hermes_cli_webhook_u_save_subscriptions") == 0) o = emit_hermes_cli_webhook_u_save_subscriptions(c);
        if (strcmp(op, "hermes_cli_webhook_u_get_webhook_config") == 0) o = emit_hermes_cli_webhook_u_get_webhook_config(c);
        if (strcmp(op, "hermes_cli_webhook_u_is_webhook_enabled") == 0) o = emit_hermes_cli_webhook_u_is_webhook_enabled(c);
        if (strcmp(op, "hermes_cli_webhook_u_get_webhook_base_url") == 0) o = emit_hermes_cli_webhook_u_get_webhook_base_url(c);
        if (strcmp(op, "hermes_cli_webhook_u_setup_hint") == 0) o = emit_hermes_cli_webhook_u_setup_hint(c);
        if (strcmp(op, "hermes_cli_webhook_u_require_webhook_enabled") == 0) o = emit_hermes_cli_webhook_u_require_webhook_enabled(c);
        if (strcmp(op, "hermes_cli_webhook_u_cmd_subscribe") == 0) o = emit_hermes_cli_webhook_u_cmd_subscribe(c);
        if (strcmp(op, "hermes_cli_webhook_u_cmd_remove") == 0) o = emit_hermes_cli_webhook_u_cmd_remove(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_run") == 0) o = emit_hermes_cli_curator_u_cmd_run(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_pause") == 0) o = emit_hermes_cli_curator_u_cmd_pause(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_pin") == 0) o = emit_hermes_cli_curator_u_cmd_pin(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_unpin") == 0) o = emit_hermes_cli_curator_u_cmd_unpin(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_restore") == 0) o = emit_hermes_cli_curator_u_cmd_restore(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_archive") == 0) o = emit_hermes_cli_curator_u_cmd_archive(c);
        if (strcmp(op, "hermes_cli_curator_u_idle_days") == 0) o = emit_hermes_cli_curator_u_idle_days(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_prune") == 0) o = emit_hermes_cli_curator_u_cmd_prune(c);
        if (strcmp(op, "hermes_cli_curator_u_cmd_list_archived") == 0) o = emit_hermes_cli_curator_u_cmd_list_archived(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_register_cli") == 0) o = emit_hermes_cli_onepassword_secrets_register_cli(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_cmd_set") == 0) o = emit_hermes_cli_onepassword_secrets_cmd_set(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_cmd_remove") == 0) o = emit_hermes_cli_onepassword_secrets_cmd_remove(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_cmd_token") == 0) o = emit_hermes_cli_onepassword_secrets_cmd_token(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_cmd_sync") == 0) o = emit_hermes_cli_onepassword_secrets_cmd_sync(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_cmd_disable") == 0) o = emit_hermes_cli_onepassword_secrets_cmd_disable(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_u_yn") == 0) o = emit_hermes_cli_onepassword_secrets_u_yn(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_u_op_version") == 0) o = emit_hermes_cli_onepassword_secrets_u_op_version(c);
        if (strcmp(op, "hermes_cli_onepassword_secrets_u_op_whoami") == 0) o = emit_hermes_cli_onepassword_secrets_u_op_whoami(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_is_root") == 0) o = emit_hermes_cli_security_audit_star_u_is_root(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_running_as_root") == 0) o = emit_hermes_cli_security_audit_star_u_running_as_root(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_iter_sshd_config_lines") == 0) o = emit_hermes_cli_security_audit_star_u_iter_sshd_config_lines(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_ssh_password_auth_enabled") == 0) o = emit_hermes_cli_security_audit_star_u_ssh_password_auth_enabled(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_path_is_mounted") == 0) o = emit_hermes_cli_security_audit_star_u_path_is_mounted(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_container_no_volume_mount") == 0) o = emit_hermes_cli_security_audit_star_u_container_no_volume_mount(c);
        if (strcmp(op, "hermes_cli_security_audit_star_u_network_listener_without_auth") == 0) o = emit_hermes_cli_security_audit_star_u_network_listener_without_auth(c);
        if (strcmp(op, "hermes_cli_security_audit_star_run_security_audit") == 0) o = emit_hermes_cli_security_audit_star_run_security_audit(c);
        if (strcmp(op, "hermes_cli_security_audit_star_log_startup_security_warnings") == 0) o = emit_hermes_cli_security_audit_star_log_startup_security_warnings(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_u_b64url_no_pad") == 0) o = emit_hermes_cli_dashboard_auth_nati_u_b64url_no_pad(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_u_s256") == 0) o = emit_hermes_cli_dashboard_auth_nati_u_s256(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_u_gc_locked") == 0) o = emit_hermes_cli_dashboard_auth_nati_u_gc_locked(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_u_capacity_ok_locked") == 0) o = emit_hermes_cli_dashboard_auth_nati_u_capacity_ok_locked(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_register_pending") == 0) o = emit_hermes_cli_dashboard_auth_nati_register_pending(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_get_pending") == 0) o = emit_hermes_cli_dashboard_auth_nati_get_pending(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_complete_pending") == 0) o = emit_hermes_cli_dashboard_auth_nati_complete_pending(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_nati_redeem_code") == 0) o = emit_hermes_cli_dashboard_auth_nati_redeem_code(c);
        if (strcmp(op, "hermes_cli_mcp_picker_is_custom") == 0) o = emit_hermes_cli_mcp_picker_is_custom(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_build_rows") == 0) o = emit_hermes_cli_mcp_picker_u_build_rows(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_format_row") == 0) o = emit_hermes_cli_mcp_picker_u_format_row(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_enable_disable") == 0) o = emit_hermes_cli_mcp_picker_u_enable_disable(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_configure_tools") == 0) o = emit_hermes_cli_mcp_picker_u_configure_tools(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_remove_custom") == 0) o = emit_hermes_cli_mcp_picker_u_remove_custom(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_handle_row") == 0) o = emit_hermes_cli_mcp_picker_u_handle_row(c);
        if (strcmp(op, "hermes_cli_mcp_picker_u_print_rows_text") == 0) o = emit_hermes_cli_mcp_picker_u_print_rows_text(c);
        if (strcmp(op, "hermes_cli_proxy_cli_register_cli") == 0) o = emit_hermes_cli_proxy_cli_register_cli(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_install") == 0) o = emit_hermes_cli_proxy_cli_cmd_install(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_start") == 0) o = emit_hermes_cli_proxy_cli_cmd_start(c);
        if (strcmp(op, "hermes_cli_proxy_cli_format_status_text") == 0) o = emit_hermes_cli_proxy_cli_format_status_text(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_disable") == 0) o = emit_hermes_cli_proxy_cli_cmd_disable(c);
        if (strcmp(op, "hermes_cli_proxy_cli_u_load_env_file_into_environ") == 0) o = emit_hermes_cli_proxy_cli_u_load_env_file_into_environ(c);
        if (strcmp(op, "hermes_cli_proxy_cli_u_yn") == 0) o = emit_hermes_cli_proxy_cli_u_yn(c);
        if (strcmp(op, "hermes_cli_proxy_cli_u_redact_token") == 0) o = emit_hermes_cli_proxy_cli_u_redact_token(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_windows_detach_flags") == 0) o = emit_hermes_cli__subprocess_compat_windows_detach_flags(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay") == 0) o = emit_hermes_cli__subprocess_compat_windows_detach_flags_without_b_ay(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_windows_hide_flags") == 0) o = emit_hermes_cli__subprocess_compat_windows_hide_flags(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_suppress_platform_ver_console") == 0) o = emit_hermes_cli__subprocess_compat_suppress_platform_ver_console(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_windows_detach_popen_kwargs") == 0) o = emit_hermes_cli__subprocess_compat_windows_detach_popen_kwargs(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_u_kill_git_process_tree") == 0) o = emit_hermes_cli__subprocess_compat_u_kill_git_process_tree(c);
        if (strcmp(op, "hermes_cli__subprocess_compat_bounded_git_probe") == 0) o = emit_hermes_cli__subprocess_compat_bounded_git_probe(c);
        if (strcmp(op, "hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te") == 0) o = emit_hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(c);
        if (strcmp(op, "hermes_cli_container_boot_u_read_container_argv") == 0) o = emit_hermes_cli_container_boot_u_read_container_argv(c);
        if (strcmp(op, "hermes_cli_container_boot_u_is_legacy_gateway_run_request") == 0) o = emit_hermes_cli_container_boot_u_is_legacy_gateway_run_request(c);
        if (strcmp(op, "hermes_cli_container_boot_u_read_desired_state") == 0) o = emit_hermes_cli_container_boot_u_read_desired_state(c);
        if (strcmp(op, "hermes_cli_container_boot_u_cleanup_stale_runtime_files") == 0) o = emit_hermes_cli_container_boot_u_cleanup_stale_runtime_files(c);
        if (strcmp(op, "hermes_cli_container_boot_u_register_service") == 0) o = emit_hermes_cli_container_boot_u_register_service(c);
        if (strcmp(op, "hermes_cli_container_boot_u_write_reconcile_log") == 0) o = emit_hermes_cli_container_boot_u_write_reconcile_log(c);
        if (strcmp(op, "hermes_cli_copilot_auth_validate_copilot_token") == 0) o = emit_hermes_cli_copilot_auth_validate_copilot_token(c);
        if (strcmp(op, "hermes_cli_copilot_auth_resolve_copilot_token") == 0) o = emit_hermes_cli_copilot_auth_resolve_copilot_token(c);
        if (strcmp(op, "hermes_cli_copilot_auth_u_gh_cli_candidates") == 0) o = emit_hermes_cli_copilot_auth_u_gh_cli_candidates(c);
        if (strcmp(op, "hermes_cli_copilot_auth_u_try_gh_cli_token") == 0) o = emit_hermes_cli_copilot_auth_u_try_gh_cli_token(c);
        if (strcmp(op, "hermes_cli_copilot_auth_exchange_copilot_token") == 0) o = emit_hermes_cli_copilot_auth_exchange_copilot_token(c);
        if (strcmp(op, "hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep") == 0) o = emit_hermes_cli_copilot_auth_u_derive_base_url_from_proxy_ep(c);
        if (strcmp(op, "hermes_cli_copilot_auth_copilot_request_headers") == 0) o = emit_hermes_cli_copilot_auth_copilot_request_headers(c);
        if (strcmp(op, "hermes_cli_cron_u_normalize_skills") == 0) o = emit_hermes_cli_cron_u_normalize_skills(c);
        if (strcmp(op, "hermes_cli_cron_u_cron_api") == 0) o = emit_hermes_cli_cron_u_cron_api(c);
        if (strcmp(op, "hermes_cli_cron_u_active_cron_provider_name") == 0) o = emit_hermes_cli_cron_u_active_cron_provider_name(c);
        if (strcmp(op, "hermes_cli_cron_u_warn_if_gateway_not_running") == 0) o = emit_hermes_cli_cron_u_warn_if_gateway_not_running(c);
        if (strcmp(op, "hermes_cli_cron_cron_runs") == 0) o = emit_hermes_cli_cron_cron_runs(c);
        if (strcmp(op, "hermes_cli_cron_u_print_active_jobs_summary") == 0) o = emit_hermes_cli_cron_u_print_active_jobs_summary(c);
        if (strcmp(op, "hermes_cli_cron_u_job_action") == 0) o = emit_hermes_cli_cron_u_job_action(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_start_login") == 0) o = emit_hermes_cli_dashboard_auth_base_start_login(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_complete_login") == 0) o = emit_hermes_cli_dashboard_auth_base_complete_login(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_verify_session") == 0) o = emit_hermes_cli_dashboard_auth_base_verify_session(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_refresh_session") == 0) o = emit_hermes_cli_dashboard_auth_base_refresh_session(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_revoke_session") == 0) o = emit_hermes_cli_dashboard_auth_base_revoke_session(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_complete_password_login") == 0) o = emit_hermes_cli_dashboard_auth_base_complete_password_login(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_base_assert_protocol_compliance") == 0) o = emit_hermes_cli_dashboard_auth_base_assert_protocol_compliance(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_u_timeout_seconds") == 0) o = emit_hermes_cli_nous_auth_keepalive_u_timeout_seconds(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_u_entry_state") == 0) o = emit_hermes_cli_nous_auth_keepalive_u_entry_state(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry") == 0) o = emit_hermes_cli_nous_auth_keepalive_u_refresh_selected_pool_entry(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once") == 0) o = emit_hermes_cli_nous_auth_keepalive_refresh_nous_auth_keepalive_once(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_u_keepalive_loop") == 0) o = emit_hermes_cli_nous_auth_keepalive_u_keepalive_loop(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive") == 0) o = emit_hermes_cli_nous_auth_keepalive_start_nous_auth_keepalive(c);
        if (strcmp(op, "hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive") == 0) o = emit_hermes_cli_nous_auth_keepalive_stop_nous_auth_keepalive(c);
        if (strcmp(op, "hermes_cli_nous_billing_invalidate_cached_token") == 0) o = emit_hermes_cli_nous_billing_invalidate_cached_token(c);
        if (strcmp(op, "hermes_cli_nous_billing_u_request") == 0) o = emit_hermes_cli_nous_billing_u_request(c);
        if (strcmp(op, "hermes_cli_nous_billing_get_subscription_state") == 0) o = emit_hermes_cli_nous_billing_get_subscription_state(c);
        if (strcmp(op, "hermes_cli_nous_billing_post_subscription_preview") == 0) o = emit_hermes_cli_nous_billing_post_subscription_preview(c);
        if (strcmp(op, "hermes_cli_nous_billing_put_subscription_pending_change") == 0) o = emit_hermes_cli_nous_billing_put_subscription_pending_change(c);
        if (strcmp(op, "hermes_cli_nous_billing_delete_subscription_pending_change") == 0) o = emit_hermes_cli_nous_billing_delete_subscription_pending_change(c);
        if (strcmp(op, "hermes_cli_nous_billing_post_subscription_upgrade") == 0) o = emit_hermes_cli_nous_billing_post_subscription_upgrade(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id") == 0) o = emit_hermes_cli_setup_whatsapp_clou_u_validate_phone_number_id(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_u_validate_waba_id") == 0) o = emit_hermes_cli_setup_whatsapp_clou_u_validate_waba_id(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_u_validate_app_id") == 0) o = emit_hermes_cli_setup_whatsapp_clou_u_validate_app_id(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_u_validate_app_secret") == 0) o = emit_hermes_cli_setup_whatsapp_clou_u_validate_app_secret(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_u_validate_access_token") == 0) o = emit_hermes_cli_setup_whatsapp_clou_u_validate_access_token(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_u_prompt_validated") == 0) o = emit_hermes_cli_setup_whatsapp_clou_u_prompt_validated(c);
        if (strcmp(op, "hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup") == 0) o = emit_hermes_cli_setup_whatsapp_clou_run_whatsapp_cloud_setup(c);
        if (strcmp(op, "hermes_cli__early_recovery_u_project_root") == 0) o = emit_hermes_cli__early_recovery_u_project_root(c);
        if (strcmp(op, "hermes_cli__early_recovery_u_pinned_specs") == 0) o = emit_hermes_cli__early_recovery_u_pinned_specs(c);
        if (strcmp(op, "hermes_cli__early_recovery_u_certifi_bundle_broken") == 0) o = emit_hermes_cli__early_recovery_u_certifi_bundle_broken(c);
        if (strcmp(op, "hermes_cli__early_recovery_u_probe_broken_packages") == 0) o = emit_hermes_cli__early_recovery_u_probe_broken_packages(c);
        if (strcmp(op, "hermes_cli__early_recovery_u_run_repair_install") == 0) o = emit_hermes_cli__early_recovery_u_run_repair_install(c);
        if (strcmp(op, "hermes_cli__early_recovery_recover_if_needed") == 0) o = emit_hermes_cli__early_recovery_recover_if_needed(c);
        if (strcmp(op, "hermes_cli_credential_lifecycl_u_providers_for_env_var") == 0) o = emit_hermes_cli_credential_lifecycl_u_providers_for_env_var(c);
        if (strcmp(op, "hermes_cli_credential_lifecycl_u_prune_env_pool_entries") == 0) o = emit_hermes_cli_credential_lifecycl_u_prune_env_pool_entries(c);
        if (strcmp(op, "hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors") == 0) o = emit_hermes_cli_credential_lifecycl_u_scrub_config_yaml_mirrors(c);
        if (strcmp(op, "hermes_cli_credential_lifecycl_purge_env_credential_references") == 0) o = emit_hermes_cli_credential_lifecycl_purge_env_credential_references(c);
        if (strcmp(op, "hermes_cli_credential_lifecycl_save_provider_env_credential") == 0) o = emit_hermes_cli_credential_lifecycl_save_provider_env_credential(c);
        if (strcmp(op, "hermes_cli_credential_lifecycl_remove_provider_env_credential") == 0) o = emit_hermes_cli_credential_lifecycl_remove_provider_env_credential(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_toke_register_token_route") == 0) o = emit_hermes_cli_dashboard_auth_toke_register_token_route(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_toke_is_token_route") == 0) o = emit_hermes_cli_dashboard_auth_toke_is_token_route(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_toke_clear_token_routes") == 0) o = emit_hermes_cli_dashboard_auth_toke_clear_token_routes(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_toke_extract_bearer_token") == 0) o = emit_hermes_cli_dashboard_auth_toke_extract_bearer_token(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_toke_authenticate_token") == 0) o = emit_hermes_cli_dashboard_auth_toke_authenticate_token(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_toke_token_auth_middleware") == 0) o = emit_hermes_cli_dashboard_auth_toke_token_auth_middleware(c);
        if (strcmp(op, "hermes_cli_fallback_cmd_u_read_chain") == 0) o = emit_hermes_cli_fallback_cmd_u_read_chain(c);
        if (strcmp(op, "hermes_cli_fallback_cmd_u_write_chain") == 0) o = emit_hermes_cli_fallback_cmd_u_write_chain(c);
        if (strcmp(op, "hermes_cli_fallback_cmd_u_snapshot_auth_active_provider") == 0) o = emit_hermes_cli_fallback_cmd_u_snapshot_auth_active_provider(c);
        if (strcmp(op, "hermes_cli_fallback_cmd_u_restore_auth_active_provider") == 0) o = emit_hermes_cli_fallback_cmd_u_restore_auth_active_provider(c);
        if (strcmp(op, "hermes_cli_fallback_cmd_u_restore_model_cfg") == 0) o = emit_hermes_cli_fallback_cmd_u_restore_model_cfg(c);
        if (strcmp(op, "hermes_cli_fallback_cmd_u_numbered_pick") == 0) o = emit_hermes_cli_fallback_cmd_u_numbered_pick(c);
        if (strcmp(op, "hermes_cli_managed_scope_get_managed_dir") == 0) o = emit_hermes_cli_managed_scope_get_managed_dir(c);
        if (strcmp(op, "hermes_cli_managed_scope_invalidate_managed_cache") == 0) o = emit_hermes_cli_managed_scope_invalidate_managed_cache(c);
        if (strcmp(op, "hermes_cli_managed_scope_u_cached_read") == 0) o = emit_hermes_cli_managed_scope_u_cached_read(c);
        if (strcmp(op, "hermes_cli_managed_scope_load_managed_config") == 0) o = emit_hermes_cli_managed_scope_load_managed_config(c);
        if (strcmp(op, "hermes_cli_managed_scope_load_managed_env") == 0) o = emit_hermes_cli_managed_scope_load_managed_env(c);
        if (strcmp(op, "hermes_cli_managed_scope_apply_managed_overlay") == 0) o = emit_hermes_cli_managed_scope_apply_managed_overlay(c);
        if (strcmp(op, "hermes_cli_oneshot_u_normalize_toolsets") == 0) o = emit_hermes_cli_oneshot_u_normalize_toolsets(c);
        if (strcmp(op, "hermes_cli_oneshot_u_validate_explicit_toolsets") == 0) o = emit_hermes_cli_oneshot_u_validate_explicit_toolsets(c);
        if (strcmp(op, "hermes_cli_oneshot_u_write_usage_file") == 0) o = emit_hermes_cli_oneshot_u_write_usage_file(c);
        if (strcmp(op, "hermes_cli_oneshot_run_oneshot") == 0) o = emit_hermes_cli_oneshot_run_oneshot(c);
        if (strcmp(op, "hermes_cli_oneshot_u_create_session_db_for_oneshot") == 0) o = emit_hermes_cli_oneshot_u_create_session_db_for_oneshot(c);
        if (strcmp(op, "hermes_cli_oneshot_u_oneshot_clarify_callback") == 0) o = emit_hermes_cli_oneshot_u_oneshot_clarify_callback(c);
        if (strcmp(op, "hermes_cli_providers_is_routing_aggregator") == 0) o = emit_hermes_cli_providers_is_routing_aggregator(c);
        if (strcmp(op, "hermes_cli_providers_host_mandated_api_mode") == 0) o = emit_hermes_cli_providers_host_mandated_api_mode(c);
        if (strcmp(op, "hermes_cli_providers_determine_api_mode") == 0) o = emit_hermes_cli_providers_determine_api_mode(c);
        if (strcmp(op, "hermes_cli_providers_resolve_user_provider") == 0) o = emit_hermes_cli_providers_resolve_user_provider(c);
        if (strcmp(op, "hermes_cli_providers_custom_provider_slug") == 0) o = emit_hermes_cli_providers_custom_provider_slug(c);
        if (strcmp(op, "hermes_cli_providers_resolve_custom_provider") == 0) o = emit_hermes_cli_providers_resolve_custom_provider(c);
        if (strcmp(op, "hermes_cli_secrets_cli_register_cli") == 0) o = emit_hermes_cli_secrets_cli_register_cli(c);
        if (strcmp(op, "hermes_cli_secrets_cli_cmd_token") == 0) o = emit_hermes_cli_secrets_cli_cmd_token(c);
        if (strcmp(op, "hermes_cli_secrets_cli_u_yn") == 0) o = emit_hermes_cli_secrets_cli_u_yn(c);
        if (strcmp(op, "hermes_cli_secrets_cli_u_bws_version") == 0) o = emit_hermes_cli_secrets_cli_u_bws_version(c);
        if (strcmp(op, "hermes_cli_secrets_cli_u_token_validation_status") == 0) o = emit_hermes_cli_secrets_cli_u_token_validation_status(c);
        if (strcmp(op, "hermes_cli_secrets_cli_u_resolve_server_url") == 0) o = emit_hermes_cli_secrets_cli_u_resolve_server_url(c);
        if (strcmp(op, "hermes_cli_skin_cmd_u_skins_dir") == 0) o = emit_hermes_cli_skin_cmd_u_skins_dir(c);
        if (strcmp(op, "hermes_cli_skin_cmd_u_active_skin") == 0) o = emit_hermes_cli_skin_cmd_u_active_skin(c);
        if (strcmp(op, "hermes_cli_skin_cmd_u_use") == 0) o = emit_hermes_cli_skin_cmd_u_use(c);
        if (strcmp(op, "hermes_cli_skin_cmd_u_skin_set") == 0) o = emit_hermes_cli_skin_cmd_u_skin_set(c);
        if (strcmp(op, "hermes_cli_skin_cmd_u_skin_list") == 0) o = emit_hermes_cli_skin_cmd_u_skin_list(c);
        if (strcmp(op, "hermes_cli_skin_cmd_skin_command") == 0) o = emit_hermes_cli_skin_cmd_skin_command(c);
        if (strcmp(op, "hermes_cli_azure_detect_u_resolve_credential") == 0) o = emit_hermes_cli_azure_detect_u_resolve_credential(c);
        if (strcmp(op, "hermes_cli_azure_detect_u_apply_auth_headers") == 0) o = emit_hermes_cli_azure_detect_u_apply_auth_headers(c);
        if (strcmp(op, "hermes_cli_azure_detect_u_http_get_json") == 0) o = emit_hermes_cli_azure_detect_u_http_get_json(c);
        if (strcmp(op, "hermes_cli_azure_detect_u_probe_openai_models") == 0) o = emit_hermes_cli_azure_detect_u_probe_openai_models(c);
        if (strcmp(op, "hermes_cli_azure_detect_u_probe_anthropic_messages") == 0) o = emit_hermes_cli_azure_detect_u_probe_anthropic_messages(c);
        if (strcmp(op, "hermes_cli_codex_models_u_add_forward_compat_models") == 0) o = emit_hermes_cli_codex_models_u_add_forward_compat_models(c);
        if (strcmp(op, "hermes_cli_codex_models_u_extract_chatgpt_account_id") == 0) o = emit_hermes_cli_codex_models_u_extract_chatgpt_account_id(c);
        if (strcmp(op, "hermes_cli_codex_models_u_fetch_models_from_api") == 0) o = emit_hermes_cli_codex_models_u_fetch_models_from_api(c);
        if (strcmp(op, "hermes_cli_codex_models_u_read_default_model") == 0) o = emit_hermes_cli_codex_models_u_read_default_model(c);
        if (strcmp(op, "hermes_cli_codex_models_u_read_cache_models") == 0) o = emit_hermes_cli_codex_models_u_read_cache_models(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_cook_set_session_provider_cookie") == 0) o = emit_hermes_cli_dashboard_auth_cook_set_session_provider_cookie(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_cook_read_session_cookies") == 0) o = emit_hermes_cli_dashboard_auth_cook_read_session_cookies(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_cook_read_session_provider") == 0) o = emit_hermes_cli_dashboard_auth_cook_read_session_provider(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_cook_read_pkce_cookie") == 0) o = emit_hermes_cli_dashboard_auth_cook_read_pkce_cookie(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie") == 0) o = emit_hermes_cli_dashboard_auth_cook_read_sso_attempt_cookie(c);
        if (strcmp(op, "hermes_cli_dingtalk_auth_u_api_post") == 0) o = emit_hermes_cli_dingtalk_auth_u_api_post(c);
        if (strcmp(op, "hermes_cli_dingtalk_auth_wait_for_registration_success") == 0) o = emit_hermes_cli_dingtalk_auth_wait_for_registration_success(c);
        if (strcmp(op, "hermes_cli_dingtalk_auth_u_ensure_qrcode_installed") == 0) o = emit_hermes_cli_dingtalk_auth_u_ensure_qrcode_installed(c);
        if (strcmp(op, "hermes_cli_dingtalk_auth_render_qr_to_terminal") == 0) o = emit_hermes_cli_dingtalk_auth_render_qr_to_terminal(c);
        if (strcmp(op, "hermes_cli_dingtalk_auth_dingtalk_qr_auth") == 0) o = emit_hermes_cli_dingtalk_auth_dingtalk_qr_auth(c);
        if (strcmp(op, "hermes_cli_gateway_enroll_u_default_gateway_id") == 0) o = emit_hermes_cli_gateway_enroll_u_default_gateway_id(c);
        if (strcmp(op, "hermes_cli_gateway_enroll_u_resolve_connector_url") == 0) o = emit_hermes_cli_gateway_enroll_u_resolve_connector_url(c);
        if (strcmp(op, "hermes_cli_gateway_enroll_u_resolve_identity_token") == 0) o = emit_hermes_cli_gateway_enroll_u_resolve_identity_token(c);
        if (strcmp(op, "hermes_cli_gateway_enroll_u_post_enroll") == 0) o = emit_hermes_cli_gateway_enroll_u_post_enroll(c);
        if (strcmp(op, "hermes_cli_gateway_enroll_cmd_gateway_enroll") == 0) o = emit_hermes_cli_gateway_enroll_cmd_gateway_enroll(c);
        if (strcmp(op, "hermes_cli_mcp_startup_u_has_configured_mcp_servers") == 0) o = emit_hermes_cli_mcp_startup_u_has_configured_mcp_servers(c);
        if (strcmp(op, "hermes_cli_mcp_startup_u_resolve_discovery_timeout") == 0) o = emit_hermes_cli_mcp_startup_u_resolve_discovery_timeout(c);
        if (strcmp(op, "hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th") == 0) o = emit_hermes_cli_mcp_startup_u_discover_mcp_tools_without_interact_th(c);
        if (strcmp(op, "hermes_cli_mcp_startup_mcp_discovery_in_flight") == 0) o = emit_hermes_cli_mcp_startup_mcp_discovery_in_flight(c);
        if (strcmp(op, "hermes_cli_mcp_startup_join_mcp_discovery") == 0) o = emit_hermes_cli_mcp_startup_join_mcp_discovery(c);
        if (strcmp(op, "hermes_cli_moa_config_u_default_reference_models") == 0) o = emit_hermes_cli_moa_config_u_default_reference_models(c);
        if (strcmp(op, "hermes_cli_moa_config_u_coerce_reference_timeout") == 0) o = emit_hermes_cli_moa_config_u_coerce_reference_timeout(c);
        if (strcmp(op, "hermes_cli_moa_config_u_coerce_degraded_reference_policy") == 0) o = emit_hermes_cli_moa_config_u_coerce_degraded_reference_policy(c);
        if (strcmp(op, "hermes_cli_moa_config_coerce_privacy_filter") == 0) o = emit_hermes_cli_moa_config_coerce_privacy_filter(c);
        if (strcmp(op, "hermes_cli_moa_config_moa_usage") == 0) o = emit_hermes_cli_moa_config_moa_usage(c);
        if (strcmp(op, "hermes_cli_proxy_cli_u_print_aiohttp_missing") == 0) o = emit_hermes_cli_proxy_cli_u_print_aiohttp_missing(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_proxy_start") == 0) o = emit_hermes_cli_proxy_cli_cmd_proxy_start(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_proxy_status") == 0) o = emit_hermes_cli_proxy_cli_cmd_proxy_status(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_proxy_list_providers") == 0) o = emit_hermes_cli_proxy_cli_cmd_proxy_list_providers(c);
        if (strcmp(op, "hermes_cli_proxy_cli_cmd_proxy") == 0) o = emit_hermes_cli_proxy_cli_cmd_proxy(c);
        if (strcmp(op, "hermes_cli_session_filters_parse_duration_seconds") == 0) o = emit_hermes_cli_session_filters_parse_duration_seconds(c);
        if (strcmp(op, "hermes_cli_session_filters_parse_point_in_time") == 0) o = emit_hermes_cli_session_filters_parse_point_in_time(c);
        if (strcmp(op, "hermes_cli_session_filters_format_epoch") == 0) o = emit_hermes_cli_session_filters_format_epoch(c);
        if (strcmp(op, "hermes_cli_session_filters_build_prune_filters") == 0) o = emit_hermes_cli_session_filters_build_prune_filters(c);
        if (strcmp(op, "hermes_cli_session_filters_describe_filters") == 0) o = emit_hermes_cli_session_filters_describe_filters(c);
        if (strcmp(op, "hermes_cli_urllib_security_url_origin") == 0) o = emit_hermes_cli_urllib_security_url_origin(c);
        if (strcmp(op, "hermes_cli_urllib_security_redirect_request") == 0) o = emit_hermes_cli_urllib_security_redirect_request(c);
        if (strcmp(op, "hermes_cli_urllib_security_u_sanitize") == 0) o = emit_hermes_cli_urllib_security_u_sanitize(c);
        if (strcmp(op, "hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy") == 0) o = emit_hermes_cli_urllib_security_u_secure_opener_from_installed_po_cy(c);
        if (strcmp(op, "hermes_cli_urllib_security_open_credentialed_url") == 0) o = emit_hermes_cli_urllib_security_open_credentialed_url(c);
        if (strcmp(op, "hermes_cli_bundles_u_cmd_show") == 0) o = emit_hermes_cli_bundles_u_cmd_show(c);
        if (strcmp(op, "hermes_cli_bundles_u_cmd_create") == 0) o = emit_hermes_cli_bundles_u_cmd_create(c);
        if (strcmp(op, "hermes_cli_bundles_u_cmd_delete") == 0) o = emit_hermes_cli_bundles_u_cmd_delete(c);
        if (strcmp(op, "hermes_cli_bundles_register_cli") == 0) o = emit_hermes_cli_bundles_register_cli(c);
        if (strcmp(op, "hermes_cli_dashboard_register_u_generate_dashboard_name") == 0) o = emit_hermes_cli_dashboard_register_u_generate_dashboard_name(c);
        if (strcmp(op, "hermes_cli_dashboard_register_u_register_self_hosted_client") == 0) o = emit_hermes_cli_dashboard_register_u_register_self_hosted_client(c);
        if (strcmp(op, "hermes_cli_dashboard_register_u_print_post_register_hint") == 0) o = emit_hermes_cli_dashboard_register_u_print_post_register_hint(c);
        if (strcmp(op, "hermes_cli_dashboard_register_cmd_dashboard_register") == 0) o = emit_hermes_cli_dashboard_register_cmd_dashboard_register(c);
        if (strcmp(op, "hermes_cli_memory_oauth_u_resolve_flow") == 0) o = emit_hermes_cli_memory_oauth_u_resolve_flow(c);
        if (strcmp(op, "hermes_cli_memory_oauth_u_scope_to_profile") == 0) o = emit_hermes_cli_memory_oauth_u_scope_to_profile(c);
        if (strcmp(op, "hermes_cli_memory_oauth_start_memory_oauth") == 0) o = emit_hermes_cli_memory_oauth_start_memory_oauth(c);
        if (strcmp(op, "hermes_cli_memory_oauth_memory_oauth_status") == 0) o = emit_hermes_cli_memory_oauth_memory_oauth_status(c);
        if (strcmp(op, "hermes_cli_moa_cmd_u_pick_slot") == 0) o = emit_hermes_cli_moa_cmd_u_pick_slot(c);
        if (strcmp(op, "hermes_cli_moa_cmd_u_format_slot") == 0) o = emit_hermes_cli_moa_cmd_u_format_slot(c);
        if (strcmp(op, "hermes_cli_moa_cmd_u_print_config") == 0) o = emit_hermes_cli_moa_cmd_u_print_config(c);
        if (strcmp(op, "hermes_cli_moa_cmd_cmd_moa") == 0) o = emit_hermes_cli_moa_cmd_cmd_moa(c);
        if (strcmp(op, "hermes_cli_proxy_server_u_filter_request_headers") == 0) o = emit_hermes_cli_proxy_server_u_filter_request_headers(c);
        if (strcmp(op, "hermes_cli_proxy_server_u_filter_response_headers") == 0) o = emit_hermes_cli_proxy_server_u_filter_response_headers(c);
        if (strcmp(op, "hermes_cli_proxy_server_create_app") == 0) o = emit_hermes_cli_proxy_server_create_app(c);
        if (strcmp(op, "hermes_cli_proxy_server_run_server") == 0) o = emit_hermes_cli_proxy_server_run_server(c);
        if (strcmp(op, "hermes_cli_secret_prompt_u_collect_masked_input") == 0) o = emit_hermes_cli_secret_prompt_u_collect_masked_input(c);
        if (strcmp(op, "hermes_cli_secret_prompt_u_stream_is_tty") == 0) o = emit_hermes_cli_secret_prompt_u_stream_is_tty(c);
        if (strcmp(op, "hermes_cli_secret_prompt_u_masked_secret_prompt_windows") == 0) o = emit_hermes_cli_secret_prompt_u_masked_secret_prompt_windows(c);
        if (strcmp(op, "hermes_cli_secret_prompt_u_masked_secret_prompt_posix") == 0) o = emit_hermes_cli_secret_prompt_u_masked_secret_prompt_posix(c);
        if (strcmp(op, "hermes_cli_send_cmd_u_read_message_body") == 0) o = emit_hermes_cli_send_cmd_u_read_message_body(c);
        if (strcmp(op, "hermes_cli_send_cmd_u_emit_result") == 0) o = emit_hermes_cli_send_cmd_u_emit_result(c);
        if (strcmp(op, "hermes_cli_send_cmd_u_list_targets") == 0) o = emit_hermes_cli_send_cmd_u_list_targets(c);
        if (strcmp(op, "hermes_cli_send_cmd_u_load_hermes_env") == 0) o = emit_hermes_cli_send_cmd_u_load_hermes_env(c);
        if (strcmp(op, "hermes_cli_session_export_html_u_escape_html") == 0) o = emit_hermes_cli_session_export_html_u_escape_html(c);
        if (strcmp(op, "hermes_cli_session_export_html_u_generate_messages_html") == 0) o = emit_hermes_cli_session_export_html_u_generate_messages_html(c);
        if (strcmp(op, "hermes_cli_session_export_html_generate_multi_session_html_e_rt") == 0) o = emit_hermes_cli_session_export_html_generate_multi_session_html_e_rt(c);
        if (strcmp(op, "hermes_cli_session_export_html_generate_html_export") == 0) o = emit_hermes_cli_session_export_html_generate_html_export(c);
        if (strcmp(op, "hermes_cli_sqlite_runtime_u_version_tuple") == 0) o = emit_hermes_cli_sqlite_runtime_u_version_tuple(c);
        if (strcmp(op, "hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable") == 0) o = emit_hermes_cli_sqlite_runtime_is_sqlite_wal_reset_vulnerable(c);
        if (strcmp(op, "hermes_cli_sqlite_runtime_wal_reset_vulnerable") == 0) o = emit_hermes_cli_sqlite_runtime_wal_reset_vulnerable(c);
        if (strcmp(op, "hermes_cli_sqlite_runtime_probe_sqlite_runtime") == 0) o = emit_hermes_cli_sqlite_runtime_probe_sqlite_runtime(c);
        if (strcmp(op, "hermes_cli_stdio_u_flip_console_code_page_to_utf8") == 0) o = emit_hermes_cli_stdio_u_flip_console_code_page_to_utf8(c);
        if (strcmp(op, "hermes_cli_stdio_u_reconfigure_stream") == 0) o = emit_hermes_cli_stdio_u_reconfigure_stream(c);
        if (strcmp(op, "hermes_cli_stdio_u_default_windows_editor") == 0) o = emit_hermes_cli_stdio_u_default_windows_editor(c);
        if (strcmp(op, "hermes_cli_stdio_u_augment_path_with_known_tools") == 0) o = emit_hermes_cli_stdio_u_augment_path_with_known_tools(c);
        if (strcmp(op, "hermes_cli_dep_ensure_u_has_system_browser") == 0) o = emit_hermes_cli_dep_ensure_u_has_system_browser(c);
        if (strcmp(op, "hermes_cli_dep_ensure_u_has_hermes_agent_browser") == 0) o = emit_hermes_cli_dep_ensure_u_has_hermes_agent_browser(c);
        if (strcmp(op, "hermes_cli_dep_ensure_u_find_install_script") == 0) o = emit_hermes_cli_dep_ensure_u_find_install_script(c);
        if (strcmp(op, "hermes_cli_diagnostics_upload_request_upload_url") == 0) o = emit_hermes_cli_diagnostics_upload_request_upload_url(c);
        if (strcmp(op, "hermes_cli_diagnostics_upload_put_bundle") == 0) o = emit_hermes_cli_diagnostics_upload_put_bundle(c);
        if (strcmp(op, "hermes_cli_diagnostics_upload_share_to_nous") == 0) o = emit_hermes_cli_diagnostics_upload_share_to_nous(c);
        if (strcmp(op, "hermes_cli_goals_draft_contract") == 0) o = emit_hermes_cli_goals_draft_contract(c);
        if (strcmp(op, "hermes_cli_goals_evaluate_after_turn") == 0) o = emit_hermes_cli_goals_evaluate_after_turn(c);
        if (strcmp(op, "hermes_cli_goals_run_kanban_goal_loop") == 0) o = emit_hermes_cli_goals_run_kanban_goal_loop(c);
        if (strcmp(op, "hermes_cli_profiles_u_profile_bound_backend_pids") == 0) o = emit_hermes_cli_profiles_u_profile_bound_backend_pids(c);
        if (strcmp(op, "hermes_cli_profiles_u_stop_profile_backends") == 0) o = emit_hermes_cli_profiles_u_stop_profile_backends(c);
        if (strcmp(op, "hermes_cli_profiles_u_rmtree_with_retry") == 0) o = emit_hermes_cli_profiles_u_rmtree_with_retry(c);
        if (strcmp(op, "hermes_cli_relaunch_u_build_inherited_flag_table") == 0) o = emit_hermes_cli_relaunch_u_build_inherited_flag_table(c);
        if (strcmp(op, "hermes_cli_relaunch_u_extract_inherited_flags") == 0) o = emit_hermes_cli_relaunch_u_extract_inherited_flags(c);
        if (strcmp(op, "hermes_cli_relaunch_resolve_hermes_bin") == 0) o = emit_hermes_cli_relaunch_resolve_hermes_bin(c);
        if (strcmp(op, "hermes_cli_suggestions_cmd_u_fmt_pending") == 0) o = emit_hermes_cli_suggestions_cmd_u_fmt_pending(c);
        if (strcmp(op, "hermes_cli_suggestions_cmd_u_resolve_origin") == 0) o = emit_hermes_cli_suggestions_cmd_u_resolve_origin(c);
        if (strcmp(op, "hermes_cli_suggestions_cmd_handle_suggestions_command") == 0) o = emit_hermes_cli_suggestions_cmd_handle_suggestions_command(c);
        if (strcmp(op, "hermes_cli_checkpoints_u_confirm") == 0) o = emit_hermes_cli_checkpoints_u_confirm(c);
        if (strcmp(op, "hermes_cli_checkpoints_cmd_clear_legacy") == 0) o = emit_hermes_cli_checkpoints_cmd_clear_legacy(c);
        if (strcmp(op, "hermes_cli_cli_agent_setup_mix_u_preload_resumed_session") == 0) o = emit_hermes_cli_cli_agent_setup_mix_u_preload_resumed_session(c);
        if (strcmp(op, "hermes_cli_cli_agent_setup_mix_u_display_resumed_history") == 0) o = emit_hermes_cli_cli_agent_setup_mix_u_display_resumed_history(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_logi_render_login_html") == 0) o = emit_hermes_cli_dashboard_auth_logi_render_login_html(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_logi_u_render_password_form") == 0) o = emit_hermes_cli_dashboard_auth_logi_u_render_password_form(c);
        if (strcmp(op, "hermes_cli_pairing_pairing_command") == 0) o = emit_hermes_cli_pairing_pairing_command(c);
        if (strcmp(op, "hermes_cli_pairing_u_cmd_clear_pending") == 0) o = emit_hermes_cli_pairing_u_cmd_clear_pending(c);
        if (strcmp(op, "hermes_cli_partial_compress_extract_compress_flags") == 0) o = emit_hermes_cli_partial_compress_extract_compress_flags(c);
        if (strcmp(op, "hermes_cli_partial_compress_summarize_compress_preview") == 0) o = emit_hermes_cli_partial_compress_summarize_compress_preview(c);
        if (strcmp(op, "hermes_cli_portal_cli_u_cmd_open") == 0) o = emit_hermes_cli_portal_cli_u_cmd_open(c);
        if (strcmp(op, "hermes_cli_portal_cli_u_cmd_login") == 0) o = emit_hermes_cli_portal_cli_u_cmd_login(c);
        if (strcmp(op, "hermes_cli_provider_catalog_provider_catalog") == 0) o = emit_hermes_cli_provider_catalog_provider_catalog(c);
        if (strcmp(op, "hermes_cli_provider_catalog_provider_catalog_by_slug") == 0) o = emit_hermes_cli_provider_catalog_provider_catalog_by_slug(c);
        if (strcmp(op, "hermes_cli_psutil_android_u_normalize_member_parts") == 0) o = emit_hermes_cli_psutil_android_u_normalize_member_parts(c);
        if (strcmp(op, "hermes_cli_psutil_android_u_safe_extract_tar_gz") == 0) o = emit_hermes_cli_psutil_android_u_safe_extract_tar_gz(c);
        if (strcmp(op, "hermes_cli_slack_cli_u_build_full_manifest") == 0) o = emit_hermes_cli_slack_cli_u_build_full_manifest(c);
        if (strcmp(op, "hermes_cli_slack_cli_slack_manifest_command") == 0) o = emit_hermes_cli_slack_cli_slack_manifest_command(c);
        if (strcmp(op, "hermes_cli_subcommands_dashboa_u_add_server_runtime_args") == 0) o = emit_hermes_cli_subcommands_dashboa_u_add_server_runtime_args(c);
        if (strcmp(op, "hermes_cli_subcommands_dashboa_build_dashboard_parser") == 0) o = emit_hermes_cli_subcommands_dashboa_build_dashboard_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_gateway_u_add_compat_platform_flag") == 0) o = emit_hermes_cli_subcommands_gateway_u_add_compat_platform_flag(c);
        if (strcmp(op, "hermes_cli_subcommands_gateway_build_gateway_parser") == 0) o = emit_hermes_cli_subcommands_gateway_build_gateway_parser(c);
        if (strcmp(op, "hermes_cli__parser_u_inherited_flag") == 0) o = emit_hermes_cli__parser_u_inherited_flag(c);
        if (strcmp(op, "hermes_cli_banner_u_skin_color") == 0) o = emit_hermes_cli_banner_u_skin_color(c);
        if (strcmp(op, "hermes_cli_codex_runtime_switc_check_codex_binary_ok") == 0) o = emit_hermes_cli_codex_runtime_switc_check_codex_binary_ok(c);
        if (strcmp(op, "hermes_cli_config_custom_endpoint_key_env") == 0) o = emit_hermes_cli_config_custom_endpoint_key_env(c);
        if (strcmp(op, "hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix") == 0) o = emit_hermes_cli_dashboard_auth_pref_u_warn_if_malformed_prefix(c);
        if (strcmp(op, "hermes_cli_fallback_config_resolve_entry_api_key") == 0) o = emit_hermes_cli_fallback_config_resolve_entry_api_key(c);
        if (strcmp(op, "hermes_cli_kanban_specify_list_triage_ids") == 0) o = emit_hermes_cli_kanban_specify_list_triage_ids(c);
        if (strcmp(op, "hermes_cli_memory_setup_u_env_line_safe") == 0) o = emit_hermes_cli_memory_setup_u_env_line_safe(c);
        if (strcmp(op, "hermes_cli_profile_describer_u_collect_skills") == 0) o = emit_hermes_cli_profile_describer_u_collect_skills(c);
        if (strcmp(op, "hermes_cli_route_identity_should_clear_context_pin") == 0) o = emit_hermes_cli_route_identity_should_clear_context_pin(c);
        if (strcmp(op, "hermes_cli_session_listing_query_session_listing") == 0) o = emit_hermes_cli_session_listing_query_session_listing(c);
        if (strcmp(op, "hermes_cli_session_recap_u_iter_assistant_tool_calls") == 0) o = emit_hermes_cli_session_recap_u_iter_assistant_tool_calls(c);
        if (strcmp(op, "hermes_cli_skills_config_u_normalize_skill_names") == 0) o = emit_hermes_cli_skills_config_u_normalize_skill_names(c);
        if (strcmp(op, "hermes_cli_subcommands__shared_add_accept_hooks_flag") == 0) o = emit_hermes_cli_subcommands__shared_add_accept_hooks_flag(c);
        if (strcmp(op, "hermes_cli_subcommands_acp_build_acp_parser") == 0) o = emit_hermes_cli_subcommands_acp_build_acp_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_auth_build_auth_parser") == 0) o = emit_hermes_cli_subcommands_auth_build_auth_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_backup_build_backup_parser") == 0) o = emit_hermes_cli_subcommands_backup_build_backup_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_claw_build_claw_parser") == 0) o = emit_hermes_cli_subcommands_claw_build_claw_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_config_build_config_parser") == 0) o = emit_hermes_cli_subcommands_config_build_config_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_console_build_console_parser") == 0) o = emit_hermes_cli_subcommands_console_build_console_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_cron_build_cron_parser") == 0) o = emit_hermes_cli_subcommands_cron_build_cron_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_debug_build_debug_parser") == 0) o = emit_hermes_cli_subcommands_debug_build_debug_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_doctor_build_doctor_parser") == 0) o = emit_hermes_cli_subcommands_doctor_build_doctor_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_dump_build_dump_parser") == 0) o = emit_hermes_cli_subcommands_dump_build_dump_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_gui_build_gui_parser") == 0) o = emit_hermes_cli_subcommands_gui_build_gui_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_hooks_build_hooks_parser") == 0) o = emit_hermes_cli_subcommands_hooks_build_hooks_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_import__build_import_cmd_parser") == 0) o = emit_hermes_cli_subcommands_import__build_import_cmd_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_insight_build_insights_parser") == 0) o = emit_hermes_cli_subcommands_insight_build_insights_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_login_build_login_parser") == 0) o = emit_hermes_cli_subcommands_login_build_login_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_logout_build_logout_parser") == 0) o = emit_hermes_cli_subcommands_logout_build_logout_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_logs_build_logs_parser") == 0) o = emit_hermes_cli_subcommands_logs_build_logs_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_mcp_build_mcp_parser") == 0) o = emit_hermes_cli_subcommands_mcp_build_mcp_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_memory_build_memory_parser") == 0) o = emit_hermes_cli_subcommands_memory_build_memory_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_model_build_model_parser") == 0) o = emit_hermes_cli_subcommands_model_build_model_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_pairing_build_pairing_parser") == 0) o = emit_hermes_cli_subcommands_pairing_build_pairing_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_plugins_build_plugins_parser") == 0) o = emit_hermes_cli_subcommands_plugins_build_plugins_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_profile_build_profile_parser") == 0) o = emit_hermes_cli_subcommands_profile_build_profile_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_prompt__build_prompt_size_parser") == 0) o = emit_hermes_cli_subcommands_prompt__build_prompt_size_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_securit_build_security_parser") == 0) o = emit_hermes_cli_subcommands_securit_build_security_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_setup_build_setup_parser") == 0) o = emit_hermes_cli_subcommands_setup_build_setup_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_skills_build_skills_parser") == 0) o = emit_hermes_cli_subcommands_skills_build_skills_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_skin_build_skin_parser") == 0) o = emit_hermes_cli_subcommands_skin_build_skin_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_slack_build_slack_parser") == 0) o = emit_hermes_cli_subcommands_slack_build_slack_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_status_build_status_parser") == 0) o = emit_hermes_cli_subcommands_status_build_status_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_tools_build_tools_parser") == 0) o = emit_hermes_cli_subcommands_tools_build_tools_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_uninsta_build_uninstall_parser") == 0) o = emit_hermes_cli_subcommands_uninsta_build_uninstall_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_update_build_update_parser") == 0) o = emit_hermes_cli_subcommands_update_build_update_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_version_build_version_parser") == 0) o = emit_hermes_cli_subcommands_version_build_version_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_webhook_build_webhook_parser") == 0) o = emit_hermes_cli_subcommands_webhook_build_webhook_parser(c);
        if (strcmp(op, "hermes_cli_subcommands_whatsap_build_whatsapp_parser") == 0) o = emit_hermes_cli_subcommands_whatsap_build_whatsapp_parser(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
