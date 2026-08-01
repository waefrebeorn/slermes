"""AUTO-GENERATED integration oracle for port_gateway_windows_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.gateway_windows'] = _load('hermes_cli/gateway_windows.py')

DISPATCH = {
    'gw_u_schtasks_encoding': ('hermes_cli.gateway_windows', '_schtasks_encoding'),
    'gw_u_assert_windows': ('hermes_cli.gateway_windows', '_assert_windows'),
    'gw_u_preserve_hermes_home_path': ('hermes_cli.gateway_windows', '_preserve_hermes_home_path'),
    'gw_u_quote_cmd_script_arg': ('hermes_cli.gateway_windows', '_quote_cmd_script_arg'),
    'gw_u_quote_schtasks_arg': ('hermes_cli.gateway_windows', '_quote_schtasks_arg'),
    'gw_u_exec_schtasks': ('hermes_cli.gateway_windows', '_exec_schtasks'),
    'gw_u_should_fall_back': ('hermes_cli.gateway_windows', '_should_fall_back'),
    'gw_u_is_access_denied': ('hermes_cli.gateway_windows', '_is_access_denied'),
    'gw_u_is_running_as_admin': ('hermes_cli.gateway_windows', '_is_running_as_admin'),
    'gw_u_current_profile_cli_args': ('hermes_cli.gateway_windows', '_current_profile_cli_args'),
    'gw_u_launch_elevated_gateway_command': ('hermes_cli.gateway_windows', '_launch_elevated_gateway_command'),
    'gw_u_launch_elevated_install': ('hermes_cli.gateway_windows', '_launch_elevated_install'),
    'gw_u_launch_elevated_uninstall': ('hermes_cli.gateway_windows', '_launch_elevated_uninstall'),
    'gw_get_task_name': ('hermes_cli.gateway_windows', 'get_task_name'),
    'gw_u_sanitize_filename': ('hermes_cli.gateway_windows', '_sanitize_filename'),
    'gw_get_task_script_path': ('hermes_cli.gateway_windows', 'get_task_script_path'),
    'gw_u_startup_dir': ('hermes_cli.gateway_windows', '_startup_dir'),
    'gw_get_startup_entry_path': ('hermes_cli.gateway_windows', 'get_startup_entry_path'),
    'gw_u_legacy_startup_entry_path': ('hermes_cli.gateway_windows', '_legacy_startup_entry_path'),
    'gw_u_stable_gateway_working_dir': ('hermes_cli.gateway_windows', '_stable_gateway_working_dir'),
    'gw_u_build_gateway_cmd_script': ('hermes_cli.gateway_windows', '_build_gateway_cmd_script'),
    'gw_u_quote_vbs_string': ('hermes_cli.gateway_windows', '_quote_vbs_string'),
    'gw_u_build_gateway_vbs_script': ('hermes_cli.gateway_windows', '_build_gateway_vbs_script'),
    'gw_u_build_startup_launcher': ('hermes_cli.gateway_windows', '_build_startup_launcher'),
    'gw_u_write_task_script': ('hermes_cli.gateway_windows', '_write_task_script'),
    'gw_u_resolve_task_user': ('hermes_cli.gateway_windows', '_resolve_task_user'),
    'gw_u_build_scheduled_task_xml': ('hermes_cli.gateway_windows', '_build_scheduled_task_xml'),
    'gw_u_write_scheduled_task_xml': ('hermes_cli.gateway_windows', '_write_scheduled_task_xml'),
    'gw_u_install_scheduled_task': ('hermes_cli.gateway_windows', '_install_scheduled_task'),
    'gw_u_install_startup_entry': ('hermes_cli.gateway_windows', '_install_startup_entry'),
    'gw_u_resolve_detached_python': ('hermes_cli.gateway_windows', '_resolve_detached_python'),
    'gw_u_prepend_pythonpath': ('hermes_cli.gateway_windows', '_prepend_pythonpath'),
    'gw_u_build_gateway_argv': ('hermes_cli.gateway_windows', '_build_gateway_argv'),
    'gw_windowless_gateway_restart_spec': ('hermes_cli.gateway_windows', 'windowless_gateway_restart_spec'),
    'gw_u_spawn_detached': ('hermes_cli.gateway_windows', '_spawn_detached'),
    'gw_u_install_choice_from_env': ('hermes_cli.gateway_windows', '_install_choice_from_env'),
    'gw_u_prompt_install_choices': ('hermes_cli.gateway_windows', '_prompt_install_choices'),
    'gw_u_install_startup_fallback': ('hermes_cli.gateway_windows', '_install_startup_fallback'),
    'gw_u_wait_for_gateway_ready': ('hermes_cli.gateway_windows', '_wait_for_gateway_ready'),
    'gw_u_report_gateway_start': ('hermes_cli.gateway_windows', '_report_gateway_start'),
    'gw_u_print_next_steps': ('hermes_cli.gateway_windows', '_print_next_steps'),
    'gw_is_task_registered': ('hermes_cli.gateway_windows', 'is_task_registered'),
    'gw_is_startup_entry_installed': ('hermes_cli.gateway_windows', 'is_startup_entry_installed'),
    'gw_query_task_status': ('hermes_cli.gateway_windows', 'query_task_status'),
    'gw_u_gateway_pids': ('hermes_cli.gateway_windows', '_gateway_pids'),
    'gw_u_print_deep_probes': ('hermes_cli.gateway_windows', '_print_deep_probes'),
    'gw_u_drain_gateway_pid': ('hermes_cli.gateway_windows', '_drain_gateway_pid'),
    'gw_u_windows_stop_drain_timeout': ('hermes_cli.gateway_windows', '_windows_stop_drain_timeout'),
    'gw_u_force_terminate_known_gateway_pids': ('hermes_cli.gateway_windows', '_force_terminate_known_gateway_pids'),
    'gw_u_collect_gateway_stop_pids': ('hermes_cli.gateway_windows', '_collect_gateway_stop_pids'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_gateway_windows_wrappers.py <cases.json>\n"); return 2
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
