/* AUTO-GENERATED integration oracle harness for port_main_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_main_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int main_u_exit_after_oneshot(const char *);
extern int main_u_cleanup_oneshot_runtime(const char *);
extern int main_u_run_and_exit_oneshot(const char *);
extern int main_u_set_process_title(const char *);
extern int main_u_config_default_interface_early(const char *);
extern int main_u_wants_tui_early(const char *);
extern int main_u_suppress_mouse_residue_early(const char *);
extern int main_u_is_termux_startup_environment_fast(const char *);
extern int main_u_is_termux_fast_version_argv(const char *);
extern int main_u_read_openai_version_fast(const char *);
extern int main_u_print_fast_version_info(const char *);
extern int main_u_try_termux_ultrafast_version(const char *);
extern int main_u_require_tty(const char *);
extern int main_u_apply_profile_override(const char *);
extern int main_u_is_termux_startup_environment(const char *);
extern int main_u_termux_bundled_skills_fingerprint(const char *);
extern int main_u_termux_bundled_skills_stamp_path(const char *);
extern int main_u_termux_bundled_skills_sync_needed(const char *);
extern int main_u_mark_termux_bundled_skills_synced(const char *);
extern int main_u_sync_bundled_skills_for_startup(const char *);
extern int main_u_termux_should_prefetch_update_check(const char *);
extern int main_u_has_any_provider_configured(const char *);
extern int main_u_session_browse_picker(const char *);
extern int main_u_resolve_last_session(const char *);
extern int main_u_probe_container(const char *);
extern int main_u_exec_in_container(const char *);
extern int main_u_resolve_session_by_name_or_id(const char *);
extern int main_u_print_tui_exit_summary(const char *);
extern int main_u_termux_workspace_install_context(const char *);
extern int main_u_tui_need_npm_install(const char *);
extern int main_u_iter_tui_build_inputs(const char *);
extern int main_u_tui_need_rebuild(const char *);
extern int main_u_ensure_tui_node(const char *);
extern int main_u_find_bundled_tui(const char *);
extern int main_u_make_tui_argv(const char *);
extern int main_u_normalize_tui_toolsets(const char *);
extern int main_u_resolve_tui_heap_mb(const char *);
extern int main_u_safe_tui_cwd(const char *);
extern int main_u_apply_tui_python_env(const char *);
extern int main_u_launch_tui(const char *);
extern int main_u_pin_kanban_board_env(const char *);
extern int main_u_sync_bundled_skills_quietly(const char *);
extern int main_u_resolve_use_tui(const char *);
extern int main_cmd_chat(const char *);
extern int main_cmd_proxy(const char *);
extern int main_cmd_whatsapp(const char *);
extern int main_cmd_whatsapp_cloud(const char *);
extern int main_u_is_profile_api_key_provider(const char *);
extern int main_select_provider_and_model(const char *);
extern int main_u_clear_stale_openai_base_url(const char *);
extern int main_u_all_aux_tasks(const char *);
extern int main_u_format_aux_current(const char *);
extern int main_u_save_aux_choice(const char *);
extern int main_u_reset_aux_to_auto(const char *);
extern int main_u_aux_config_menu(const char *);
extern int main_u_aux_select_for_task(const char *);
extern int main_u_aux_flow_provider_model(const char *);
extern int main_u_aux_flow_custom_endpoint(const char *);
extern int main_u_prompt_provider_choice(const char *);
extern int main_u_prompt_custom_api_mode_selection(const char *);
extern int main_u_custom_provider_api_key_config_value(const char *);
extern int main_u_custom_provider_base_url_config_value(const char *);
extern int main_u_save_custom_provider(const char *);
extern int main_u_remove_custom_provider(const char *);
extern int main_u__getattr__(const char *);
extern int main_u_set_reasoning_effort(const char *);
extern int main_u_prompt_reasoning_effort_selection(const char *);
extern int main_u_run_anthropic_oauth_flow(const char *);
extern int main_cmd_login(const char *);
extern int main_cmd_logout(const char *);
extern int main_cmd_slack(const char *);
extern int main_cmd_project(const char *);
extern int main_cmd_hooks(const char *);
extern int main_cmd_security(const char *);
extern int main_cmd_import(const char *);
extern int main_u_print_version_info(const char *);
extern int main_cmd_version(const char *);
extern int main_u_clear_bytecode_cache(const char *);
extern int main_u_capture_head_sha(const char *);
extern int main_u_validate_critical_files_syntax(const char *);
extern int main_u_gateway_prompt(const char *);
extern int main_u_web_ui_build_needed(const char *);
extern int main_u_compute_web_ui_content_hash(const char *);
extern int main_u_web_ui_stamp_path(const char *);
extern int main_u_write_web_ui_build_stamp(const char *);
extern int main_u_run_with_idle_timeout(const char *);
extern int main_u_nixos_build_env(const char *);
extern int main_u_run_npm_install_deterministic(const char *);
extern int main_u_build_web_ui(const char *);
extern int main_u_do_build_web_ui(const char *);
extern int main_u_desktop_dist_exists(const char *);
extern int main_u_compute_desktop_content_hash(const char *);
extern int main_u_desktop_stamp_path(const char *);
extern int main_u_desktop_build_needed(const char *);
extern int main_u_write_desktop_build_stamp(const char *);
extern int main_u_desktop_packaged_executable(const char *);
extern int main_u_expected_windows_pe_machines(const char *);
extern int main_u_parse_pe_machine(const char *);
extern int main_u_pe_machine_or_none(const char *);
extern int main_u_desktop_exe_integrity_error(const char *);
extern int main_u_desktop_backup_unpacked_dir(const char *);
extern int main_u_rollback_desktop_from_backup(const char *);
extern int main_u_ensure_desktop_exe_launchable(const char *);
extern int main_u_purge_electron_build_cache(const char *);
extern int main_u_redownload_electron_dist(const char *);
extern int main_u_stop_desktop_processes_locking_build(const char *);
extern int main_u_desktop_macos_relaunchable_fixup(const char *);
extern int main_u_force_adhoc_macos_signing(const char *);
extern int main_u_desktop_linux_needs_no_sandbox(const char *);
extern int main_u_desktop_linux_sandbox_helper_is_regular_file(const char *);
extern int main_u_desktop_linux_sandbox_fixup(const char *);
extern int main_u_desktop_launch_options(const char *);
extern int main_cmd_gui(const char *);
extern int main_u_find_stale_dashboard_pids(const char *);
extern int main_u_print_curator_first_run_notice(const char *);
extern int main_u_print_fts_optimize_available_notice(const char *);
extern int main_u_print_curator_recent_run_notice(const char *);
extern int main_u_restart_managed_dashboard_service(const char *);
extern int main_u_kill_stale_dashboard_processes(const char *);
extern int main_u_update_via_zip(const char *);
extern int main_u_stash_local_changes_if_needed(const char *);
extern int main_u_resolve_stash_selector(const char *);
extern int main_u_print_stash_cleanup_guidance(const char *);
extern int main_u_stash_apply_failed_only_on_existing_untracked(const char *);
extern int main_u_restore_stashed_changes(const char *);
extern int main_u_discard_stashed_changes(const char *);
extern int main_u_get_origin_url(const char *);
extern int main_u_is_fork(const char *);
extern int main_u_has_upstream_remote(const char *);
extern int main_u_add_upstream_remote(const char *);
extern int main_u_count_commits_between(const char *);
extern int main_u_should_skip_upstream_prompt(const char *);
extern int main_u_mark_skip_upstream_prompt(const char *);
extern int main_u_sync_fork_with_upstream(const char *);
extern int main_u_sync_with_upstream_if_needed(const char *);
extern int main_u_invalidate_update_cache(const char *);
extern int main_u_load_installable_optional_extras(const char *);
extern int main_u_lazy_refresh_marker_path(const char *);
extern int main_u_write_marker_file(const char *);
extern int main_u_clear_marker_file(const char *);
extern int main_u_write_update_incomplete_marker(const char *);
extern int main_u_clear_update_incomplete_marker(const char *);
extern int main_u_write_lazy_refresh_incomplete_marker(const char *);
extern int main_u_clear_lazy_refresh_incomplete_marker(const char *);
extern int main_u_recover_from_interrupted_install(const char *);
extern int main_u_recover_lazy_refresh_marker_locked(const char *);
extern int main_u_recover_core_update_marker_locked(const char *);
extern int main_u_windows_running_hermes_launcher_locked(const char *);
extern int main_u_default_venv_install_target(const char *);
extern int main_u_run_install_with_heartbeat(const char *);
extern int main_u_venv_scripts_dir(const char *);
extern int main_u_hermes_exe_shims(const char *);
extern int main_u_detect_concurrent_hermes_instances(const char *);
extern int main_u_format_concurrent_instances_message(const char *);
extern int main_u_quarantine_running_hermes_exe(const char *);
extern int main_u_schedule_replace_on_reboot(const char *);
extern int main_u_restore_quarantined_exes(const char *);
extern int main_u_run_quarantined_install(const char *);
extern int main_u_cleanup_quarantined_exes(const char *);
extern int main_u_run_package_only_install(const char *);
extern int main_u_lazy_refresh_repair_specs(const char *);
extern int main_u_upgrade_pip_before_lazy_refresh(const char *);
extern int main_u_detect_broken_lazy_refresh_imports(const char *);
extern int main_u_repair_broken_lazy_refresh_imports(const char *);
extern int main_u_repair_venv_via_import_probes(const char *);
extern int main_u_refresh_active_lazy_features(const char *);
extern int main_u_install_python_dependencies_with_optional_fallback(const char *);
extern int main_u_load_console_script_names(const char *);
extern int main_u_verify_console_scripts_installed(const char *);
extern int main_u_verify_core_dependencies_installed(const char *);
extern int main_u_resolve_install_target_python(const char *);
extern int main_u_install_psutil_android_compat(const char *);
extern int main_u_ensure_uv_for_termux(const char *);
extern int main_u_npm_manifest_paths(const char *);
extern int main_u_npm_manifests_digest(const char *);
extern int main_u_npm_lockfile_changed(const char *);
extern int main_u_record_npm_lockfile_hash(const char *);
extern int main_u_is_windows_npm_path(const char *);
extern int main_u_resolve_node_runtime_npm(const char *);
extern int main_u_update_node_dependencies(const char *);
extern int main_u__getattr___2(const char *);
extern int main_u_install_hangup_protection(const char *);
extern int main_u_log_only_write(const char *);
extern int main_u_run_logged_subprocess(const char *);
extern int main_u_finalize_update_output(const char *);
extern int main_u_resolve_update_branch(const char *);
extern int main_u_cmd_update_check(const char *);
extern int main_u_ensure_fhs_path_guard(const char *);
extern int main_u_size_delta_label(const char *);
extern int main_u_resolve_pre_update_backup_mode(const char *);
extern int main_u_run_pre_update_backup(const char *);
extern int main_u_write_update_planned_stop_marker(const char *);
extern int main_u_wait_for_windows_update_gateway_exit(const char *);
extern int main_u_venv_core_imports_healthy(const char *);
extern int main_u_detect_venv_python_processes(const char *);
extern int main_u_format_venv_python_holders_message(const char *);
extern int main_u_pause_windows_gateways_for_update(const char *);
extern int main_u_cold_start_windows_gateway_after_update(const char *);
extern int main_u_for_each_systemd_gateway_unit(const char *);
extern int main_u_warn_incomplete_gateway_fleet_restart(const char *);
extern int main_u_resume_windows_gateways_after_update(const char *);
extern int main_u_discard_lockfile_churn(const char *);
extern int main_u_cmd_update_impl(const char *);
extern int main_u_render_distribution_plan(const char *);
extern int main_u_report_dashboard_status(const char *);
extern int main_u_dashboard_listening(const char *);
extern int main_u_maybe_setup_dashboard_auth_interactively(const char *);
extern int main_u_read_ssh_session_token_file(const char *);
extern int main_u_is_electron_packaged_web_dist(const char *);
extern int main_cmd_dashboard_register(const char *);
extern int main_cmd_gateway_enroll(const char *);
extern int main_cmd_completion(const char *);
extern int main_cmd_console(const char *);
extern int main_u_plugin_cli_discovery_needed(const char *);
extern int main_u_command_has_dedicated_mcp_startup(const char *);
extern int main_u_should_background_mcp_startup(const char *);
extern int main_u_prepare_agent_startup(const char *);
extern int main_u_apply_safe_mode(const char *);
extern int main_u_set_chat_arg_defaults(const char *);
extern int main_u_try_termux_fast_cli_launch(const char *);
extern int main_u_try_termux_fast_tui_launch(const char *);
extern int main_cmd_acp(const char *);
extern int main_cmd_pairing(const char *);
extern int main_cmd_claw(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_main_u_exit_after_oneshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_exit_after_oneshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_exit_after_oneshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_cleanup_oneshot_runtime(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_cleanup_oneshot_runtime(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_cleanup_oneshot_runtime"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_and_exit_oneshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_and_exit_oneshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_and_exit_oneshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_set_process_title(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_set_process_title(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_set_process_title"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_config_default_interface_early(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_config_default_interface_early(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_config_default_interface_early"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_wants_tui_early(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_wants_tui_early(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_wants_tui_early"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_suppress_mouse_residue_early(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_suppress_mouse_residue_early(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_suppress_mouse_residue_early"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_termux_startup_environment_fast(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_termux_startup_environment_fast(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_termux_startup_environment_fast"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_termux_fast_version_argv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_termux_fast_version_argv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_termux_fast_version_argv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_read_openai_version_fast(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_read_openai_version_fast(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_read_openai_version_fast"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_fast_version_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_fast_version_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_fast_version_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_try_termux_ultrafast_version(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_try_termux_ultrafast_version(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_try_termux_ultrafast_version"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_require_tty(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_require_tty(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_require_tty"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_apply_profile_override(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_apply_profile_override(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_apply_profile_override"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_termux_startup_environment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_termux_startup_environment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_termux_startup_environment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_termux_bundled_skills_fingerprint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_termux_bundled_skills_fingerprint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_termux_bundled_skills_fingerprint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_termux_bundled_skills_stamp_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_termux_bundled_skills_stamp_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_termux_bundled_skills_stamp_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_termux_bundled_skills_sync_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_termux_bundled_skills_sync_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_termux_bundled_skills_sync_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_mark_termux_bundled_skills_synced(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_mark_termux_bundled_skills_synced(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_mark_termux_bundled_skills_synced"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_sync_bundled_skills_for_startup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_sync_bundled_skills_for_startup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_sync_bundled_skills_for_startup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_termux_should_prefetch_update_check(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_termux_should_prefetch_update_check(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_termux_should_prefetch_update_check"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_has_any_provider_configured(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_has_any_provider_configured(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_has_any_provider_configured"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_session_browse_picker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_session_browse_picker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_session_browse_picker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_last_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_last_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_last_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_probe_container(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_probe_container(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_probe_container"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_exec_in_container(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_exec_in_container(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_exec_in_container"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_session_by_name_or_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_session_by_name_or_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_session_by_name_or_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_tui_exit_summary(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_tui_exit_summary(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_tui_exit_summary"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_termux_workspace_install_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_termux_workspace_install_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_termux_workspace_install_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_tui_need_npm_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_tui_need_npm_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_tui_need_npm_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_iter_tui_build_inputs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_iter_tui_build_inputs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_iter_tui_build_inputs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_tui_need_rebuild(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_tui_need_rebuild(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_tui_need_rebuild"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_ensure_tui_node(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_ensure_tui_node(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_ensure_tui_node"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_find_bundled_tui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_find_bundled_tui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_find_bundled_tui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_make_tui_argv(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_make_tui_argv(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_make_tui_argv"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_normalize_tui_toolsets(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_normalize_tui_toolsets(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_normalize_tui_toolsets"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_tui_heap_mb(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_tui_heap_mb(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_tui_heap_mb"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_safe_tui_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_safe_tui_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_safe_tui_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_apply_tui_python_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_apply_tui_python_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_apply_tui_python_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_launch_tui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_launch_tui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_launch_tui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_pin_kanban_board_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_pin_kanban_board_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_pin_kanban_board_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_sync_bundled_skills_quietly(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_sync_bundled_skills_quietly(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_sync_bundled_skills_quietly"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_use_tui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_use_tui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_use_tui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_chat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_chat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_chat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_proxy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_proxy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_proxy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_whatsapp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_whatsapp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_whatsapp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_whatsapp_cloud(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_whatsapp_cloud(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_whatsapp_cloud"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_profile_api_key_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_profile_api_key_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_profile_api_key_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_select_provider_and_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_select_provider_and_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_select_provider_and_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_clear_stale_openai_base_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_clear_stale_openai_base_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_clear_stale_openai_base_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_all_aux_tasks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_all_aux_tasks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_all_aux_tasks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_format_aux_current(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_format_aux_current(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_format_aux_current"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_save_aux_choice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_save_aux_choice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_save_aux_choice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_reset_aux_to_auto(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_reset_aux_to_auto(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_reset_aux_to_auto"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_aux_config_menu(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_aux_config_menu(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_aux_config_menu"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_aux_select_for_task(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_aux_select_for_task(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_aux_select_for_task"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_aux_flow_provider_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_aux_flow_provider_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_aux_flow_provider_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_aux_flow_custom_endpoint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_aux_flow_custom_endpoint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_aux_flow_custom_endpoint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_prompt_provider_choice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_prompt_provider_choice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_prompt_provider_choice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_prompt_custom_api_mode_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_prompt_custom_api_mode_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_prompt_custom_api_mode_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_custom_provider_api_key_config_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_custom_provider_api_key_config_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_custom_provider_api_key_config_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_custom_provider_base_url_config_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_custom_provider_base_url_config_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_custom_provider_base_url_config_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_save_custom_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_save_custom_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_save_custom_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_remove_custom_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_remove_custom_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_remove_custom_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u__getattr__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u__getattr__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u__getattr__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_set_reasoning_effort(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_set_reasoning_effort(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_set_reasoning_effort"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_prompt_reasoning_effort_selection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_prompt_reasoning_effort_selection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_prompt_reasoning_effort_selection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_anthropic_oauth_flow(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_anthropic_oauth_flow(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_anthropic_oauth_flow"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_login(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_login(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_login"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_logout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_logout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_logout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_slack(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_slack(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_slack"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_project(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_project(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_project"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_hooks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_hooks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_hooks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_security(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_security(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_security"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_import(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_import(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_import"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_version_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_version_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_version_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_version(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_version(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_version"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_clear_bytecode_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_clear_bytecode_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_clear_bytecode_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_capture_head_sha(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_capture_head_sha(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_capture_head_sha"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_validate_critical_files_syntax(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_validate_critical_files_syntax(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_validate_critical_files_syntax"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_gateway_prompt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_gateway_prompt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_gateway_prompt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_web_ui_build_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_web_ui_build_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_web_ui_build_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_compute_web_ui_content_hash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_compute_web_ui_content_hash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_compute_web_ui_content_hash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_web_ui_stamp_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_web_ui_stamp_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_web_ui_stamp_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_write_web_ui_build_stamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_write_web_ui_build_stamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_write_web_ui_build_stamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_with_idle_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_with_idle_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_with_idle_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_nixos_build_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_nixos_build_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_nixos_build_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_npm_install_deterministic(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_npm_install_deterministic(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_npm_install_deterministic"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_build_web_ui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_build_web_ui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_build_web_ui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_do_build_web_ui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_do_build_web_ui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_do_build_web_ui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_dist_exists(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_dist_exists(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_dist_exists"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_compute_desktop_content_hash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_compute_desktop_content_hash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_compute_desktop_content_hash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_stamp_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_stamp_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_stamp_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_build_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_build_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_build_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_write_desktop_build_stamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_write_desktop_build_stamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_write_desktop_build_stamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_packaged_executable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_packaged_executable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_packaged_executable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_expected_windows_pe_machines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_expected_windows_pe_machines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_expected_windows_pe_machines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_parse_pe_machine(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_parse_pe_machine(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_parse_pe_machine"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_pe_machine_or_none(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_pe_machine_or_none(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_pe_machine_or_none"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_exe_integrity_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_exe_integrity_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_exe_integrity_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_backup_unpacked_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_backup_unpacked_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_backup_unpacked_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_rollback_desktop_from_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_rollback_desktop_from_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_rollback_desktop_from_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_ensure_desktop_exe_launchable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_ensure_desktop_exe_launchable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_ensure_desktop_exe_launchable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_purge_electron_build_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_purge_electron_build_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_purge_electron_build_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_redownload_electron_dist(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_redownload_electron_dist(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_redownload_electron_dist"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_stop_desktop_processes_locking_build(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_stop_desktop_processes_locking_build(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_stop_desktop_processes_locking_build"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_macos_relaunchable_fixup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_macos_relaunchable_fixup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_macos_relaunchable_fixup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_force_adhoc_macos_signing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_force_adhoc_macos_signing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_force_adhoc_macos_signing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_linux_needs_no_sandbox(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_linux_needs_no_sandbox(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_linux_needs_no_sandbox"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_linux_sandbox_helper_is_regular_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_linux_sandbox_helper_is_regular_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_linux_sandbox_helper_is_regular_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_linux_sandbox_fixup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_linux_sandbox_fixup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_linux_sandbox_fixup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_desktop_launch_options(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_desktop_launch_options(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_desktop_launch_options"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_gui(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_gui(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_gui"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_find_stale_dashboard_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_find_stale_dashboard_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_find_stale_dashboard_pids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_curator_first_run_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_curator_first_run_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_curator_first_run_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_fts_optimize_available_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_fts_optimize_available_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_fts_optimize_available_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_curator_recent_run_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_curator_recent_run_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_curator_recent_run_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_restart_managed_dashboard_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_restart_managed_dashboard_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_restart_managed_dashboard_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_kill_stale_dashboard_processes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_kill_stale_dashboard_processes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_kill_stale_dashboard_processes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_update_via_zip(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_update_via_zip(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_update_via_zip"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_stash_local_changes_if_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_stash_local_changes_if_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_stash_local_changes_if_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_stash_selector(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_stash_selector(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_stash_selector"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_print_stash_cleanup_guidance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_print_stash_cleanup_guidance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_print_stash_cleanup_guidance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_stash_apply_failed_only_on_existing_untracked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_stash_apply_failed_only_on_existing_untracked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_stash_apply_failed_only_on_existing_untracked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_restore_stashed_changes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_restore_stashed_changes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_restore_stashed_changes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_discard_stashed_changes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_discard_stashed_changes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_discard_stashed_changes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_get_origin_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_get_origin_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_get_origin_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_fork(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_fork(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_fork"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_has_upstream_remote(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_has_upstream_remote(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_has_upstream_remote"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_add_upstream_remote(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_add_upstream_remote(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_add_upstream_remote"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_count_commits_between(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_count_commits_between(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_count_commits_between"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_should_skip_upstream_prompt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_should_skip_upstream_prompt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_should_skip_upstream_prompt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_mark_skip_upstream_prompt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_mark_skip_upstream_prompt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_mark_skip_upstream_prompt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_sync_fork_with_upstream(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_sync_fork_with_upstream(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_sync_fork_with_upstream"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_sync_with_upstream_if_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_sync_with_upstream_if_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_sync_with_upstream_if_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_invalidate_update_cache(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_invalidate_update_cache(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_invalidate_update_cache"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_load_installable_optional_extras(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_load_installable_optional_extras(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_load_installable_optional_extras"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_lazy_refresh_marker_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_lazy_refresh_marker_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_lazy_refresh_marker_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_write_marker_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_write_marker_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_write_marker_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_clear_marker_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_clear_marker_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_clear_marker_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_write_update_incomplete_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_write_update_incomplete_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_write_update_incomplete_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_clear_update_incomplete_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_clear_update_incomplete_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_clear_update_incomplete_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_write_lazy_refresh_incomplete_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_write_lazy_refresh_incomplete_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_write_lazy_refresh_incomplete_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_clear_lazy_refresh_incomplete_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_clear_lazy_refresh_incomplete_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_clear_lazy_refresh_incomplete_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_recover_from_interrupted_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_recover_from_interrupted_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_recover_from_interrupted_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_recover_lazy_refresh_marker_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_recover_lazy_refresh_marker_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_recover_lazy_refresh_marker_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_recover_core_update_marker_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_recover_core_update_marker_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_recover_core_update_marker_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_windows_running_hermes_launcher_locked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_windows_running_hermes_launcher_locked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_windows_running_hermes_launcher_locked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_default_venv_install_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_default_venv_install_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_default_venv_install_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_install_with_heartbeat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_install_with_heartbeat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_install_with_heartbeat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_venv_scripts_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_venv_scripts_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_venv_scripts_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_hermes_exe_shims(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_hermes_exe_shims(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_hermes_exe_shims"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_detect_concurrent_hermes_instances(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_detect_concurrent_hermes_instances(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_detect_concurrent_hermes_instances"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_format_concurrent_instances_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_format_concurrent_instances_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_format_concurrent_instances_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_quarantine_running_hermes_exe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_quarantine_running_hermes_exe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_quarantine_running_hermes_exe"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_schedule_replace_on_reboot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_schedule_replace_on_reboot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_schedule_replace_on_reboot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_restore_quarantined_exes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_restore_quarantined_exes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_restore_quarantined_exes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_quarantined_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_quarantined_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_quarantined_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_cleanup_quarantined_exes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_cleanup_quarantined_exes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_cleanup_quarantined_exes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_package_only_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_package_only_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_package_only_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_lazy_refresh_repair_specs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_lazy_refresh_repair_specs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_lazy_refresh_repair_specs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_upgrade_pip_before_lazy_refresh(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_upgrade_pip_before_lazy_refresh(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_upgrade_pip_before_lazy_refresh"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_detect_broken_lazy_refresh_imports(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_detect_broken_lazy_refresh_imports(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_detect_broken_lazy_refresh_imports"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_repair_broken_lazy_refresh_imports(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_repair_broken_lazy_refresh_imports(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_repair_broken_lazy_refresh_imports"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_repair_venv_via_import_probes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_repair_venv_via_import_probes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_repair_venv_via_import_probes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_refresh_active_lazy_features(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_refresh_active_lazy_features(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_refresh_active_lazy_features"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_install_python_dependencies_with_optional_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_install_python_dependencies_with_optional_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_install_python_dependencies_with_optional_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_load_console_script_names(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_load_console_script_names(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_load_console_script_names"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_verify_console_scripts_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_verify_console_scripts_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_verify_console_scripts_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_verify_core_dependencies_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_verify_core_dependencies_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_verify_core_dependencies_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_install_target_python(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_install_target_python(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_install_target_python"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_install_psutil_android_compat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_install_psutil_android_compat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_install_psutil_android_compat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_ensure_uv_for_termux(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_ensure_uv_for_termux(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_ensure_uv_for_termux"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_npm_manifest_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_npm_manifest_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_npm_manifest_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_npm_manifests_digest(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_npm_manifests_digest(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_npm_manifests_digest"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_npm_lockfile_changed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_npm_lockfile_changed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_npm_lockfile_changed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_record_npm_lockfile_hash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_record_npm_lockfile_hash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_record_npm_lockfile_hash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_windows_npm_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_windows_npm_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_windows_npm_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_node_runtime_npm(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_node_runtime_npm(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_node_runtime_npm"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_update_node_dependencies(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_update_node_dependencies(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_update_node_dependencies"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u__getattr___2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u__getattr___2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u__getattr___2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_install_hangup_protection(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_install_hangup_protection(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_install_hangup_protection"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_log_only_write(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_log_only_write(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_log_only_write"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_logged_subprocess(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_logged_subprocess(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_logged_subprocess"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_finalize_update_output(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_finalize_update_output(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_finalize_update_output"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_update_branch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_update_branch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_update_branch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_cmd_update_check(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_cmd_update_check(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_cmd_update_check"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_ensure_fhs_path_guard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_ensure_fhs_path_guard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_ensure_fhs_path_guard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_size_delta_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_size_delta_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_size_delta_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resolve_pre_update_backup_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resolve_pre_update_backup_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resolve_pre_update_backup_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_run_pre_update_backup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_run_pre_update_backup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_run_pre_update_backup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_write_update_planned_stop_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_write_update_planned_stop_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_write_update_planned_stop_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_wait_for_windows_update_gateway_exit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_wait_for_windows_update_gateway_exit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_wait_for_windows_update_gateway_exit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_venv_core_imports_healthy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_venv_core_imports_healthy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_venv_core_imports_healthy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_detect_venv_python_processes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_detect_venv_python_processes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_detect_venv_python_processes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_format_venv_python_holders_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_format_venv_python_holders_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_format_venv_python_holders_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_pause_windows_gateways_for_update(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_pause_windows_gateways_for_update(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_pause_windows_gateways_for_update"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_cold_start_windows_gateway_after_update(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_cold_start_windows_gateway_after_update(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_cold_start_windows_gateway_after_update"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_for_each_systemd_gateway_unit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_for_each_systemd_gateway_unit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_for_each_systemd_gateway_unit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_warn_incomplete_gateway_fleet_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_warn_incomplete_gateway_fleet_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_warn_incomplete_gateway_fleet_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_resume_windows_gateways_after_update(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_resume_windows_gateways_after_update(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_resume_windows_gateways_after_update"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_discard_lockfile_churn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_discard_lockfile_churn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_discard_lockfile_churn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_cmd_update_impl(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_cmd_update_impl(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_cmd_update_impl"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_render_distribution_plan(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_render_distribution_plan(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_render_distribution_plan"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_report_dashboard_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_report_dashboard_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_report_dashboard_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_dashboard_listening(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_dashboard_listening(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_dashboard_listening"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_maybe_setup_dashboard_auth_interactively(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_maybe_setup_dashboard_auth_interactively(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_maybe_setup_dashboard_auth_interactively"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_read_ssh_session_token_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_read_ssh_session_token_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_read_ssh_session_token_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_is_electron_packaged_web_dist(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_is_electron_packaged_web_dist(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_is_electron_packaged_web_dist"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_dashboard_register(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_dashboard_register(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_dashboard_register"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_gateway_enroll(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_gateway_enroll(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_gateway_enroll"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_completion(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_completion(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_completion"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_console(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_console(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_console"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_plugin_cli_discovery_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_plugin_cli_discovery_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_plugin_cli_discovery_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_command_has_dedicated_mcp_startup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_command_has_dedicated_mcp_startup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_command_has_dedicated_mcp_startup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_should_background_mcp_startup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_should_background_mcp_startup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_should_background_mcp_startup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_prepare_agent_startup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_prepare_agent_startup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_prepare_agent_startup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_apply_safe_mode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_apply_safe_mode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_apply_safe_mode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_set_chat_arg_defaults(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_set_chat_arg_defaults(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_set_chat_arg_defaults"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_try_termux_fast_cli_launch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_try_termux_fast_cli_launch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_try_termux_fast_cli_launch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_u_try_termux_fast_tui_launch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_u_try_termux_fast_tui_launch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_u_try_termux_fast_tui_launch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_acp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_acp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_acp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_pairing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_pairing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_pairing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_main_cmd_claw(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)main_cmd_claw(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("main_cmd_claw"));
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
        if (strcmp(op, "main_u_exit_after_oneshot") == 0) o = emit_main_u_exit_after_oneshot(c);
        if (strcmp(op, "main_u_cleanup_oneshot_runtime") == 0) o = emit_main_u_cleanup_oneshot_runtime(c);
        if (strcmp(op, "main_u_run_and_exit_oneshot") == 0) o = emit_main_u_run_and_exit_oneshot(c);
        if (strcmp(op, "main_u_set_process_title") == 0) o = emit_main_u_set_process_title(c);
        if (strcmp(op, "main_u_config_default_interface_early") == 0) o = emit_main_u_config_default_interface_early(c);
        if (strcmp(op, "main_u_wants_tui_early") == 0) o = emit_main_u_wants_tui_early(c);
        if (strcmp(op, "main_u_suppress_mouse_residue_early") == 0) o = emit_main_u_suppress_mouse_residue_early(c);
        if (strcmp(op, "main_u_is_termux_startup_environment_fast") == 0) o = emit_main_u_is_termux_startup_environment_fast(c);
        if (strcmp(op, "main_u_is_termux_fast_version_argv") == 0) o = emit_main_u_is_termux_fast_version_argv(c);
        if (strcmp(op, "main_u_read_openai_version_fast") == 0) o = emit_main_u_read_openai_version_fast(c);
        if (strcmp(op, "main_u_print_fast_version_info") == 0) o = emit_main_u_print_fast_version_info(c);
        if (strcmp(op, "main_u_try_termux_ultrafast_version") == 0) o = emit_main_u_try_termux_ultrafast_version(c);
        if (strcmp(op, "main_u_require_tty") == 0) o = emit_main_u_require_tty(c);
        if (strcmp(op, "main_u_apply_profile_override") == 0) o = emit_main_u_apply_profile_override(c);
        if (strcmp(op, "main_u_is_termux_startup_environment") == 0) o = emit_main_u_is_termux_startup_environment(c);
        if (strcmp(op, "main_u_termux_bundled_skills_fingerprint") == 0) o = emit_main_u_termux_bundled_skills_fingerprint(c);
        if (strcmp(op, "main_u_termux_bundled_skills_stamp_path") == 0) o = emit_main_u_termux_bundled_skills_stamp_path(c);
        if (strcmp(op, "main_u_termux_bundled_skills_sync_needed") == 0) o = emit_main_u_termux_bundled_skills_sync_needed(c);
        if (strcmp(op, "main_u_mark_termux_bundled_skills_synced") == 0) o = emit_main_u_mark_termux_bundled_skills_synced(c);
        if (strcmp(op, "main_u_sync_bundled_skills_for_startup") == 0) o = emit_main_u_sync_bundled_skills_for_startup(c);
        if (strcmp(op, "main_u_termux_should_prefetch_update_check") == 0) o = emit_main_u_termux_should_prefetch_update_check(c);
        if (strcmp(op, "main_u_has_any_provider_configured") == 0) o = emit_main_u_has_any_provider_configured(c);
        if (strcmp(op, "main_u_session_browse_picker") == 0) o = emit_main_u_session_browse_picker(c);
        if (strcmp(op, "main_u_resolve_last_session") == 0) o = emit_main_u_resolve_last_session(c);
        if (strcmp(op, "main_u_probe_container") == 0) o = emit_main_u_probe_container(c);
        if (strcmp(op, "main_u_exec_in_container") == 0) o = emit_main_u_exec_in_container(c);
        if (strcmp(op, "main_u_resolve_session_by_name_or_id") == 0) o = emit_main_u_resolve_session_by_name_or_id(c);
        if (strcmp(op, "main_u_print_tui_exit_summary") == 0) o = emit_main_u_print_tui_exit_summary(c);
        if (strcmp(op, "main_u_termux_workspace_install_context") == 0) o = emit_main_u_termux_workspace_install_context(c);
        if (strcmp(op, "main_u_tui_need_npm_install") == 0) o = emit_main_u_tui_need_npm_install(c);
        if (strcmp(op, "main_u_iter_tui_build_inputs") == 0) o = emit_main_u_iter_tui_build_inputs(c);
        if (strcmp(op, "main_u_tui_need_rebuild") == 0) o = emit_main_u_tui_need_rebuild(c);
        if (strcmp(op, "main_u_ensure_tui_node") == 0) o = emit_main_u_ensure_tui_node(c);
        if (strcmp(op, "main_u_find_bundled_tui") == 0) o = emit_main_u_find_bundled_tui(c);
        if (strcmp(op, "main_u_make_tui_argv") == 0) o = emit_main_u_make_tui_argv(c);
        if (strcmp(op, "main_u_normalize_tui_toolsets") == 0) o = emit_main_u_normalize_tui_toolsets(c);
        if (strcmp(op, "main_u_resolve_tui_heap_mb") == 0) o = emit_main_u_resolve_tui_heap_mb(c);
        if (strcmp(op, "main_u_safe_tui_cwd") == 0) o = emit_main_u_safe_tui_cwd(c);
        if (strcmp(op, "main_u_apply_tui_python_env") == 0) o = emit_main_u_apply_tui_python_env(c);
        if (strcmp(op, "main_u_launch_tui") == 0) o = emit_main_u_launch_tui(c);
        if (strcmp(op, "main_u_pin_kanban_board_env") == 0) o = emit_main_u_pin_kanban_board_env(c);
        if (strcmp(op, "main_u_sync_bundled_skills_quietly") == 0) o = emit_main_u_sync_bundled_skills_quietly(c);
        if (strcmp(op, "main_u_resolve_use_tui") == 0) o = emit_main_u_resolve_use_tui(c);
        if (strcmp(op, "main_cmd_chat") == 0) o = emit_main_cmd_chat(c);
        if (strcmp(op, "main_cmd_proxy") == 0) o = emit_main_cmd_proxy(c);
        if (strcmp(op, "main_cmd_whatsapp") == 0) o = emit_main_cmd_whatsapp(c);
        if (strcmp(op, "main_cmd_whatsapp_cloud") == 0) o = emit_main_cmd_whatsapp_cloud(c);
        if (strcmp(op, "main_u_is_profile_api_key_provider") == 0) o = emit_main_u_is_profile_api_key_provider(c);
        if (strcmp(op, "main_select_provider_and_model") == 0) o = emit_main_select_provider_and_model(c);
        if (strcmp(op, "main_u_clear_stale_openai_base_url") == 0) o = emit_main_u_clear_stale_openai_base_url(c);
        if (strcmp(op, "main_u_all_aux_tasks") == 0) o = emit_main_u_all_aux_tasks(c);
        if (strcmp(op, "main_u_format_aux_current") == 0) o = emit_main_u_format_aux_current(c);
        if (strcmp(op, "main_u_save_aux_choice") == 0) o = emit_main_u_save_aux_choice(c);
        if (strcmp(op, "main_u_reset_aux_to_auto") == 0) o = emit_main_u_reset_aux_to_auto(c);
        if (strcmp(op, "main_u_aux_config_menu") == 0) o = emit_main_u_aux_config_menu(c);
        if (strcmp(op, "main_u_aux_select_for_task") == 0) o = emit_main_u_aux_select_for_task(c);
        if (strcmp(op, "main_u_aux_flow_provider_model") == 0) o = emit_main_u_aux_flow_provider_model(c);
        if (strcmp(op, "main_u_aux_flow_custom_endpoint") == 0) o = emit_main_u_aux_flow_custom_endpoint(c);
        if (strcmp(op, "main_u_prompt_provider_choice") == 0) o = emit_main_u_prompt_provider_choice(c);
        if (strcmp(op, "main_u_prompt_custom_api_mode_selection") == 0) o = emit_main_u_prompt_custom_api_mode_selection(c);
        if (strcmp(op, "main_u_custom_provider_api_key_config_value") == 0) o = emit_main_u_custom_provider_api_key_config_value(c);
        if (strcmp(op, "main_u_custom_provider_base_url_config_value") == 0) o = emit_main_u_custom_provider_base_url_config_value(c);
        if (strcmp(op, "main_u_save_custom_provider") == 0) o = emit_main_u_save_custom_provider(c);
        if (strcmp(op, "main_u_remove_custom_provider") == 0) o = emit_main_u_remove_custom_provider(c);
        if (strcmp(op, "main_u__getattr__") == 0) o = emit_main_u__getattr__(c);
        if (strcmp(op, "main_u_set_reasoning_effort") == 0) o = emit_main_u_set_reasoning_effort(c);
        if (strcmp(op, "main_u_prompt_reasoning_effort_selection") == 0) o = emit_main_u_prompt_reasoning_effort_selection(c);
        if (strcmp(op, "main_u_run_anthropic_oauth_flow") == 0) o = emit_main_u_run_anthropic_oauth_flow(c);
        if (strcmp(op, "main_cmd_login") == 0) o = emit_main_cmd_login(c);
        if (strcmp(op, "main_cmd_logout") == 0) o = emit_main_cmd_logout(c);
        if (strcmp(op, "main_cmd_slack") == 0) o = emit_main_cmd_slack(c);
        if (strcmp(op, "main_cmd_project") == 0) o = emit_main_cmd_project(c);
        if (strcmp(op, "main_cmd_hooks") == 0) o = emit_main_cmd_hooks(c);
        if (strcmp(op, "main_cmd_security") == 0) o = emit_main_cmd_security(c);
        if (strcmp(op, "main_cmd_import") == 0) o = emit_main_cmd_import(c);
        if (strcmp(op, "main_u_print_version_info") == 0) o = emit_main_u_print_version_info(c);
        if (strcmp(op, "main_cmd_version") == 0) o = emit_main_cmd_version(c);
        if (strcmp(op, "main_u_clear_bytecode_cache") == 0) o = emit_main_u_clear_bytecode_cache(c);
        if (strcmp(op, "main_u_capture_head_sha") == 0) o = emit_main_u_capture_head_sha(c);
        if (strcmp(op, "main_u_validate_critical_files_syntax") == 0) o = emit_main_u_validate_critical_files_syntax(c);
        if (strcmp(op, "main_u_gateway_prompt") == 0) o = emit_main_u_gateway_prompt(c);
        if (strcmp(op, "main_u_web_ui_build_needed") == 0) o = emit_main_u_web_ui_build_needed(c);
        if (strcmp(op, "main_u_compute_web_ui_content_hash") == 0) o = emit_main_u_compute_web_ui_content_hash(c);
        if (strcmp(op, "main_u_web_ui_stamp_path") == 0) o = emit_main_u_web_ui_stamp_path(c);
        if (strcmp(op, "main_u_write_web_ui_build_stamp") == 0) o = emit_main_u_write_web_ui_build_stamp(c);
        if (strcmp(op, "main_u_run_with_idle_timeout") == 0) o = emit_main_u_run_with_idle_timeout(c);
        if (strcmp(op, "main_u_nixos_build_env") == 0) o = emit_main_u_nixos_build_env(c);
        if (strcmp(op, "main_u_run_npm_install_deterministic") == 0) o = emit_main_u_run_npm_install_deterministic(c);
        if (strcmp(op, "main_u_build_web_ui") == 0) o = emit_main_u_build_web_ui(c);
        if (strcmp(op, "main_u_do_build_web_ui") == 0) o = emit_main_u_do_build_web_ui(c);
        if (strcmp(op, "main_u_desktop_dist_exists") == 0) o = emit_main_u_desktop_dist_exists(c);
        if (strcmp(op, "main_u_compute_desktop_content_hash") == 0) o = emit_main_u_compute_desktop_content_hash(c);
        if (strcmp(op, "main_u_desktop_stamp_path") == 0) o = emit_main_u_desktop_stamp_path(c);
        if (strcmp(op, "main_u_desktop_build_needed") == 0) o = emit_main_u_desktop_build_needed(c);
        if (strcmp(op, "main_u_write_desktop_build_stamp") == 0) o = emit_main_u_write_desktop_build_stamp(c);
        if (strcmp(op, "main_u_desktop_packaged_executable") == 0) o = emit_main_u_desktop_packaged_executable(c);
        if (strcmp(op, "main_u_expected_windows_pe_machines") == 0) o = emit_main_u_expected_windows_pe_machines(c);
        if (strcmp(op, "main_u_parse_pe_machine") == 0) o = emit_main_u_parse_pe_machine(c);
        if (strcmp(op, "main_u_pe_machine_or_none") == 0) o = emit_main_u_pe_machine_or_none(c);
        if (strcmp(op, "main_u_desktop_exe_integrity_error") == 0) o = emit_main_u_desktop_exe_integrity_error(c);
        if (strcmp(op, "main_u_desktop_backup_unpacked_dir") == 0) o = emit_main_u_desktop_backup_unpacked_dir(c);
        if (strcmp(op, "main_u_rollback_desktop_from_backup") == 0) o = emit_main_u_rollback_desktop_from_backup(c);
        if (strcmp(op, "main_u_ensure_desktop_exe_launchable") == 0) o = emit_main_u_ensure_desktop_exe_launchable(c);
        if (strcmp(op, "main_u_purge_electron_build_cache") == 0) o = emit_main_u_purge_electron_build_cache(c);
        if (strcmp(op, "main_u_redownload_electron_dist") == 0) o = emit_main_u_redownload_electron_dist(c);
        if (strcmp(op, "main_u_stop_desktop_processes_locking_build") == 0) o = emit_main_u_stop_desktop_processes_locking_build(c);
        if (strcmp(op, "main_u_desktop_macos_relaunchable_fixup") == 0) o = emit_main_u_desktop_macos_relaunchable_fixup(c);
        if (strcmp(op, "main_u_force_adhoc_macos_signing") == 0) o = emit_main_u_force_adhoc_macos_signing(c);
        if (strcmp(op, "main_u_desktop_linux_needs_no_sandbox") == 0) o = emit_main_u_desktop_linux_needs_no_sandbox(c);
        if (strcmp(op, "main_u_desktop_linux_sandbox_helper_is_regular_file") == 0) o = emit_main_u_desktop_linux_sandbox_helper_is_regular_file(c);
        if (strcmp(op, "main_u_desktop_linux_sandbox_fixup") == 0) o = emit_main_u_desktop_linux_sandbox_fixup(c);
        if (strcmp(op, "main_u_desktop_launch_options") == 0) o = emit_main_u_desktop_launch_options(c);
        if (strcmp(op, "main_cmd_gui") == 0) o = emit_main_cmd_gui(c);
        if (strcmp(op, "main_u_find_stale_dashboard_pids") == 0) o = emit_main_u_find_stale_dashboard_pids(c);
        if (strcmp(op, "main_u_print_curator_first_run_notice") == 0) o = emit_main_u_print_curator_first_run_notice(c);
        if (strcmp(op, "main_u_print_fts_optimize_available_notice") == 0) o = emit_main_u_print_fts_optimize_available_notice(c);
        if (strcmp(op, "main_u_print_curator_recent_run_notice") == 0) o = emit_main_u_print_curator_recent_run_notice(c);
        if (strcmp(op, "main_u_restart_managed_dashboard_service") == 0) o = emit_main_u_restart_managed_dashboard_service(c);
        if (strcmp(op, "main_u_kill_stale_dashboard_processes") == 0) o = emit_main_u_kill_stale_dashboard_processes(c);
        if (strcmp(op, "main_u_update_via_zip") == 0) o = emit_main_u_update_via_zip(c);
        if (strcmp(op, "main_u_stash_local_changes_if_needed") == 0) o = emit_main_u_stash_local_changes_if_needed(c);
        if (strcmp(op, "main_u_resolve_stash_selector") == 0) o = emit_main_u_resolve_stash_selector(c);
        if (strcmp(op, "main_u_print_stash_cleanup_guidance") == 0) o = emit_main_u_print_stash_cleanup_guidance(c);
        if (strcmp(op, "main_u_stash_apply_failed_only_on_existing_untracked") == 0) o = emit_main_u_stash_apply_failed_only_on_existing_untracked(c);
        if (strcmp(op, "main_u_restore_stashed_changes") == 0) o = emit_main_u_restore_stashed_changes(c);
        if (strcmp(op, "main_u_discard_stashed_changes") == 0) o = emit_main_u_discard_stashed_changes(c);
        if (strcmp(op, "main_u_get_origin_url") == 0) o = emit_main_u_get_origin_url(c);
        if (strcmp(op, "main_u_is_fork") == 0) o = emit_main_u_is_fork(c);
        if (strcmp(op, "main_u_has_upstream_remote") == 0) o = emit_main_u_has_upstream_remote(c);
        if (strcmp(op, "main_u_add_upstream_remote") == 0) o = emit_main_u_add_upstream_remote(c);
        if (strcmp(op, "main_u_count_commits_between") == 0) o = emit_main_u_count_commits_between(c);
        if (strcmp(op, "main_u_should_skip_upstream_prompt") == 0) o = emit_main_u_should_skip_upstream_prompt(c);
        if (strcmp(op, "main_u_mark_skip_upstream_prompt") == 0) o = emit_main_u_mark_skip_upstream_prompt(c);
        if (strcmp(op, "main_u_sync_fork_with_upstream") == 0) o = emit_main_u_sync_fork_with_upstream(c);
        if (strcmp(op, "main_u_sync_with_upstream_if_needed") == 0) o = emit_main_u_sync_with_upstream_if_needed(c);
        if (strcmp(op, "main_u_invalidate_update_cache") == 0) o = emit_main_u_invalidate_update_cache(c);
        if (strcmp(op, "main_u_load_installable_optional_extras") == 0) o = emit_main_u_load_installable_optional_extras(c);
        if (strcmp(op, "main_u_lazy_refresh_marker_path") == 0) o = emit_main_u_lazy_refresh_marker_path(c);
        if (strcmp(op, "main_u_write_marker_file") == 0) o = emit_main_u_write_marker_file(c);
        if (strcmp(op, "main_u_clear_marker_file") == 0) o = emit_main_u_clear_marker_file(c);
        if (strcmp(op, "main_u_write_update_incomplete_marker") == 0) o = emit_main_u_write_update_incomplete_marker(c);
        if (strcmp(op, "main_u_clear_update_incomplete_marker") == 0) o = emit_main_u_clear_update_incomplete_marker(c);
        if (strcmp(op, "main_u_write_lazy_refresh_incomplete_marker") == 0) o = emit_main_u_write_lazy_refresh_incomplete_marker(c);
        if (strcmp(op, "main_u_clear_lazy_refresh_incomplete_marker") == 0) o = emit_main_u_clear_lazy_refresh_incomplete_marker(c);
        if (strcmp(op, "main_u_recover_from_interrupted_install") == 0) o = emit_main_u_recover_from_interrupted_install(c);
        if (strcmp(op, "main_u_recover_lazy_refresh_marker_locked") == 0) o = emit_main_u_recover_lazy_refresh_marker_locked(c);
        if (strcmp(op, "main_u_recover_core_update_marker_locked") == 0) o = emit_main_u_recover_core_update_marker_locked(c);
        if (strcmp(op, "main_u_windows_running_hermes_launcher_locked") == 0) o = emit_main_u_windows_running_hermes_launcher_locked(c);
        if (strcmp(op, "main_u_default_venv_install_target") == 0) o = emit_main_u_default_venv_install_target(c);
        if (strcmp(op, "main_u_run_install_with_heartbeat") == 0) o = emit_main_u_run_install_with_heartbeat(c);
        if (strcmp(op, "main_u_venv_scripts_dir") == 0) o = emit_main_u_venv_scripts_dir(c);
        if (strcmp(op, "main_u_hermes_exe_shims") == 0) o = emit_main_u_hermes_exe_shims(c);
        if (strcmp(op, "main_u_detect_concurrent_hermes_instances") == 0) o = emit_main_u_detect_concurrent_hermes_instances(c);
        if (strcmp(op, "main_u_format_concurrent_instances_message") == 0) o = emit_main_u_format_concurrent_instances_message(c);
        if (strcmp(op, "main_u_quarantine_running_hermes_exe") == 0) o = emit_main_u_quarantine_running_hermes_exe(c);
        if (strcmp(op, "main_u_schedule_replace_on_reboot") == 0) o = emit_main_u_schedule_replace_on_reboot(c);
        if (strcmp(op, "main_u_restore_quarantined_exes") == 0) o = emit_main_u_restore_quarantined_exes(c);
        if (strcmp(op, "main_u_run_quarantined_install") == 0) o = emit_main_u_run_quarantined_install(c);
        if (strcmp(op, "main_u_cleanup_quarantined_exes") == 0) o = emit_main_u_cleanup_quarantined_exes(c);
        if (strcmp(op, "main_u_run_package_only_install") == 0) o = emit_main_u_run_package_only_install(c);
        if (strcmp(op, "main_u_lazy_refresh_repair_specs") == 0) o = emit_main_u_lazy_refresh_repair_specs(c);
        if (strcmp(op, "main_u_upgrade_pip_before_lazy_refresh") == 0) o = emit_main_u_upgrade_pip_before_lazy_refresh(c);
        if (strcmp(op, "main_u_detect_broken_lazy_refresh_imports") == 0) o = emit_main_u_detect_broken_lazy_refresh_imports(c);
        if (strcmp(op, "main_u_repair_broken_lazy_refresh_imports") == 0) o = emit_main_u_repair_broken_lazy_refresh_imports(c);
        if (strcmp(op, "main_u_repair_venv_via_import_probes") == 0) o = emit_main_u_repair_venv_via_import_probes(c);
        if (strcmp(op, "main_u_refresh_active_lazy_features") == 0) o = emit_main_u_refresh_active_lazy_features(c);
        if (strcmp(op, "main_u_install_python_dependencies_with_optional_fallback") == 0) o = emit_main_u_install_python_dependencies_with_optional_fallback(c);
        if (strcmp(op, "main_u_load_console_script_names") == 0) o = emit_main_u_load_console_script_names(c);
        if (strcmp(op, "main_u_verify_console_scripts_installed") == 0) o = emit_main_u_verify_console_scripts_installed(c);
        if (strcmp(op, "main_u_verify_core_dependencies_installed") == 0) o = emit_main_u_verify_core_dependencies_installed(c);
        if (strcmp(op, "main_u_resolve_install_target_python") == 0) o = emit_main_u_resolve_install_target_python(c);
        if (strcmp(op, "main_u_install_psutil_android_compat") == 0) o = emit_main_u_install_psutil_android_compat(c);
        if (strcmp(op, "main_u_ensure_uv_for_termux") == 0) o = emit_main_u_ensure_uv_for_termux(c);
        if (strcmp(op, "main_u_npm_manifest_paths") == 0) o = emit_main_u_npm_manifest_paths(c);
        if (strcmp(op, "main_u_npm_manifests_digest") == 0) o = emit_main_u_npm_manifests_digest(c);
        if (strcmp(op, "main_u_npm_lockfile_changed") == 0) o = emit_main_u_npm_lockfile_changed(c);
        if (strcmp(op, "main_u_record_npm_lockfile_hash") == 0) o = emit_main_u_record_npm_lockfile_hash(c);
        if (strcmp(op, "main_u_is_windows_npm_path") == 0) o = emit_main_u_is_windows_npm_path(c);
        if (strcmp(op, "main_u_resolve_node_runtime_npm") == 0) o = emit_main_u_resolve_node_runtime_npm(c);
        if (strcmp(op, "main_u_update_node_dependencies") == 0) o = emit_main_u_update_node_dependencies(c);
        if (strcmp(op, "main_u__getattr___2") == 0) o = emit_main_u__getattr___2(c);
        if (strcmp(op, "main_u_install_hangup_protection") == 0) o = emit_main_u_install_hangup_protection(c);
        if (strcmp(op, "main_u_log_only_write") == 0) o = emit_main_u_log_only_write(c);
        if (strcmp(op, "main_u_run_logged_subprocess") == 0) o = emit_main_u_run_logged_subprocess(c);
        if (strcmp(op, "main_u_finalize_update_output") == 0) o = emit_main_u_finalize_update_output(c);
        if (strcmp(op, "main_u_resolve_update_branch") == 0) o = emit_main_u_resolve_update_branch(c);
        if (strcmp(op, "main_u_cmd_update_check") == 0) o = emit_main_u_cmd_update_check(c);
        if (strcmp(op, "main_u_ensure_fhs_path_guard") == 0) o = emit_main_u_ensure_fhs_path_guard(c);
        if (strcmp(op, "main_u_size_delta_label") == 0) o = emit_main_u_size_delta_label(c);
        if (strcmp(op, "main_u_resolve_pre_update_backup_mode") == 0) o = emit_main_u_resolve_pre_update_backup_mode(c);
        if (strcmp(op, "main_u_run_pre_update_backup") == 0) o = emit_main_u_run_pre_update_backup(c);
        if (strcmp(op, "main_u_write_update_planned_stop_marker") == 0) o = emit_main_u_write_update_planned_stop_marker(c);
        if (strcmp(op, "main_u_wait_for_windows_update_gateway_exit") == 0) o = emit_main_u_wait_for_windows_update_gateway_exit(c);
        if (strcmp(op, "main_u_venv_core_imports_healthy") == 0) o = emit_main_u_venv_core_imports_healthy(c);
        if (strcmp(op, "main_u_detect_venv_python_processes") == 0) o = emit_main_u_detect_venv_python_processes(c);
        if (strcmp(op, "main_u_format_venv_python_holders_message") == 0) o = emit_main_u_format_venv_python_holders_message(c);
        if (strcmp(op, "main_u_pause_windows_gateways_for_update") == 0) o = emit_main_u_pause_windows_gateways_for_update(c);
        if (strcmp(op, "main_u_cold_start_windows_gateway_after_update") == 0) o = emit_main_u_cold_start_windows_gateway_after_update(c);
        if (strcmp(op, "main_u_for_each_systemd_gateway_unit") == 0) o = emit_main_u_for_each_systemd_gateway_unit(c);
        if (strcmp(op, "main_u_warn_incomplete_gateway_fleet_restart") == 0) o = emit_main_u_warn_incomplete_gateway_fleet_restart(c);
        if (strcmp(op, "main_u_resume_windows_gateways_after_update") == 0) o = emit_main_u_resume_windows_gateways_after_update(c);
        if (strcmp(op, "main_u_discard_lockfile_churn") == 0) o = emit_main_u_discard_lockfile_churn(c);
        if (strcmp(op, "main_u_cmd_update_impl") == 0) o = emit_main_u_cmd_update_impl(c);
        if (strcmp(op, "main_u_render_distribution_plan") == 0) o = emit_main_u_render_distribution_plan(c);
        if (strcmp(op, "main_u_report_dashboard_status") == 0) o = emit_main_u_report_dashboard_status(c);
        if (strcmp(op, "main_u_dashboard_listening") == 0) o = emit_main_u_dashboard_listening(c);
        if (strcmp(op, "main_u_maybe_setup_dashboard_auth_interactively") == 0) o = emit_main_u_maybe_setup_dashboard_auth_interactively(c);
        if (strcmp(op, "main_u_read_ssh_session_token_file") == 0) o = emit_main_u_read_ssh_session_token_file(c);
        if (strcmp(op, "main_u_is_electron_packaged_web_dist") == 0) o = emit_main_u_is_electron_packaged_web_dist(c);
        if (strcmp(op, "main_cmd_dashboard_register") == 0) o = emit_main_cmd_dashboard_register(c);
        if (strcmp(op, "main_cmd_gateway_enroll") == 0) o = emit_main_cmd_gateway_enroll(c);
        if (strcmp(op, "main_cmd_completion") == 0) o = emit_main_cmd_completion(c);
        if (strcmp(op, "main_cmd_console") == 0) o = emit_main_cmd_console(c);
        if (strcmp(op, "main_u_plugin_cli_discovery_needed") == 0) o = emit_main_u_plugin_cli_discovery_needed(c);
        if (strcmp(op, "main_u_command_has_dedicated_mcp_startup") == 0) o = emit_main_u_command_has_dedicated_mcp_startup(c);
        if (strcmp(op, "main_u_should_background_mcp_startup") == 0) o = emit_main_u_should_background_mcp_startup(c);
        if (strcmp(op, "main_u_prepare_agent_startup") == 0) o = emit_main_u_prepare_agent_startup(c);
        if (strcmp(op, "main_u_apply_safe_mode") == 0) o = emit_main_u_apply_safe_mode(c);
        if (strcmp(op, "main_u_set_chat_arg_defaults") == 0) o = emit_main_u_set_chat_arg_defaults(c);
        if (strcmp(op, "main_u_try_termux_fast_cli_launch") == 0) o = emit_main_u_try_termux_fast_cli_launch(c);
        if (strcmp(op, "main_u_try_termux_fast_tui_launch") == 0) o = emit_main_u_try_termux_fast_tui_launch(c);
        if (strcmp(op, "main_cmd_acp") == 0) o = emit_main_cmd_acp(c);
        if (strcmp(op, "main_cmd_pairing") == 0) o = emit_main_cmd_pairing(c);
        if (strcmp(op, "main_cmd_claw") == 0) o = emit_main_cmd_claw(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
