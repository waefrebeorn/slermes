/* AUTO-GENERATED integration oracle harness for port_cli_gateway_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_cli_gateway_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int cgw_has_process_service_mismatch(const char *);
extern int cgw_u_scan_gateway_pids(const char *);
extern int cgw_u_filter_venv_launcher_stubs(const char *);
extern int cgw_find_profile_gateway_processes(const char *);
extern int cgw_u_gateway_run_args_for_profile(const char *);
extern int cgw_u_prepare_profile_gateway_update_restart(const char *);
extern int cgw_launch_detached_profile_gateway_restart(const char *);
extern int cgw_u_probe_systemd_service_running(const char *);
extern int cgw_u_read_systemd_unit_environment(const char *);
extern int cgw_u_hermes_home_from_systemd_unit_file(const char *);
extern int cgw_u_sync_hermes_home_from_systemd_unit(const char *);
extern int cgw_u_read_systemd_unit_properties(const char *);
extern int cgw_u_systemd_main_pid_from_props(const char *);
extern int cgw_u_systemd_main_pid(const char *);
extern int cgw_u_read_gateway_runtime_status(const char *);
extern int cgw_u_gateway_runtime_status_for_pid(const char *);
extern int cgw_u_wait_for_systemd_service_restart(const char *);
extern int cgw_u_systemd_unit_is_start_limited(const char *);
extern int cgw_u_systemd_error_indicates_start_limit(const char *);
extern int cgw_u_systemd_service_is_start_limited(const char *);
extern int cgw_u_print_systemd_start_limit_wait(const char *);
extern int cgw_u_recover_pending_systemd_restart(const char *);
extern int cgw_u_parse_launchd_pid_from_list_output(const char *);
extern int cgw_u_probe_launchd_service_running(const char *);
extern int cgw_get_gateway_runtime_snapshot(const char *);
extern int cgw_u_format_gateway_pids(const char *);
extern int cgw_u_print_gateway_process_mismatch(const char *);
extern int cgw_u_print_other_profiles_gateway_status(const char *);
extern int cgw_u_reap_unsupervised_gateway_orphans(const char *);
extern int cgw_u_wsl_systemd_operational(const char *);
extern int cgw_u_systemd_operational(const char *);
extern int cgw_u_container_systemd_operational(const char *);
extern int cgw_u_windows_gateway_should_absorb_console_controls(const char *);
extern int cgw_u_profile_arg_for_target_user(const char *);
extern int cgw_get_service_name(const char *);
extern int cgw_get_systemd_unit_path(const char *);
extern int cgw_u_user_dbus_socket_path(const char *);
extern int cgw_u_user_systemd_private_socket_path(const char *);
extern int cgw_u_user_systemd_socket_ready(const char *);
extern int cgw_u_ensure_user_systemd_env(const char *);
extern int cgw_u_wait_for_user_dbus_socket(const char *);
extern int cgw_u_preflight_user_systemd(const char *);
extern int cgw_u_raise_user_systemd_unavailable(const char *);
extern int cgw_u_systemctl_cmd(const char *);
extern int cgw_u_journalctl_cmd(const char *);
extern int cgw_u_run_systemctl(const char *);
extern int cgw_u_service_scope_label(const char *);
extern int cgw_get_installed_systemd_scopes(const char *);
extern int cgw_has_conflicting_systemd_units(const char *);
extern int cgw_u_legacy_unit_search_paths(const char *);
extern int cgw_u_find_legacy_hermes_units(const char *);
extern int cgw_has_legacy_hermes_units(const char *);
extern int cgw_print_legacy_unit_warning(const char *);
extern int cgw_remove_legacy_hermes_units(const char *);
extern int cgw_print_systemd_scope_conflict_warning(const char *);
extern int cgw_u_require_root_for_system_service(const char *);
extern int cgw_u_system_service_identity(const char *);
extern int cgw_u_read_systemd_user_from_unit(const char *);
extern int cgw_u_default_system_service_user(const char *);
extern int cgw_prompt_linux_gateway_install_scope(const char *);
extern int cgw_install_linux_gateway_from_setup(const char *);
extern int cgw_get_systemd_linger_status(const char *);
extern int cgw_print_systemd_linger_guidance(const char *);
extern int cgw_u_launchd_user_home(const char *);
extern int cgw_get_launchd_plist_path(const char *);
extern int cgw_u_detect_venv_dir(const char *);
extern int cgw_get_python_path(const char *);
extern int cgw_u_build_user_local_paths(const char *);
extern int cgw_u_build_wsl_interop_paths(const char *);
extern int cgw_u_remap_path_for_user(const char *);
extern int cgw_u_hermes_home_for_target_user(const char *);
extern int cgw_u_build_service_path_dirs(const char *);
extern int cgw_u_stable_service_working_dir(const char *);
extern int cgw_u_systemd_watchdog_seconds(const char *);
extern int cgw_u_systemd_watchdog_service_fields(const char *);
extern int cgw_generate_systemd_unit(const char *);
extern int cgw_u_normalize_service_definition(const char *);
extern int cgw_u_strip_optional_systemd_directives(const char *);
extern int cgw_u_normalize_launchd_plist_for_comparison(const char *);
extern int cgw_systemd_unit_is_current(const char *);
extern int cgw_u_temp_home_in_service_definition(const char *);
extern int cgw_u_refuse_temp_home_service_write(const char *);
extern int cgw_refresh_systemd_unit_if_needed(const char *);
extern int cgw_u_print_linger_enable_warning(const char *);
extern int cgw_u_ensure_linger_enabled(const char *);
extern int cgw_u_select_systemd_scope(const char *);
extern int cgw_u_system_scope_wizard_would_need_root(const char *);
extern int cgw_u_print_system_scope_remediation(const char *);
extern int cgw_u_get_restart_drain_timeout(const char *);
extern int cgw_systemd_install(const char *);
extern int cgw_systemd_uninstall(const char *);
extern int cgw_u_require_service_installed(const char *);
extern int cgw_systemd_start(const char *);
extern int cgw_systemd_stop(const char *);
extern int cgw_systemd_restart(const char *);
extern int cgw_systemd_status(const char *);
extern int cgw_get_launchd_label(const char *);
extern int cgw_u_launchd_domain(const char *);
extern int cgw_u_launchd_error_indicates_unloaded(const char *);
extern int cgw_u_launchctl_domain_unsupported(const char *);
extern int cgw_u_launchctl_bootstrap(const char *);
extern int cgw_u_launchd_reload_log_path(const char *);
extern int cgw_u_append_launchd_reload_log(const char *);
extern int cgw_u_launchctl_label_registered(const char *);
extern int cgw_u_retry_launchctl_bootstrap_until_registered(const char *);
extern int cgw_u_launchd_unsupported_marker_path(const char *);
extern int cgw_u_write_launchd_unsupported_marker(const char *);
extern int cgw_u_clear_launchd_unsupported_marker(const char *);
extern int cgw_u_launchd_unsupported_marker_exists(const char *);
extern int cgw_u_gateway_run_command(const char *);
extern int cgw_u_spawn_detached_gateway(const char *);
extern int cgw_u_launchd_fallback_to_detached(const char *);
extern int cgw_generate_launchd_plist(const char *);
extern int cgw_launchd_plist_is_current(const char *);
extern int cgw_refresh_launchd_plist_if_needed(const char *);
extern int cgw_launchd_install(const char *);
extern int cgw_launchd_uninstall(const char *);
extern int cgw_launchd_start(const char *);
extern int cgw_launchd_stop(const char *);
extern int cgw_u_wait_for_gateway_exit(const char *);
extern int cgw_launchd_restart(const char *);
extern int cgw_launchd_status(const char *);
extern int cgw_u_truthy_env(const char *);
extern int cgw_u_is_official_docker_checkout(const char *);
extern int cgw_u_running_under_gateway_supervisor(const char *);
extern int cgw_u_guard_supervised_gateway_conflict(const char *);
extern int cgw_u_guard_existing_gateway_process_conflict(const char *);
extern int cgw_u_guard_official_docker_root_gateway(const char *);
extern int cgw_u_all_platforms(const char *);
extern int cgw_u_platform_status(const char *);
extern int cgw_u_runtime_health_lines(const char *);
extern int cgw_u_set_platform_unauthorized_dm_behavior(const char *);
extern int cgw_u_setup_standard_platform(const char *);
extern int cgw_u_is_service_installed(const char *);
extern int cgw_u_is_service_running(const char *);
extern int cgw_u_builtin_setup_fn(const char *);
extern int cgw_u_configure_platform(const char *);
extern int cgw_u_dispatch_via_service_manager_if_s6(const char *);
extern int cgw_u_dispatch_all_via_service_manager_if_s6(const char *);
extern int cgw_u_maybe_redirect_run_to_s6_supervision(const char *);
extern int cgw_u_block_until_terminated(const char *);
extern int cgw_u_gateway_command_inner(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_cgw_has_process_service_mismatch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_has_process_service_mismatch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_has_process_service_mismatch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_scan_gateway_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_scan_gateway_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_scan_gateway_pids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_filter_venv_launcher_stubs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_filter_venv_launcher_stubs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_filter_venv_launcher_stubs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_find_profile_gateway_processes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_find_profile_gateway_processes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_find_profile_gateway_processes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_gateway_run_args_for_profile(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_gateway_run_args_for_profile(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_gateway_run_args_for_profile"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_prepare_profile_gateway_update_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_prepare_profile_gateway_update_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_prepare_profile_gateway_update_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launch_detached_profile_gateway_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launch_detached_profile_gateway_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launch_detached_profile_gateway_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_probe_systemd_service_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_probe_systemd_service_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_probe_systemd_service_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_read_systemd_unit_environment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_read_systemd_unit_environment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_read_systemd_unit_environment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_hermes_home_from_systemd_unit_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_hermes_home_from_systemd_unit_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_hermes_home_from_systemd_unit_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_sync_hermes_home_from_systemd_unit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_sync_hermes_home_from_systemd_unit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_sync_hermes_home_from_systemd_unit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_read_systemd_unit_properties(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_read_systemd_unit_properties(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_read_systemd_unit_properties"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_main_pid_from_props(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_main_pid_from_props(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_main_pid_from_props"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_main_pid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_main_pid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_main_pid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_read_gateway_runtime_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_read_gateway_runtime_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_read_gateway_runtime_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_gateway_runtime_status_for_pid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_gateway_runtime_status_for_pid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_gateway_runtime_status_for_pid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_wait_for_systemd_service_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_wait_for_systemd_service_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_wait_for_systemd_service_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_unit_is_start_limited(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_unit_is_start_limited(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_unit_is_start_limited"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_error_indicates_start_limit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_error_indicates_start_limit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_error_indicates_start_limit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_service_is_start_limited(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_service_is_start_limited(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_service_is_start_limited"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_print_systemd_start_limit_wait(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_print_systemd_start_limit_wait(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_print_systemd_start_limit_wait"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_recover_pending_systemd_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_recover_pending_systemd_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_recover_pending_systemd_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_parse_launchd_pid_from_list_output(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_parse_launchd_pid_from_list_output(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_parse_launchd_pid_from_list_output"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_probe_launchd_service_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_probe_launchd_service_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_probe_launchd_service_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_gateway_runtime_snapshot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_gateway_runtime_snapshot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_gateway_runtime_snapshot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_format_gateway_pids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_format_gateway_pids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_format_gateway_pids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_print_gateway_process_mismatch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_print_gateway_process_mismatch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_print_gateway_process_mismatch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_print_other_profiles_gateway_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_print_other_profiles_gateway_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_print_other_profiles_gateway_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_reap_unsupervised_gateway_orphans(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_reap_unsupervised_gateway_orphans(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_reap_unsupervised_gateway_orphans"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_wsl_systemd_operational(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_wsl_systemd_operational(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_wsl_systemd_operational"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_operational(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_operational(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_operational"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_container_systemd_operational(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_container_systemd_operational(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_container_systemd_operational"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_windows_gateway_should_absorb_console_controls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_windows_gateway_should_absorb_console_controls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_windows_gateway_should_absorb_console_controls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_profile_arg_for_target_user(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_profile_arg_for_target_user(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_profile_arg_for_target_user"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_service_name(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_service_name(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_service_name"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_systemd_unit_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_systemd_unit_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_systemd_unit_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_user_dbus_socket_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_user_dbus_socket_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_user_dbus_socket_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_user_systemd_private_socket_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_user_systemd_private_socket_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_user_systemd_private_socket_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_user_systemd_socket_ready(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_user_systemd_socket_ready(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_user_systemd_socket_ready"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_ensure_user_systemd_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_ensure_user_systemd_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_ensure_user_systemd_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_wait_for_user_dbus_socket(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_wait_for_user_dbus_socket(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_wait_for_user_dbus_socket"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_preflight_user_systemd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_preflight_user_systemd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_preflight_user_systemd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_raise_user_systemd_unavailable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_raise_user_systemd_unavailable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_raise_user_systemd_unavailable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemctl_cmd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemctl_cmd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemctl_cmd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_journalctl_cmd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_journalctl_cmd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_journalctl_cmd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_run_systemctl(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_run_systemctl(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_run_systemctl"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_service_scope_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_service_scope_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_service_scope_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_installed_systemd_scopes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_installed_systemd_scopes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_installed_systemd_scopes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_has_conflicting_systemd_units(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_has_conflicting_systemd_units(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_has_conflicting_systemd_units"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_legacy_unit_search_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_legacy_unit_search_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_legacy_unit_search_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_find_legacy_hermes_units(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_find_legacy_hermes_units(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_find_legacy_hermes_units"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_has_legacy_hermes_units(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_has_legacy_hermes_units(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_has_legacy_hermes_units"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_print_legacy_unit_warning(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_print_legacy_unit_warning(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_print_legacy_unit_warning"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_remove_legacy_hermes_units(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_remove_legacy_hermes_units(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_remove_legacy_hermes_units"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_print_systemd_scope_conflict_warning(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_print_systemd_scope_conflict_warning(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_print_systemd_scope_conflict_warning"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_require_root_for_system_service(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_require_root_for_system_service(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_require_root_for_system_service"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_system_service_identity(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_system_service_identity(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_system_service_identity"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_read_systemd_user_from_unit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_read_systemd_user_from_unit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_read_systemd_user_from_unit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_default_system_service_user(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_default_system_service_user(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_default_system_service_user"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_prompt_linux_gateway_install_scope(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_prompt_linux_gateway_install_scope(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_prompt_linux_gateway_install_scope"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_install_linux_gateway_from_setup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_install_linux_gateway_from_setup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_install_linux_gateway_from_setup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_systemd_linger_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_systemd_linger_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_systemd_linger_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_print_systemd_linger_guidance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_print_systemd_linger_guidance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_print_systemd_linger_guidance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_user_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_user_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_user_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_launchd_plist_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_launchd_plist_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_launchd_plist_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_detect_venv_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_detect_venv_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_detect_venv_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_python_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_python_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_python_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_build_user_local_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_build_user_local_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_build_user_local_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_build_wsl_interop_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_build_wsl_interop_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_build_wsl_interop_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_remap_path_for_user(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_remap_path_for_user(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_remap_path_for_user"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_hermes_home_for_target_user(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_hermes_home_for_target_user(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_hermes_home_for_target_user"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_build_service_path_dirs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_build_service_path_dirs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_build_service_path_dirs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_stable_service_working_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_stable_service_working_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_stable_service_working_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_watchdog_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_watchdog_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_watchdog_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_systemd_watchdog_service_fields(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_systemd_watchdog_service_fields(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_systemd_watchdog_service_fields"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_generate_systemd_unit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_generate_systemd_unit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_generate_systemd_unit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_normalize_service_definition(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_normalize_service_definition(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_normalize_service_definition"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_strip_optional_systemd_directives(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_strip_optional_systemd_directives(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_strip_optional_systemd_directives"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_normalize_launchd_plist_for_comparison(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_normalize_launchd_plist_for_comparison(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_normalize_launchd_plist_for_comparison"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_unit_is_current(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_unit_is_current(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_unit_is_current"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_temp_home_in_service_definition(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_temp_home_in_service_definition(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_temp_home_in_service_definition"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_refuse_temp_home_service_write(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_refuse_temp_home_service_write(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_refuse_temp_home_service_write"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_refresh_systemd_unit_if_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_refresh_systemd_unit_if_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_refresh_systemd_unit_if_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_print_linger_enable_warning(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_print_linger_enable_warning(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_print_linger_enable_warning"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_ensure_linger_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_ensure_linger_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_ensure_linger_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_select_systemd_scope(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_select_systemd_scope(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_select_systemd_scope"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_system_scope_wizard_would_need_root(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_system_scope_wizard_would_need_root(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_system_scope_wizard_would_need_root"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_print_system_scope_remediation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_print_system_scope_remediation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_print_system_scope_remediation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_get_restart_drain_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_get_restart_drain_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_get_restart_drain_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_uninstall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_uninstall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_uninstall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_require_service_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_require_service_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_require_service_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_stop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_stop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_stop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_systemd_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_systemd_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_systemd_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_get_launchd_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_get_launchd_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_get_launchd_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_domain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_domain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_domain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_error_indicates_unloaded(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_error_indicates_unloaded(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_error_indicates_unloaded"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchctl_domain_unsupported(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchctl_domain_unsupported(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchctl_domain_unsupported"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchctl_bootstrap(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchctl_bootstrap(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchctl_bootstrap"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_reload_log_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_reload_log_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_reload_log_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_append_launchd_reload_log(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_append_launchd_reload_log(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_append_launchd_reload_log"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchctl_label_registered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchctl_label_registered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchctl_label_registered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_retry_launchctl_bootstrap_until_registered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_retry_launchctl_bootstrap_until_registered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_retry_launchctl_bootstrap_until_registered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_unsupported_marker_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_unsupported_marker_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_unsupported_marker_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_write_launchd_unsupported_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_write_launchd_unsupported_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_write_launchd_unsupported_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_clear_launchd_unsupported_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_clear_launchd_unsupported_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_clear_launchd_unsupported_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_unsupported_marker_exists(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_unsupported_marker_exists(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_unsupported_marker_exists"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_gateway_run_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_gateway_run_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_gateway_run_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_spawn_detached_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_spawn_detached_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_spawn_detached_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_launchd_fallback_to_detached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_launchd_fallback_to_detached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_launchd_fallback_to_detached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_generate_launchd_plist(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_generate_launchd_plist(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_generate_launchd_plist"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_plist_is_current(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_plist_is_current(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_plist_is_current"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_refresh_launchd_plist_if_needed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_refresh_launchd_plist_if_needed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_refresh_launchd_plist_if_needed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_install(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_install(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_install"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_uninstall(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_uninstall(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_uninstall"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_stop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_stop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_stop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_wait_for_gateway_exit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_wait_for_gateway_exit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_wait_for_gateway_exit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_restart(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_restart(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_restart"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_launchd_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_launchd_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_launchd_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_truthy_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_truthy_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_truthy_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_is_official_docker_checkout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_is_official_docker_checkout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_is_official_docker_checkout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_running_under_gateway_supervisor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_running_under_gateway_supervisor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_running_under_gateway_supervisor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_guard_supervised_gateway_conflict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_guard_supervised_gateway_conflict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_guard_supervised_gateway_conflict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_guard_existing_gateway_process_conflict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_guard_existing_gateway_process_conflict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_guard_existing_gateway_process_conflict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_guard_official_docker_root_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_guard_official_docker_root_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_guard_official_docker_root_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_all_platforms(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_all_platforms(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_all_platforms"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_platform_status(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_platform_status(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_platform_status"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_runtime_health_lines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_runtime_health_lines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_runtime_health_lines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_set_platform_unauthorized_dm_behavior(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_set_platform_unauthorized_dm_behavior(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_set_platform_unauthorized_dm_behavior"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_setup_standard_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_setup_standard_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_setup_standard_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_is_service_installed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_is_service_installed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_is_service_installed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_is_service_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_is_service_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_is_service_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_builtin_setup_fn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_builtin_setup_fn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_builtin_setup_fn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_configure_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_configure_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_configure_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_dispatch_via_service_manager_if_s6(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_dispatch_via_service_manager_if_s6(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_dispatch_via_service_manager_if_s6"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_dispatch_all_via_service_manager_if_s6(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_dispatch_all_via_service_manager_if_s6(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_dispatch_all_via_service_manager_if_s6"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_maybe_redirect_run_to_s6_supervision(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_maybe_redirect_run_to_s6_supervision(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_maybe_redirect_run_to_s6_supervision"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_block_until_terminated(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_block_until_terminated(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_block_until_terminated"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cgw_u_gateway_command_inner(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cgw_u_gateway_command_inner(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cgw_u_gateway_command_inner"));
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
        if (strcmp(op, "cgw_has_process_service_mismatch") == 0) o = emit_cgw_has_process_service_mismatch(c);
        if (strcmp(op, "cgw_u_scan_gateway_pids") == 0) o = emit_cgw_u_scan_gateway_pids(c);
        if (strcmp(op, "cgw_u_filter_venv_launcher_stubs") == 0) o = emit_cgw_u_filter_venv_launcher_stubs(c);
        if (strcmp(op, "cgw_find_profile_gateway_processes") == 0) o = emit_cgw_find_profile_gateway_processes(c);
        if (strcmp(op, "cgw_u_gateway_run_args_for_profile") == 0) o = emit_cgw_u_gateway_run_args_for_profile(c);
        if (strcmp(op, "cgw_u_prepare_profile_gateway_update_restart") == 0) o = emit_cgw_u_prepare_profile_gateway_update_restart(c);
        if (strcmp(op, "cgw_launch_detached_profile_gateway_restart") == 0) o = emit_cgw_launch_detached_profile_gateway_restart(c);
        if (strcmp(op, "cgw_u_probe_systemd_service_running") == 0) o = emit_cgw_u_probe_systemd_service_running(c);
        if (strcmp(op, "cgw_u_read_systemd_unit_environment") == 0) o = emit_cgw_u_read_systemd_unit_environment(c);
        if (strcmp(op, "cgw_u_hermes_home_from_systemd_unit_file") == 0) o = emit_cgw_u_hermes_home_from_systemd_unit_file(c);
        if (strcmp(op, "cgw_u_sync_hermes_home_from_systemd_unit") == 0) o = emit_cgw_u_sync_hermes_home_from_systemd_unit(c);
        if (strcmp(op, "cgw_u_read_systemd_unit_properties") == 0) o = emit_cgw_u_read_systemd_unit_properties(c);
        if (strcmp(op, "cgw_u_systemd_main_pid_from_props") == 0) o = emit_cgw_u_systemd_main_pid_from_props(c);
        if (strcmp(op, "cgw_u_systemd_main_pid") == 0) o = emit_cgw_u_systemd_main_pid(c);
        if (strcmp(op, "cgw_u_read_gateway_runtime_status") == 0) o = emit_cgw_u_read_gateway_runtime_status(c);
        if (strcmp(op, "cgw_u_gateway_runtime_status_for_pid") == 0) o = emit_cgw_u_gateway_runtime_status_for_pid(c);
        if (strcmp(op, "cgw_u_wait_for_systemd_service_restart") == 0) o = emit_cgw_u_wait_for_systemd_service_restart(c);
        if (strcmp(op, "cgw_u_systemd_unit_is_start_limited") == 0) o = emit_cgw_u_systemd_unit_is_start_limited(c);
        if (strcmp(op, "cgw_u_systemd_error_indicates_start_limit") == 0) o = emit_cgw_u_systemd_error_indicates_start_limit(c);
        if (strcmp(op, "cgw_u_systemd_service_is_start_limited") == 0) o = emit_cgw_u_systemd_service_is_start_limited(c);
        if (strcmp(op, "cgw_u_print_systemd_start_limit_wait") == 0) o = emit_cgw_u_print_systemd_start_limit_wait(c);
        if (strcmp(op, "cgw_u_recover_pending_systemd_restart") == 0) o = emit_cgw_u_recover_pending_systemd_restart(c);
        if (strcmp(op, "cgw_u_parse_launchd_pid_from_list_output") == 0) o = emit_cgw_u_parse_launchd_pid_from_list_output(c);
        if (strcmp(op, "cgw_u_probe_launchd_service_running") == 0) o = emit_cgw_u_probe_launchd_service_running(c);
        if (strcmp(op, "cgw_get_gateway_runtime_snapshot") == 0) o = emit_cgw_get_gateway_runtime_snapshot(c);
        if (strcmp(op, "cgw_u_format_gateway_pids") == 0) o = emit_cgw_u_format_gateway_pids(c);
        if (strcmp(op, "cgw_u_print_gateway_process_mismatch") == 0) o = emit_cgw_u_print_gateway_process_mismatch(c);
        if (strcmp(op, "cgw_u_print_other_profiles_gateway_status") == 0) o = emit_cgw_u_print_other_profiles_gateway_status(c);
        if (strcmp(op, "cgw_u_reap_unsupervised_gateway_orphans") == 0) o = emit_cgw_u_reap_unsupervised_gateway_orphans(c);
        if (strcmp(op, "cgw_u_wsl_systemd_operational") == 0) o = emit_cgw_u_wsl_systemd_operational(c);
        if (strcmp(op, "cgw_u_systemd_operational") == 0) o = emit_cgw_u_systemd_operational(c);
        if (strcmp(op, "cgw_u_container_systemd_operational") == 0) o = emit_cgw_u_container_systemd_operational(c);
        if (strcmp(op, "cgw_u_windows_gateway_should_absorb_console_controls") == 0) o = emit_cgw_u_windows_gateway_should_absorb_console_controls(c);
        if (strcmp(op, "cgw_u_profile_arg_for_target_user") == 0) o = emit_cgw_u_profile_arg_for_target_user(c);
        if (strcmp(op, "cgw_get_service_name") == 0) o = emit_cgw_get_service_name(c);
        if (strcmp(op, "cgw_get_systemd_unit_path") == 0) o = emit_cgw_get_systemd_unit_path(c);
        if (strcmp(op, "cgw_u_user_dbus_socket_path") == 0) o = emit_cgw_u_user_dbus_socket_path(c);
        if (strcmp(op, "cgw_u_user_systemd_private_socket_path") == 0) o = emit_cgw_u_user_systemd_private_socket_path(c);
        if (strcmp(op, "cgw_u_user_systemd_socket_ready") == 0) o = emit_cgw_u_user_systemd_socket_ready(c);
        if (strcmp(op, "cgw_u_ensure_user_systemd_env") == 0) o = emit_cgw_u_ensure_user_systemd_env(c);
        if (strcmp(op, "cgw_u_wait_for_user_dbus_socket") == 0) o = emit_cgw_u_wait_for_user_dbus_socket(c);
        if (strcmp(op, "cgw_u_preflight_user_systemd") == 0) o = emit_cgw_u_preflight_user_systemd(c);
        if (strcmp(op, "cgw_u_raise_user_systemd_unavailable") == 0) o = emit_cgw_u_raise_user_systemd_unavailable(c);
        if (strcmp(op, "cgw_u_systemctl_cmd") == 0) o = emit_cgw_u_systemctl_cmd(c);
        if (strcmp(op, "cgw_u_journalctl_cmd") == 0) o = emit_cgw_u_journalctl_cmd(c);
        if (strcmp(op, "cgw_u_run_systemctl") == 0) o = emit_cgw_u_run_systemctl(c);
        if (strcmp(op, "cgw_u_service_scope_label") == 0) o = emit_cgw_u_service_scope_label(c);
        if (strcmp(op, "cgw_get_installed_systemd_scopes") == 0) o = emit_cgw_get_installed_systemd_scopes(c);
        if (strcmp(op, "cgw_has_conflicting_systemd_units") == 0) o = emit_cgw_has_conflicting_systemd_units(c);
        if (strcmp(op, "cgw_u_legacy_unit_search_paths") == 0) o = emit_cgw_u_legacy_unit_search_paths(c);
        if (strcmp(op, "cgw_u_find_legacy_hermes_units") == 0) o = emit_cgw_u_find_legacy_hermes_units(c);
        if (strcmp(op, "cgw_has_legacy_hermes_units") == 0) o = emit_cgw_has_legacy_hermes_units(c);
        if (strcmp(op, "cgw_print_legacy_unit_warning") == 0) o = emit_cgw_print_legacy_unit_warning(c);
        if (strcmp(op, "cgw_remove_legacy_hermes_units") == 0) o = emit_cgw_remove_legacy_hermes_units(c);
        if (strcmp(op, "cgw_print_systemd_scope_conflict_warning") == 0) o = emit_cgw_print_systemd_scope_conflict_warning(c);
        if (strcmp(op, "cgw_u_require_root_for_system_service") == 0) o = emit_cgw_u_require_root_for_system_service(c);
        if (strcmp(op, "cgw_u_system_service_identity") == 0) o = emit_cgw_u_system_service_identity(c);
        if (strcmp(op, "cgw_u_read_systemd_user_from_unit") == 0) o = emit_cgw_u_read_systemd_user_from_unit(c);
        if (strcmp(op, "cgw_u_default_system_service_user") == 0) o = emit_cgw_u_default_system_service_user(c);
        if (strcmp(op, "cgw_prompt_linux_gateway_install_scope") == 0) o = emit_cgw_prompt_linux_gateway_install_scope(c);
        if (strcmp(op, "cgw_install_linux_gateway_from_setup") == 0) o = emit_cgw_install_linux_gateway_from_setup(c);
        if (strcmp(op, "cgw_get_systemd_linger_status") == 0) o = emit_cgw_get_systemd_linger_status(c);
        if (strcmp(op, "cgw_print_systemd_linger_guidance") == 0) o = emit_cgw_print_systemd_linger_guidance(c);
        if (strcmp(op, "cgw_u_launchd_user_home") == 0) o = emit_cgw_u_launchd_user_home(c);
        if (strcmp(op, "cgw_get_launchd_plist_path") == 0) o = emit_cgw_get_launchd_plist_path(c);
        if (strcmp(op, "cgw_u_detect_venv_dir") == 0) o = emit_cgw_u_detect_venv_dir(c);
        if (strcmp(op, "cgw_get_python_path") == 0) o = emit_cgw_get_python_path(c);
        if (strcmp(op, "cgw_u_build_user_local_paths") == 0) o = emit_cgw_u_build_user_local_paths(c);
        if (strcmp(op, "cgw_u_build_wsl_interop_paths") == 0) o = emit_cgw_u_build_wsl_interop_paths(c);
        if (strcmp(op, "cgw_u_remap_path_for_user") == 0) o = emit_cgw_u_remap_path_for_user(c);
        if (strcmp(op, "cgw_u_hermes_home_for_target_user") == 0) o = emit_cgw_u_hermes_home_for_target_user(c);
        if (strcmp(op, "cgw_u_build_service_path_dirs") == 0) o = emit_cgw_u_build_service_path_dirs(c);
        if (strcmp(op, "cgw_u_stable_service_working_dir") == 0) o = emit_cgw_u_stable_service_working_dir(c);
        if (strcmp(op, "cgw_u_systemd_watchdog_seconds") == 0) o = emit_cgw_u_systemd_watchdog_seconds(c);
        if (strcmp(op, "cgw_u_systemd_watchdog_service_fields") == 0) o = emit_cgw_u_systemd_watchdog_service_fields(c);
        if (strcmp(op, "cgw_generate_systemd_unit") == 0) o = emit_cgw_generate_systemd_unit(c);
        if (strcmp(op, "cgw_u_normalize_service_definition") == 0) o = emit_cgw_u_normalize_service_definition(c);
        if (strcmp(op, "cgw_u_strip_optional_systemd_directives") == 0) o = emit_cgw_u_strip_optional_systemd_directives(c);
        if (strcmp(op, "cgw_u_normalize_launchd_plist_for_comparison") == 0) o = emit_cgw_u_normalize_launchd_plist_for_comparison(c);
        if (strcmp(op, "cgw_systemd_unit_is_current") == 0) o = emit_cgw_systemd_unit_is_current(c);
        if (strcmp(op, "cgw_u_temp_home_in_service_definition") == 0) o = emit_cgw_u_temp_home_in_service_definition(c);
        if (strcmp(op, "cgw_u_refuse_temp_home_service_write") == 0) o = emit_cgw_u_refuse_temp_home_service_write(c);
        if (strcmp(op, "cgw_refresh_systemd_unit_if_needed") == 0) o = emit_cgw_refresh_systemd_unit_if_needed(c);
        if (strcmp(op, "cgw_u_print_linger_enable_warning") == 0) o = emit_cgw_u_print_linger_enable_warning(c);
        if (strcmp(op, "cgw_u_ensure_linger_enabled") == 0) o = emit_cgw_u_ensure_linger_enabled(c);
        if (strcmp(op, "cgw_u_select_systemd_scope") == 0) o = emit_cgw_u_select_systemd_scope(c);
        if (strcmp(op, "cgw_u_system_scope_wizard_would_need_root") == 0) o = emit_cgw_u_system_scope_wizard_would_need_root(c);
        if (strcmp(op, "cgw_u_print_system_scope_remediation") == 0) o = emit_cgw_u_print_system_scope_remediation(c);
        if (strcmp(op, "cgw_u_get_restart_drain_timeout") == 0) o = emit_cgw_u_get_restart_drain_timeout(c);
        if (strcmp(op, "cgw_systemd_install") == 0) o = emit_cgw_systemd_install(c);
        if (strcmp(op, "cgw_systemd_uninstall") == 0) o = emit_cgw_systemd_uninstall(c);
        if (strcmp(op, "cgw_u_require_service_installed") == 0) o = emit_cgw_u_require_service_installed(c);
        if (strcmp(op, "cgw_systemd_start") == 0) o = emit_cgw_systemd_start(c);
        if (strcmp(op, "cgw_systemd_stop") == 0) o = emit_cgw_systemd_stop(c);
        if (strcmp(op, "cgw_systemd_restart") == 0) o = emit_cgw_systemd_restart(c);
        if (strcmp(op, "cgw_systemd_status") == 0) o = emit_cgw_systemd_status(c);
        if (strcmp(op, "cgw_get_launchd_label") == 0) o = emit_cgw_get_launchd_label(c);
        if (strcmp(op, "cgw_u_launchd_domain") == 0) o = emit_cgw_u_launchd_domain(c);
        if (strcmp(op, "cgw_u_launchd_error_indicates_unloaded") == 0) o = emit_cgw_u_launchd_error_indicates_unloaded(c);
        if (strcmp(op, "cgw_u_launchctl_domain_unsupported") == 0) o = emit_cgw_u_launchctl_domain_unsupported(c);
        if (strcmp(op, "cgw_u_launchctl_bootstrap") == 0) o = emit_cgw_u_launchctl_bootstrap(c);
        if (strcmp(op, "cgw_u_launchd_reload_log_path") == 0) o = emit_cgw_u_launchd_reload_log_path(c);
        if (strcmp(op, "cgw_u_append_launchd_reload_log") == 0) o = emit_cgw_u_append_launchd_reload_log(c);
        if (strcmp(op, "cgw_u_launchctl_label_registered") == 0) o = emit_cgw_u_launchctl_label_registered(c);
        if (strcmp(op, "cgw_u_retry_launchctl_bootstrap_until_registered") == 0) o = emit_cgw_u_retry_launchctl_bootstrap_until_registered(c);
        if (strcmp(op, "cgw_u_launchd_unsupported_marker_path") == 0) o = emit_cgw_u_launchd_unsupported_marker_path(c);
        if (strcmp(op, "cgw_u_write_launchd_unsupported_marker") == 0) o = emit_cgw_u_write_launchd_unsupported_marker(c);
        if (strcmp(op, "cgw_u_clear_launchd_unsupported_marker") == 0) o = emit_cgw_u_clear_launchd_unsupported_marker(c);
        if (strcmp(op, "cgw_u_launchd_unsupported_marker_exists") == 0) o = emit_cgw_u_launchd_unsupported_marker_exists(c);
        if (strcmp(op, "cgw_u_gateway_run_command") == 0) o = emit_cgw_u_gateway_run_command(c);
        if (strcmp(op, "cgw_u_spawn_detached_gateway") == 0) o = emit_cgw_u_spawn_detached_gateway(c);
        if (strcmp(op, "cgw_u_launchd_fallback_to_detached") == 0) o = emit_cgw_u_launchd_fallback_to_detached(c);
        if (strcmp(op, "cgw_generate_launchd_plist") == 0) o = emit_cgw_generate_launchd_plist(c);
        if (strcmp(op, "cgw_launchd_plist_is_current") == 0) o = emit_cgw_launchd_plist_is_current(c);
        if (strcmp(op, "cgw_refresh_launchd_plist_if_needed") == 0) o = emit_cgw_refresh_launchd_plist_if_needed(c);
        if (strcmp(op, "cgw_launchd_install") == 0) o = emit_cgw_launchd_install(c);
        if (strcmp(op, "cgw_launchd_uninstall") == 0) o = emit_cgw_launchd_uninstall(c);
        if (strcmp(op, "cgw_launchd_start") == 0) o = emit_cgw_launchd_start(c);
        if (strcmp(op, "cgw_launchd_stop") == 0) o = emit_cgw_launchd_stop(c);
        if (strcmp(op, "cgw_u_wait_for_gateway_exit") == 0) o = emit_cgw_u_wait_for_gateway_exit(c);
        if (strcmp(op, "cgw_launchd_restart") == 0) o = emit_cgw_launchd_restart(c);
        if (strcmp(op, "cgw_launchd_status") == 0) o = emit_cgw_launchd_status(c);
        if (strcmp(op, "cgw_u_truthy_env") == 0) o = emit_cgw_u_truthy_env(c);
        if (strcmp(op, "cgw_u_is_official_docker_checkout") == 0) o = emit_cgw_u_is_official_docker_checkout(c);
        if (strcmp(op, "cgw_u_running_under_gateway_supervisor") == 0) o = emit_cgw_u_running_under_gateway_supervisor(c);
        if (strcmp(op, "cgw_u_guard_supervised_gateway_conflict") == 0) o = emit_cgw_u_guard_supervised_gateway_conflict(c);
        if (strcmp(op, "cgw_u_guard_existing_gateway_process_conflict") == 0) o = emit_cgw_u_guard_existing_gateway_process_conflict(c);
        if (strcmp(op, "cgw_u_guard_official_docker_root_gateway") == 0) o = emit_cgw_u_guard_official_docker_root_gateway(c);
        if (strcmp(op, "cgw_u_all_platforms") == 0) o = emit_cgw_u_all_platforms(c);
        if (strcmp(op, "cgw_u_platform_status") == 0) o = emit_cgw_u_platform_status(c);
        if (strcmp(op, "cgw_u_runtime_health_lines") == 0) o = emit_cgw_u_runtime_health_lines(c);
        if (strcmp(op, "cgw_u_set_platform_unauthorized_dm_behavior") == 0) o = emit_cgw_u_set_platform_unauthorized_dm_behavior(c);
        if (strcmp(op, "cgw_u_setup_standard_platform") == 0) o = emit_cgw_u_setup_standard_platform(c);
        if (strcmp(op, "cgw_u_is_service_installed") == 0) o = emit_cgw_u_is_service_installed(c);
        if (strcmp(op, "cgw_u_is_service_running") == 0) o = emit_cgw_u_is_service_running(c);
        if (strcmp(op, "cgw_u_builtin_setup_fn") == 0) o = emit_cgw_u_builtin_setup_fn(c);
        if (strcmp(op, "cgw_u_configure_platform") == 0) o = emit_cgw_u_configure_platform(c);
        if (strcmp(op, "cgw_u_dispatch_via_service_manager_if_s6") == 0) o = emit_cgw_u_dispatch_via_service_manager_if_s6(c);
        if (strcmp(op, "cgw_u_dispatch_all_via_service_manager_if_s6") == 0) o = emit_cgw_u_dispatch_all_via_service_manager_if_s6(c);
        if (strcmp(op, "cgw_u_maybe_redirect_run_to_s6_supervision") == 0) o = emit_cgw_u_maybe_redirect_run_to_s6_supervision(c);
        if (strcmp(op, "cgw_u_block_until_terminated") == 0) o = emit_cgw_u_block_until_terminated(c);
        if (strcmp(op, "cgw_u_gateway_command_inner") == 0) o = emit_cgw_u_gateway_command_inner(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
