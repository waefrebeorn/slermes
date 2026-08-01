"""AUTO-GENERATED integration oracle for port_uninstall_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.uninstall'] = _load('hermes_cli/uninstall.py')

DISPATCH = {
    'uninst_log_info': ('hermes_cli.uninstall', 'log_info'),
    'uninst_log_success': ('hermes_cli.uninstall', 'log_success'),
    'uninst_log_warn': ('hermes_cli.uninstall', 'log_warn'),
    'uninst_find_shell_configs': ('hermes_cli.uninstall', 'find_shell_configs'),
    'uninst_remove_path_from_shell_configs': ('hermes_cli.uninstall', 'remove_path_from_shell_configs'),
    'uninst_remove_wrapper_script': ('hermes_cli.uninstall', 'remove_wrapper_script'),
    'uninst_u_node_symlink_candidate_dirs': ('hermes_cli.uninstall', '_node_symlink_candidate_dirs'),
    'uninst_remove_node_symlinks': ('hermes_cli.uninstall', 'remove_node_symlinks'),
    'uninst_uninstall_gateway_service': ('hermes_cli.uninstall', 'uninstall_gateway_service'),
    'uninst_u_hermes_path_markers': ('hermes_cli.uninstall', '_hermes_path_markers'),
    'uninst_remove_path_from_windows_registry': ('hermes_cli.uninstall', 'remove_path_from_windows_registry'),
    'uninst_remove_hermes_env_vars_windows': ('hermes_cli.uninstall', 'remove_hermes_env_vars_windows'),
    'uninst_remove_portable_tooling_windows': ('hermes_cli.uninstall', 'remove_portable_tooling_windows'),
    'uninst_u_is_default_hermes_home': ('hermes_cli.uninstall', '_is_default_hermes_home'),
    'uninst_u_discover_named_profiles': ('hermes_cli.uninstall', '_discover_named_profiles'),
    'uninst_u_uninstall_profile': ('hermes_cli.uninstall', '_uninstall_profile'),
    'uninst_run_gui_uninstall': ('hermes_cli.uninstall', 'run_gui_uninstall'),
    'uninst_run_uninstall': ('hermes_cli.uninstall', 'run_uninstall'),
    'uninst_u_print_uninstall_dry_run': ('hermes_cli.uninstall', '_print_uninstall_dry_run'),
    'uninst_u_perform_uninstall': ('hermes_cli.uninstall', '_perform_uninstall'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_uninstall_wrappers.py <cases.json>\n"); return 2
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
