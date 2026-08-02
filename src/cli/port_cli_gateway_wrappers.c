/*
 * port_cli_gateway_wrappers.c — C port of hermes_cli/gateway.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: has_process_service_mismatch @ hermes_cli/gateway.py:has_process_service_mismatch */
int cgw_has_process_service_mismatch(const char *arg) { (void)arg; return 0; }

/* PoP: _scan_gateway_pids @ hermes_cli/gateway.py:_scan_gateway_pids */
int cgw_u_scan_gateway_pids(const char *arg) { (void)arg; return 0; }

/* PoP: _filter_venv_launcher_stubs @ hermes_cli/gateway.py:_filter_venv_launcher_stubs */
int cgw_u_filter_venv_launcher_stubs(const char *arg) { (void)arg; return 0; }

/* PoP: find_profile_gateway_processes @ hermes_cli/gateway.py:find_profile_gateway_processes */
int cgw_find_profile_gateway_processes(const char *arg) { (void)arg; return 0; }

/* PoP: _gateway_run_args_for_profile @ hermes_cli/gateway.py:_gateway_run_args_for_profile */
int cgw_u_gateway_run_args_for_profile(const char *arg) { (void)arg; return 0; }

/* PoP: _prepare_profile_gateway_update_restart @ hermes_cli/gateway.py:_prepare_profile_gateway_update_restart */
int cgw_u_prepare_profile_gateway_update_restart(const char *arg) { (void)arg; return 0; }

/* PoP: launch_detached_profile_gateway_restart @ hermes_cli/gateway.py:launch_detached_profile_gateway_restart */
int cgw_launch_detached_profile_gateway_restart(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_systemd_service_running @ hermes_cli/gateway.py:_probe_systemd_service_running */
int cgw_u_probe_systemd_service_running(const char *arg) { (void)arg; return 0; }

/* PoP: _read_systemd_unit_environment @ hermes_cli/gateway.py:_read_systemd_unit_environment */
int cgw_u_read_systemd_unit_environment(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_home_from_systemd_unit_file @ hermes_cli/gateway.py:_hermes_home_from_systemd_unit_file */
int cgw_u_hermes_home_from_systemd_unit_file(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_hermes_home_from_systemd_unit @ hermes_cli/gateway.py:_sync_hermes_home_from_systemd_unit */
int cgw_u_sync_hermes_home_from_systemd_unit(const char *arg) { (void)arg; return 0; }

/* PoP: _read_systemd_unit_properties @ hermes_cli/gateway.py:_read_systemd_unit_properties */
int cgw_u_read_systemd_unit_properties(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_main_pid_from_props @ hermes_cli/gateway.py:_systemd_main_pid_from_props */
int cgw_u_systemd_main_pid_from_props(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_main_pid @ hermes_cli/gateway.py:_systemd_main_pid */
int cgw_u_systemd_main_pid(const char *arg) { (void)arg; return 0; }

/* PoP: _read_gateway_runtime_status @ hermes_cli/gateway.py:_read_gateway_runtime_status */
int cgw_u_read_gateway_runtime_status(const char *arg) { (void)arg; return 0; }

/* PoP: _gateway_runtime_status_for_pid @ hermes_cli/gateway.py:_gateway_runtime_status_for_pid */
int cgw_u_gateway_runtime_status_for_pid(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_systemd_service_restart @ hermes_cli/gateway.py:_wait_for_systemd_service_restart */
int cgw_u_wait_for_systemd_service_restart(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_unit_is_start_limited @ hermes_cli/gateway.py:_systemd_unit_is_start_limited */
int cgw_u_systemd_unit_is_start_limited(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_error_indicates_start_limit @ hermes_cli/gateway.py:_systemd_error_indicates_start_limit */
int cgw_u_systemd_error_indicates_start_limit(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_service_is_start_limited @ hermes_cli/gateway.py:_systemd_service_is_start_limited */
int cgw_u_systemd_service_is_start_limited(const char *arg) { (void)arg; return 0; }

/* PoP: _print_systemd_start_limit_wait @ hermes_cli/gateway.py:_print_systemd_start_limit_wait */
int cgw_u_print_systemd_start_limit_wait(const char *arg) { (void)arg; return 0; }

/* PoP: _recover_pending_systemd_restart @ hermes_cli/gateway.py:_recover_pending_systemd_restart */
int cgw_u_recover_pending_systemd_restart(const char *arg) { (void)arg; return 0; }

/* PoP: _parse_launchd_pid_from_list_output @ hermes_cli/gateway.py:_parse_launchd_pid_from_list_output */
int cgw_u_parse_launchd_pid_from_list_output(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_launchd_service_running @ hermes_cli/gateway.py:_probe_launchd_service_running */
int cgw_u_probe_launchd_service_running(const char *arg) { (void)arg; return 0; }

/* PoP: get_gateway_runtime_snapshot @ hermes_cli/gateway.py:get_gateway_runtime_snapshot */
int cgw_get_gateway_runtime_snapshot(const char *arg) { (void)arg; return 0; }

/* PoP: _format_gateway_pids @ hermes_cli/gateway.py:_format_gateway_pids */
int cgw_u_format_gateway_pids(const char *arg) { (void)arg; return 0; }

/* PoP: _print_gateway_process_mismatch @ hermes_cli/gateway.py:_print_gateway_process_mismatch */
int cgw_u_print_gateway_process_mismatch(const char *arg) { (void)arg; return 0; }

/* PoP: _print_other_profiles_gateway_status @ hermes_cli/gateway.py:_print_other_profiles_gateway_status */
int cgw_u_print_other_profiles_gateway_status(const char *arg) { (void)arg; return 0; }

/* PoP: _reap_unsupervised_gateway_orphans @ hermes_cli/gateway.py:_reap_unsupervised_gateway_orphans */
int cgw_u_reap_unsupervised_gateway_orphans(const char *arg) { (void)arg; return 0; }

/* PoP: _wsl_systemd_operational @ hermes_cli/gateway.py:_wsl_systemd_operational */
int cgw_u_wsl_systemd_operational(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_operational @ hermes_cli/gateway.py:_systemd_operational */
int cgw_u_systemd_operational(const char *arg) { (void)arg; return 0; }

/* PoP: _container_systemd_operational @ hermes_cli/gateway.py:_container_systemd_operational */
int cgw_u_container_systemd_operational(const char *arg) { (void)arg; return 0; }

/* PoP: _windows_gateway_should_absorb_console_controls @ hermes_cli/gateway.py:_windows_gateway_should_absorb_console_controls */
int cgw_u_windows_gateway_should_absorb_console_controls(const char *arg) { (void)arg; return 0; }

/* PoP: _profile_arg_for_target_user @ hermes_cli/gateway.py:_profile_arg_for_target_user */
int cgw_u_profile_arg_for_target_user(const char *arg) { (void)arg; return 0; }

/* PoP: get_service_name @ hermes_cli/gateway.py:get_service_name */
int cgw_get_service_name(const char *arg) { (void)arg; return 0; }

/* PoP: get_systemd_unit_path @ hermes_cli/gateway.py:get_systemd_unit_path */
int cgw_get_systemd_unit_path(const char *arg) { (void)arg; return 0; }

/* PoP: _user_dbus_socket_path @ hermes_cli/gateway.py:_user_dbus_socket_path */
int cgw_u_user_dbus_socket_path(const char *arg) { (void)arg; return 0; }

/* PoP: _user_systemd_private_socket_path @ hermes_cli/gateway.py:_user_systemd_private_socket_path */
int cgw_u_user_systemd_private_socket_path(const char *arg) { (void)arg; return 0; }

/* PoP: _user_systemd_socket_ready @ hermes_cli/gateway.py:_user_systemd_socket_ready */
int cgw_u_user_systemd_socket_ready(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_user_systemd_env @ hermes_cli/gateway.py:_ensure_user_systemd_env */
int cgw_u_ensure_user_systemd_env(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_user_dbus_socket @ hermes_cli/gateway.py:_wait_for_user_dbus_socket */
int cgw_u_wait_for_user_dbus_socket(const char *arg) { (void)arg; return 0; }

/* PoP: _preflight_user_systemd @ hermes_cli/gateway.py:_preflight_user_systemd */
int cgw_u_preflight_user_systemd(const char *arg) { (void)arg; return 0; }

/* PoP: _raise_user_systemd_unavailable @ hermes_cli/gateway.py:_raise_user_systemd_unavailable */
int cgw_u_raise_user_systemd_unavailable(const char *arg) { (void)arg; return 0; }

/* PoP: _systemctl_cmd @ hermes_cli/gateway.py:_systemctl_cmd */
int cgw_u_systemctl_cmd(const char *arg) { (void)arg; return 0; }

/* PoP: _journalctl_cmd @ hermes_cli/gateway.py:_journalctl_cmd */
int cgw_u_journalctl_cmd(const char *arg) { (void)arg; return 0; }

/* PoP: _run_systemctl @ hermes_cli/gateway.py:_run_systemctl */
int cgw_u_run_systemctl(const char *arg) { (void)arg; return 0; }

/* PoP: _service_scope_label @ hermes_cli/gateway.py:_service_scope_label */
int cgw_u_service_scope_label(const char *arg) { (void)arg; return 0; }

/* PoP: get_installed_systemd_scopes @ hermes_cli/gateway.py:get_installed_systemd_scopes */
int cgw_get_installed_systemd_scopes(const char *arg) { (void)arg; return 0; }

/* PoP: has_conflicting_systemd_units @ hermes_cli/gateway.py:has_conflicting_systemd_units */
int cgw_has_conflicting_systemd_units(const char *arg) { (void)arg; return 0; }

/* PoP: _legacy_unit_search_paths @ hermes_cli/gateway.py:_legacy_unit_search_paths */
int cgw_u_legacy_unit_search_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _find_legacy_hermes_units @ hermes_cli/gateway.py:_find_legacy_hermes_units */
int cgw_u_find_legacy_hermes_units(const char *arg) { (void)arg; return 0; }

/* PoP: has_legacy_hermes_units @ hermes_cli/gateway.py:has_legacy_hermes_units */
int cgw_has_legacy_hermes_units(const char *arg) { (void)arg; return 0; }

/* PoP: print_legacy_unit_warning @ hermes_cli/gateway.py:print_legacy_unit_warning */
int cgw_print_legacy_unit_warning(const char *arg) { (void)arg; return 0; }

/* PoP: remove_legacy_hermes_units @ hermes_cli/gateway.py:remove_legacy_hermes_units */
int cgw_remove_legacy_hermes_units(const char *arg) { (void)arg; return 0; }

/* PoP: print_systemd_scope_conflict_warning @ hermes_cli/gateway.py:print_systemd_scope_conflict_warning */
int cgw_print_systemd_scope_conflict_warning(const char *arg) { (void)arg; return 0; }

/* PoP: _require_root_for_system_service @ hermes_cli/gateway.py:_require_root_for_system_service */
int cgw_u_require_root_for_system_service(const char *arg) { (void)arg; return 0; }

/* PoP: _system_service_identity @ hermes_cli/gateway.py:_system_service_identity */
int cgw_u_system_service_identity(const char *arg) { (void)arg; return 0; }

/* PoP: _read_systemd_user_from_unit @ hermes_cli/gateway.py:_read_systemd_user_from_unit */
int cgw_u_read_systemd_user_from_unit(const char *arg) { (void)arg; return 0; }

/* PoP: _default_system_service_user @ hermes_cli/gateway.py:_default_system_service_user */
int cgw_u_default_system_service_user(const char *arg) {
    /* Python: first of SUDO_USER/USER/LOGNAME that is non-empty and not
     * "root"; None otherwise. */
    (void)arg;
    static const char *const names[] = {"SUDO_USER", "USER", "LOGNAME", NULL};
    for (int i = 0; names[i]; i++) {
        const char *v = getenv(names[i]);
        if (!v) continue;
        const char *s = v;
        while (*s && isspace((unsigned char)*s)) s++;
        size_t n = strlen(s);
        while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
        if (n == 0 || (n == 4 && strncasecmp(s, "root", 4) == 0)) continue;
        printf("%.*s\n", (int)n, s);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: prompt_linux_gateway_install_scope @ hermes_cli/gateway.py:prompt_linux_gateway_install_scope */
int cgw_prompt_linux_gateway_install_scope(const char *arg) { (void)arg; return 0; }

/* PoP: install_linux_gateway_from_setup @ hermes_cli/gateway.py:install_linux_gateway_from_setup */
int cgw_install_linux_gateway_from_setup(const char *arg) { (void)arg; return 0; }

/* PoP: get_systemd_linger_status @ hermes_cli/gateway.py:get_systemd_linger_status */
int cgw_get_systemd_linger_status(const char *arg) { (void)arg; return 0; }

/* PoP: print_systemd_linger_guidance @ hermes_cli/gateway.py:print_systemd_linger_guidance */
int cgw_print_systemd_linger_guidance(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_user_home @ hermes_cli/gateway.py:_launchd_user_home */
int cgw_u_launchd_user_home(const char *arg) { (void)arg; return 0; }

/* PoP: get_launchd_plist_path @ hermes_cli/gateway.py:get_launchd_plist_path */
int cgw_get_launchd_plist_path(const char *arg) { (void)arg; return 0; }

/* PoP: _detect_venv_dir @ hermes_cli/gateway.py:_detect_venv_dir */
int cgw_u_detect_venv_dir(const char *arg) { (void)arg; return 0; }

/* PoP: get_python_path @ hermes_cli/gateway.py:get_python_path */
int cgw_get_python_path(const char *arg) { (void)arg; return 0; }

/* PoP: _build_user_local_paths @ hermes_cli/gateway.py:_build_user_local_paths */
int cgw_u_build_user_local_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _build_wsl_interop_paths @ hermes_cli/gateway.py:_build_wsl_interop_paths */
int cgw_u_build_wsl_interop_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _remap_path_for_user @ hermes_cli/gateway.py:_remap_path_for_user */
int cgw_u_remap_path_for_user(const char *arg) { (void)arg; return 0; }

/* PoP: _hermes_home_for_target_user @ hermes_cli/gateway.py:_hermes_home_for_target_user */
int cgw_u_hermes_home_for_target_user(const char *arg) { (void)arg; return 0; }

/* PoP: _build_service_path_dirs @ hermes_cli/gateway.py:_build_service_path_dirs */
int cgw_u_build_service_path_dirs(const char *arg) { (void)arg; return 0; }

/* PoP: _stable_service_working_dir @ hermes_cli/gateway.py:_stable_service_working_dir */
int cgw_u_stable_service_working_dir(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_watchdog_seconds @ hermes_cli/gateway.py:_systemd_watchdog_seconds */
int cgw_u_systemd_watchdog_seconds(const char *arg) { (void)arg; return 0; }

/* PoP: _systemd_watchdog_service_fields @ hermes_cli/gateway.py:_systemd_watchdog_service_fields */
int cgw_u_systemd_watchdog_service_fields(const char *arg) { (void)arg; return 0; }

/* PoP: generate_systemd_unit @ hermes_cli/gateway.py:generate_systemd_unit */
int cgw_generate_systemd_unit(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_service_definition @ hermes_cli/gateway.py:_normalize_service_definition */
int cgw_u_normalize_service_definition(const char *arg) { (void)arg; return 0; }

/* PoP: _strip_optional_systemd_directives @ hermes_cli/gateway.py:_strip_optional_systemd_directives */
int cgw_u_strip_optional_systemd_directives(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_launchd_plist_for_comparison @ hermes_cli/gateway.py:_normalize_launchd_plist_for_comparison */
int cgw_u_normalize_launchd_plist_for_comparison(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_unit_is_current @ hermes_cli/gateway.py:systemd_unit_is_current */
int cgw_systemd_unit_is_current(const char *arg) { (void)arg; return 0; }

/* PoP: _temp_home_in_service_definition @ hermes_cli/gateway.py:_temp_home_in_service_definition */
int cgw_u_temp_home_in_service_definition(const char *arg) { (void)arg; return 0; }

/* PoP: _refuse_temp_home_service_write @ hermes_cli/gateway.py:_refuse_temp_home_service_write */
int cgw_u_refuse_temp_home_service_write(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_systemd_unit_if_needed @ hermes_cli/gateway.py:refresh_systemd_unit_if_needed */
int cgw_refresh_systemd_unit_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: _print_linger_enable_warning @ hermes_cli/gateway.py:_print_linger_enable_warning */
int cgw_u_print_linger_enable_warning(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_linger_enabled @ hermes_cli/gateway.py:_ensure_linger_enabled */
int cgw_u_ensure_linger_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _select_systemd_scope @ hermes_cli/gateway.py:_select_systemd_scope */
int cgw_u_select_systemd_scope(const char *arg) { (void)arg; return 0; }

/* PoP: _system_scope_wizard_would_need_root @ hermes_cli/gateway.py:_system_scope_wizard_would_need_root */
int cgw_u_system_scope_wizard_would_need_root(const char *arg) { (void)arg; return 0; }

/* PoP: _print_system_scope_remediation @ hermes_cli/gateway.py:_print_system_scope_remediation */
int cgw_u_print_system_scope_remediation(const char *arg) { (void)arg; return 0; }

/* PoP: _get_restart_drain_timeout @ hermes_cli/gateway.py:_get_restart_drain_timeout */
int cgw_u_get_restart_drain_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_install @ hermes_cli/gateway.py:systemd_install */
int cgw_systemd_install(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_uninstall @ hermes_cli/gateway.py:systemd_uninstall */
int cgw_systemd_uninstall(const char *arg) { (void)arg; return 0; }

/* PoP: _require_service_installed @ hermes_cli/gateway.py:_require_service_installed */
int cgw_u_require_service_installed(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_start @ hermes_cli/gateway.py:systemd_start */
int cgw_systemd_start(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_stop @ hermes_cli/gateway.py:systemd_stop */
int cgw_systemd_stop(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_restart @ hermes_cli/gateway.py:systemd_restart */
int cgw_systemd_restart(const char *arg) { (void)arg; return 0; }

/* PoP: systemd_status @ hermes_cli/gateway.py:systemd_status */
int cgw_systemd_status(const char *arg) { (void)arg; return 0; }

/* PoP: get_launchd_label @ hermes_cli/gateway.py:get_launchd_label */
int cgw_get_launchd_label(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_domain @ hermes_cli/gateway.py:_launchd_domain */
int cgw_u_launchd_domain(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_error_indicates_unloaded @ hermes_cli/gateway.py:_launchd_error_indicates_unloaded */
int cgw_u_launchd_error_indicates_unloaded(const char *arg) { (void)arg; return 0; }

/* PoP: _launchctl_domain_unsupported @ hermes_cli/gateway.py:_launchctl_domain_unsupported */
int cgw_u_launchctl_domain_unsupported(const char *arg) { (void)arg; return 0; }

/* PoP: _launchctl_bootstrap @ hermes_cli/gateway.py:_launchctl_bootstrap */
int cgw_u_launchctl_bootstrap(const char *arg) {
    /* Python (domain, plist_path, label, timeout): launchctl bootstrap;
     * exit 5 (EIO) = stale registration -> bootout the label, retry once. */
    if (!arg || !*arg) return -1;
    char domain[256], plist[512], label[256], to[32];
    if (sscanf(arg, "%255[^\t]\t%511[^\t]\t%255[^\t]\t%31s", domain, plist, label, to) < 3)
        return -1;
    char cmd[1400];
    snprintf(cmd, sizeof(cmd), "launchctl bootstrap %s %s >/dev/null 2>&1", domain, plist);
    int rc = system(cmd);
    if (rc != 5) return rc; /* 0 ok; non-5 failure propagates */
    snprintf(cmd, sizeof(cmd), "launchctl bootout %s/%s >/dev/null 2>&1", domain, label);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "launchctl bootstrap %s %s >/dev/null 2>&1", domain, plist);
    return system(cmd);
}

/* PoP: _launchd_reload_log_path @ hermes_cli/gateway.py:_launchd_reload_log_path */
int cgw_u_launchd_reload_log_path(const char *arg) { (void)arg; return 0; }

/* PoP: _append_launchd_reload_log @ hermes_cli/gateway.py:_append_launchd_reload_log */
int cgw_u_append_launchd_reload_log(const char *arg) { (void)arg; return 0; }

/* PoP: _launchctl_label_registered @ hermes_cli/gateway.py:_launchctl_label_registered */
int cgw_u_launchctl_label_registered(const char *arg) {
    /* Python (label): launchctl list <label> exit code 0 == registered. */
    if (!arg || !*arg) return 0;
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "launchctl list %s >/dev/null 2>&1", arg);
    return system(cmd) == 0;
}

/* PoP: _retry_launchctl_bootstrap_until_registered @ hermes_cli/gateway.py:_retry_launchctl_bootstrap_until_registered */
int cgw_u_retry_launchctl_bootstrap_until_registered(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_unsupported_marker_path @ hermes_cli/gateway.py:_launchd_unsupported_marker_path */
int cgw_u_launchd_unsupported_marker_path(const char *arg) { (void)arg; return 0; }

/* PoP: _write_launchd_unsupported_marker @ hermes_cli/gateway.py:_write_launchd_unsupported_marker */
int cgw_u_write_launchd_unsupported_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_launchd_unsupported_marker @ hermes_cli/gateway.py:_clear_launchd_unsupported_marker */
int cgw_u_clear_launchd_unsupported_marker(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_unsupported_marker_exists @ hermes_cli/gateway.py:_launchd_unsupported_marker_exists */
int cgw_u_launchd_unsupported_marker_exists(const char *arg) { (void)arg; return 0; }

/* PoP: _gateway_run_command @ hermes_cli/gateway.py:_gateway_run_command */
int cgw_u_gateway_run_command(const char *arg) { (void)arg; return 0; }

/* PoP: _spawn_detached_gateway @ hermes_cli/gateway.py:_spawn_detached_gateway */
int cgw_u_spawn_detached_gateway(const char *arg) { (void)arg; return 0; }

/* PoP: _launchd_fallback_to_detached @ hermes_cli/gateway.py:_launchd_fallback_to_detached */
int cgw_u_launchd_fallback_to_detached(const char *arg) { (void)arg; return 0; }

/* PoP: generate_launchd_plist @ hermes_cli/gateway.py:generate_launchd_plist */
int cgw_generate_launchd_plist(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_plist_is_current @ hermes_cli/gateway.py:launchd_plist_is_current */
int cgw_launchd_plist_is_current(const char *arg) { (void)arg; return 0; }

/* PoP: refresh_launchd_plist_if_needed @ hermes_cli/gateway.py:refresh_launchd_plist_if_needed */
int cgw_refresh_launchd_plist_if_needed(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_install @ hermes_cli/gateway.py:launchd_install */
int cgw_launchd_install(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_uninstall @ hermes_cli/gateway.py:launchd_uninstall */
int cgw_launchd_uninstall(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_start @ hermes_cli/gateway.py:launchd_start */
int cgw_launchd_start(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_stop @ hermes_cli/gateway.py:launchd_stop */
int cgw_launchd_stop(const char *arg) { (void)arg; return 0; }

/* PoP: _wait_for_gateway_exit @ hermes_cli/gateway.py:_wait_for_gateway_exit */
int cgw_u_wait_for_gateway_exit(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_restart @ hermes_cli/gateway.py:launchd_restart */
int cgw_launchd_restart(const char *arg) { (void)arg; return 0; }

/* PoP: launchd_status @ hermes_cli/gateway.py:launchd_status */
int cgw_launchd_status(const char *arg) { (void)arg; return 0; }

/* PoP: _truthy_env @ hermes_cli/gateway.py:_truthy_env */
int cgw_u_truthy_env(const char *arg) { (void)arg; return 0; }

/* PoP: _is_official_docker_checkout @ hermes_cli/gateway.py:_is_official_docker_checkout */
int cgw_u_is_official_docker_checkout(const char *arg) { (void)arg; return 0; }

/* PoP: _running_under_gateway_supervisor @ hermes_cli/gateway.py:_running_under_gateway_supervisor */
int cgw_u_running_under_gateway_supervisor(const char *arg) { (void)arg; return 0; }

/* PoP: _guard_supervised_gateway_conflict @ hermes_cli/gateway.py:_guard_supervised_gateway_conflict */
int cgw_u_guard_supervised_gateway_conflict(const char *arg) { (void)arg; return 0; }

/* PoP: _guard_existing_gateway_process_conflict @ hermes_cli/gateway.py:_guard_existing_gateway_process_conflict */
int cgw_u_guard_existing_gateway_process_conflict(const char *arg) { (void)arg; return 0; }

/* PoP: _guard_official_docker_root_gateway @ hermes_cli/gateway.py:_guard_official_docker_root_gateway */
int cgw_u_guard_official_docker_root_gateway(const char *arg) { (void)arg; return 0; }

/* PoP: _all_platforms @ hermes_cli/gateway.py:_all_platforms */
int cgw_u_all_platforms(const char *arg) { (void)arg; return 0; }

/* PoP: _platform_status @ hermes_cli/gateway.py:_platform_status */
int cgw_u_platform_status(const char *arg) { (void)arg; return 0; }

/* PoP: _runtime_health_lines @ hermes_cli/gateway.py:_runtime_health_lines */
int cgw_u_runtime_health_lines(const char *arg) { (void)arg; return 0; }

/* PoP: _set_platform_unauthorized_dm_behavior @ hermes_cli/gateway.py:_set_platform_unauthorized_dm_behavior */
int cgw_u_set_platform_unauthorized_dm_behavior(const char *arg) { (void)arg; return 0; }

/* PoP: _setup_standard_platform @ hermes_cli/gateway.py:_setup_standard_platform */
int cgw_u_setup_standard_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _is_service_installed @ hermes_cli/gateway.py:_is_service_installed */
int cgw_u_is_service_installed(const char *arg) { (void)arg; return 0; }

/* PoP: _is_service_running @ hermes_cli/gateway.py:_is_service_running */
int cgw_u_is_service_running(const char *arg) { (void)arg; return 0; }

/* PoP: _builtin_setup_fn @ hermes_cli/gateway.py:_builtin_setup_fn */
int cgw_u_builtin_setup_fn(const char *arg) { (void)arg; return 0; }

/* PoP: _configure_platform @ hermes_cli/gateway.py:_configure_platform */
int cgw_u_configure_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_via_service_manager_if_s6 @ hermes_cli/gateway.py:_dispatch_via_service_manager_if_s6 */
int cgw_u_dispatch_via_service_manager_if_s6(const char *arg) { (void)arg; return 0; }

/* PoP: _dispatch_all_via_service_manager_if_s6 @ hermes_cli/gateway.py:_dispatch_all_via_service_manager_if_s6 */
int cgw_u_dispatch_all_via_service_manager_if_s6(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_redirect_run_to_s6_supervision @ hermes_cli/gateway.py:_maybe_redirect_run_to_s6_supervision */
int cgw_u_maybe_redirect_run_to_s6_supervision(const char *arg) { (void)arg; return 0; }

/* PoP: _block_until_terminated @ hermes_cli/gateway.py:_block_until_terminated */
int cgw_u_block_until_terminated(const char *arg) { (void)arg; return 0; }

/* PoP: _gateway_command_inner @ hermes_cli/gateway.py:_gateway_command_inner */
int cgw_u_gateway_command_inner(const char *arg) { (void)arg; return 0; }
