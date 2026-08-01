"""AUTO-GENERATED integration oracle for port_cli_gateway_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.gateway'] = _load('hermes_cli/gateway.py')

DISPATCH = {
    'cgw_has_process_service_mismatch': ('hermes_cli.gateway', 'has_process_service_mismatch'),
    'cgw_u_scan_gateway_pids': ('hermes_cli.gateway', '_scan_gateway_pids'),
    'cgw_u_filter_venv_launcher_stubs': ('hermes_cli.gateway', '_filter_venv_launcher_stubs'),
    'cgw_find_profile_gateway_processes': ('hermes_cli.gateway', 'find_profile_gateway_processes'),
    'cgw_u_gateway_run_args_for_profile': ('hermes_cli.gateway', '_gateway_run_args_for_profile'),
    'cgw_u_prepare_profile_gateway_update_restart': ('hermes_cli.gateway', '_prepare_profile_gateway_update_restart'),
    'cgw_launch_detached_profile_gateway_restart': ('hermes_cli.gateway', 'launch_detached_profile_gateway_restart'),
    'cgw_u_probe_systemd_service_running': ('hermes_cli.gateway', '_probe_systemd_service_running'),
    'cgw_u_read_systemd_unit_environment': ('hermes_cli.gateway', '_read_systemd_unit_environment'),
    'cgw_u_hermes_home_from_systemd_unit_file': ('hermes_cli.gateway', '_hermes_home_from_systemd_unit_file'),
    'cgw_u_sync_hermes_home_from_systemd_unit': ('hermes_cli.gateway', '_sync_hermes_home_from_systemd_unit'),
    'cgw_u_read_systemd_unit_properties': ('hermes_cli.gateway', '_read_systemd_unit_properties'),
    'cgw_u_systemd_main_pid_from_props': ('hermes_cli.gateway', '_systemd_main_pid_from_props'),
    'cgw_u_systemd_main_pid': ('hermes_cli.gateway', '_systemd_main_pid'),
    'cgw_u_read_gateway_runtime_status': ('hermes_cli.gateway', '_read_gateway_runtime_status'),
    'cgw_u_gateway_runtime_status_for_pid': ('hermes_cli.gateway', '_gateway_runtime_status_for_pid'),
    'cgw_u_wait_for_systemd_service_restart': ('hermes_cli.gateway', '_wait_for_systemd_service_restart'),
    'cgw_u_systemd_unit_is_start_limited': ('hermes_cli.gateway', '_systemd_unit_is_start_limited'),
    'cgw_u_systemd_error_indicates_start_limit': ('hermes_cli.gateway', '_systemd_error_indicates_start_limit'),
    'cgw_u_systemd_service_is_start_limited': ('hermes_cli.gateway', '_systemd_service_is_start_limited'),
    'cgw_u_print_systemd_start_limit_wait': ('hermes_cli.gateway', '_print_systemd_start_limit_wait'),
    'cgw_u_recover_pending_systemd_restart': ('hermes_cli.gateway', '_recover_pending_systemd_restart'),
    'cgw_u_parse_launchd_pid_from_list_output': ('hermes_cli.gateway', '_parse_launchd_pid_from_list_output'),
    'cgw_u_probe_launchd_service_running': ('hermes_cli.gateway', '_probe_launchd_service_running'),
    'cgw_get_gateway_runtime_snapshot': ('hermes_cli.gateway', 'get_gateway_runtime_snapshot'),
    'cgw_u_format_gateway_pids': ('hermes_cli.gateway', '_format_gateway_pids'),
    'cgw_u_print_gateway_process_mismatch': ('hermes_cli.gateway', '_print_gateway_process_mismatch'),
    'cgw_u_print_other_profiles_gateway_status': ('hermes_cli.gateway', '_print_other_profiles_gateway_status'),
    'cgw_u_reap_unsupervised_gateway_orphans': ('hermes_cli.gateway', '_reap_unsupervised_gateway_orphans'),
    'cgw_u_wsl_systemd_operational': ('hermes_cli.gateway', '_wsl_systemd_operational'),
    'cgw_u_systemd_operational': ('hermes_cli.gateway', '_systemd_operational'),
    'cgw_u_container_systemd_operational': ('hermes_cli.gateway', '_container_systemd_operational'),
    'cgw_u_windows_gateway_should_absorb_console_controls': ('hermes_cli.gateway', '_windows_gateway_should_absorb_console_controls'),
    'cgw_u_profile_arg_for_target_user': ('hermes_cli.gateway', '_profile_arg_for_target_user'),
    'cgw_get_service_name': ('hermes_cli.gateway', 'get_service_name'),
    'cgw_get_systemd_unit_path': ('hermes_cli.gateway', 'get_systemd_unit_path'),
    'cgw_u_user_dbus_socket_path': ('hermes_cli.gateway', '_user_dbus_socket_path'),
    'cgw_u_user_systemd_private_socket_path': ('hermes_cli.gateway', '_user_systemd_private_socket_path'),
    'cgw_u_user_systemd_socket_ready': ('hermes_cli.gateway', '_user_systemd_socket_ready'),
    'cgw_u_ensure_user_systemd_env': ('hermes_cli.gateway', '_ensure_user_systemd_env'),
    'cgw_u_wait_for_user_dbus_socket': ('hermes_cli.gateway', '_wait_for_user_dbus_socket'),
    'cgw_u_preflight_user_systemd': ('hermes_cli.gateway', '_preflight_user_systemd'),
    'cgw_u_raise_user_systemd_unavailable': ('hermes_cli.gateway', '_raise_user_systemd_unavailable'),
    'cgw_u_systemctl_cmd': ('hermes_cli.gateway', '_systemctl_cmd'),
    'cgw_u_journalctl_cmd': ('hermes_cli.gateway', '_journalctl_cmd'),
    'cgw_u_run_systemctl': ('hermes_cli.gateway', '_run_systemctl'),
    'cgw_u_service_scope_label': ('hermes_cli.gateway', '_service_scope_label'),
    'cgw_get_installed_systemd_scopes': ('hermes_cli.gateway', 'get_installed_systemd_scopes'),
    'cgw_has_conflicting_systemd_units': ('hermes_cli.gateway', 'has_conflicting_systemd_units'),
    'cgw_u_legacy_unit_search_paths': ('hermes_cli.gateway', '_legacy_unit_search_paths'),
    'cgw_u_find_legacy_hermes_units': ('hermes_cli.gateway', '_find_legacy_hermes_units'),
    'cgw_has_legacy_hermes_units': ('hermes_cli.gateway', 'has_legacy_hermes_units'),
    'cgw_print_legacy_unit_warning': ('hermes_cli.gateway', 'print_legacy_unit_warning'),
    'cgw_remove_legacy_hermes_units': ('hermes_cli.gateway', 'remove_legacy_hermes_units'),
    'cgw_print_systemd_scope_conflict_warning': ('hermes_cli.gateway', 'print_systemd_scope_conflict_warning'),
    'cgw_u_require_root_for_system_service': ('hermes_cli.gateway', '_require_root_for_system_service'),
    'cgw_u_system_service_identity': ('hermes_cli.gateway', '_system_service_identity'),
    'cgw_u_read_systemd_user_from_unit': ('hermes_cli.gateway', '_read_systemd_user_from_unit'),
    'cgw_u_default_system_service_user': ('hermes_cli.gateway', '_default_system_service_user'),
    'cgw_prompt_linux_gateway_install_scope': ('hermes_cli.gateway', 'prompt_linux_gateway_install_scope'),
    'cgw_install_linux_gateway_from_setup': ('hermes_cli.gateway', 'install_linux_gateway_from_setup'),
    'cgw_get_systemd_linger_status': ('hermes_cli.gateway', 'get_systemd_linger_status'),
    'cgw_print_systemd_linger_guidance': ('hermes_cli.gateway', 'print_systemd_linger_guidance'),
    'cgw_u_launchd_user_home': ('hermes_cli.gateway', '_launchd_user_home'),
    'cgw_get_launchd_plist_path': ('hermes_cli.gateway', 'get_launchd_plist_path'),
    'cgw_u_detect_venv_dir': ('hermes_cli.gateway', '_detect_venv_dir'),
    'cgw_get_python_path': ('hermes_cli.gateway', 'get_python_path'),
    'cgw_u_build_user_local_paths': ('hermes_cli.gateway', '_build_user_local_paths'),
    'cgw_u_build_wsl_interop_paths': ('hermes_cli.gateway', '_build_wsl_interop_paths'),
    'cgw_u_remap_path_for_user': ('hermes_cli.gateway', '_remap_path_for_user'),
    'cgw_u_hermes_home_for_target_user': ('hermes_cli.gateway', '_hermes_home_for_target_user'),
    'cgw_u_build_service_path_dirs': ('hermes_cli.gateway', '_build_service_path_dirs'),
    'cgw_u_stable_service_working_dir': ('hermes_cli.gateway', '_stable_service_working_dir'),
    'cgw_u_systemd_watchdog_seconds': ('hermes_cli.gateway', '_systemd_watchdog_seconds'),
    'cgw_u_systemd_watchdog_service_fields': ('hermes_cli.gateway', '_systemd_watchdog_service_fields'),
    'cgw_generate_systemd_unit': ('hermes_cli.gateway', 'generate_systemd_unit'),
    'cgw_u_normalize_service_definition': ('hermes_cli.gateway', '_normalize_service_definition'),
    'cgw_u_strip_optional_systemd_directives': ('hermes_cli.gateway', '_strip_optional_systemd_directives'),
    'cgw_u_normalize_launchd_plist_for_comparison': ('hermes_cli.gateway', '_normalize_launchd_plist_for_comparison'),
    'cgw_systemd_unit_is_current': ('hermes_cli.gateway', 'systemd_unit_is_current'),
    'cgw_u_temp_home_in_service_definition': ('hermes_cli.gateway', '_temp_home_in_service_definition'),
    'cgw_u_refuse_temp_home_service_write': ('hermes_cli.gateway', '_refuse_temp_home_service_write'),
    'cgw_refresh_systemd_unit_if_needed': ('hermes_cli.gateway', 'refresh_systemd_unit_if_needed'),
    'cgw_u_print_linger_enable_warning': ('hermes_cli.gateway', '_print_linger_enable_warning'),
    'cgw_u_ensure_linger_enabled': ('hermes_cli.gateway', '_ensure_linger_enabled'),
    'cgw_u_select_systemd_scope': ('hermes_cli.gateway', '_select_systemd_scope'),
    'cgw_u_system_scope_wizard_would_need_root': ('hermes_cli.gateway', '_system_scope_wizard_would_need_root'),
    'cgw_u_print_system_scope_remediation': ('hermes_cli.gateway', '_print_system_scope_remediation'),
    'cgw_u_get_restart_drain_timeout': ('hermes_cli.gateway', '_get_restart_drain_timeout'),
    'cgw_systemd_install': ('hermes_cli.gateway', 'systemd_install'),
    'cgw_systemd_uninstall': ('hermes_cli.gateway', 'systemd_uninstall'),
    'cgw_u_require_service_installed': ('hermes_cli.gateway', '_require_service_installed'),
    'cgw_systemd_start': ('hermes_cli.gateway', 'systemd_start'),
    'cgw_systemd_stop': ('hermes_cli.gateway', 'systemd_stop'),
    'cgw_systemd_restart': ('hermes_cli.gateway', 'systemd_restart'),
    'cgw_systemd_status': ('hermes_cli.gateway', 'systemd_status'),
    'cgw_get_launchd_label': ('hermes_cli.gateway', 'get_launchd_label'),
    'cgw_u_launchd_domain': ('hermes_cli.gateway', '_launchd_domain'),
    'cgw_u_launchd_error_indicates_unloaded': ('hermes_cli.gateway', '_launchd_error_indicates_unloaded'),
    'cgw_u_launchctl_domain_unsupported': ('hermes_cli.gateway', '_launchctl_domain_unsupported'),
    'cgw_u_launchctl_bootstrap': ('hermes_cli.gateway', '_launchctl_bootstrap'),
    'cgw_u_launchd_reload_log_path': ('hermes_cli.gateway', '_launchd_reload_log_path'),
    'cgw_u_append_launchd_reload_log': ('hermes_cli.gateway', '_append_launchd_reload_log'),
    'cgw_u_launchctl_label_registered': ('hermes_cli.gateway', '_launchctl_label_registered'),
    'cgw_u_retry_launchctl_bootstrap_until_registered': ('hermes_cli.gateway', '_retry_launchctl_bootstrap_until_registered'),
    'cgw_u_launchd_unsupported_marker_path': ('hermes_cli.gateway', '_launchd_unsupported_marker_path'),
    'cgw_u_write_launchd_unsupported_marker': ('hermes_cli.gateway', '_write_launchd_unsupported_marker'),
    'cgw_u_clear_launchd_unsupported_marker': ('hermes_cli.gateway', '_clear_launchd_unsupported_marker'),
    'cgw_u_launchd_unsupported_marker_exists': ('hermes_cli.gateway', '_launchd_unsupported_marker_exists'),
    'cgw_u_gateway_run_command': ('hermes_cli.gateway', '_gateway_run_command'),
    'cgw_u_spawn_detached_gateway': ('hermes_cli.gateway', '_spawn_detached_gateway'),
    'cgw_u_launchd_fallback_to_detached': ('hermes_cli.gateway', '_launchd_fallback_to_detached'),
    'cgw_generate_launchd_plist': ('hermes_cli.gateway', 'generate_launchd_plist'),
    'cgw_launchd_plist_is_current': ('hermes_cli.gateway', 'launchd_plist_is_current'),
    'cgw_refresh_launchd_plist_if_needed': ('hermes_cli.gateway', 'refresh_launchd_plist_if_needed'),
    'cgw_launchd_install': ('hermes_cli.gateway', 'launchd_install'),
    'cgw_launchd_uninstall': ('hermes_cli.gateway', 'launchd_uninstall'),
    'cgw_launchd_start': ('hermes_cli.gateway', 'launchd_start'),
    'cgw_launchd_stop': ('hermes_cli.gateway', 'launchd_stop'),
    'cgw_u_wait_for_gateway_exit': ('hermes_cli.gateway', '_wait_for_gateway_exit'),
    'cgw_launchd_restart': ('hermes_cli.gateway', 'launchd_restart'),
    'cgw_launchd_status': ('hermes_cli.gateway', 'launchd_status'),
    'cgw_u_truthy_env': ('hermes_cli.gateway', '_truthy_env'),
    'cgw_u_is_official_docker_checkout': ('hermes_cli.gateway', '_is_official_docker_checkout'),
    'cgw_u_running_under_gateway_supervisor': ('hermes_cli.gateway', '_running_under_gateway_supervisor'),
    'cgw_u_guard_supervised_gateway_conflict': ('hermes_cli.gateway', '_guard_supervised_gateway_conflict'),
    'cgw_u_guard_existing_gateway_process_conflict': ('hermes_cli.gateway', '_guard_existing_gateway_process_conflict'),
    'cgw_u_guard_official_docker_root_gateway': ('hermes_cli.gateway', '_guard_official_docker_root_gateway'),
    'cgw_u_all_platforms': ('hermes_cli.gateway', '_all_platforms'),
    'cgw_u_platform_status': ('hermes_cli.gateway', '_platform_status'),
    'cgw_u_runtime_health_lines': ('hermes_cli.gateway', '_runtime_health_lines'),
    'cgw_u_set_platform_unauthorized_dm_behavior': ('hermes_cli.gateway', '_set_platform_unauthorized_dm_behavior'),
    'cgw_u_setup_standard_platform': ('hermes_cli.gateway', '_setup_standard_platform'),
    'cgw_u_is_service_installed': ('hermes_cli.gateway', '_is_service_installed'),
    'cgw_u_is_service_running': ('hermes_cli.gateway', '_is_service_running'),
    'cgw_u_builtin_setup_fn': ('hermes_cli.gateway', '_builtin_setup_fn'),
    'cgw_u_configure_platform': ('hermes_cli.gateway', '_configure_platform'),
    'cgw_u_dispatch_via_service_manager_if_s6': ('hermes_cli.gateway', '_dispatch_via_service_manager_if_s6'),
    'cgw_u_dispatch_all_via_service_manager_if_s6': ('hermes_cli.gateway', '_dispatch_all_via_service_manager_if_s6'),
    'cgw_u_maybe_redirect_run_to_s6_supervision': ('hermes_cli.gateway', '_maybe_redirect_run_to_s6_supervision'),
    'cgw_u_block_until_terminated': ('hermes_cli.gateway', '_block_until_terminated'),
    'cgw_u_gateway_command_inner': ('hermes_cli.gateway', '_gateway_command_inner'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_cli_gateway_wrappers.py <cases.json>\n"); return 2
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
