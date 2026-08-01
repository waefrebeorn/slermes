"""AUTO-GENERATED integration oracle for port_env_local_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.environments.local'] = _load('tools/environments/local.py')

DISPATCH = {
    'envl_u_msys_to_windows_path': ('tools.environments.local', '_msys_to_windows_path'),
    'envl_u_resolve_local_initial_cwd': ('tools.environments.local', '_resolve_local_initial_cwd'),
    'envl_u_windows_to_msys_path': ('tools.environments.local', '_windows_to_msys_path'),
    'envl_u_bash_safe_path': ('tools.environments.local', '_bash_safe_path'),
    'envl_u_quote_bash_path': ('tools.environments.local', '_quote_bash_path'),
    'envl_u_cwd_usable': ('tools.environments.local', '_cwd_usable'),
    'envl_u_resolve_safe_cwd': ('tools.environments.local', '_resolve_safe_cwd'),
    'envl_u_build_provider_env_blocklist': ('tools.environments.local', '_build_provider_env_blocklist'),
    'envl_u_inject_context_hermes_home': ('tools.environments.local', '_inject_context_hermes_home'),
    'envl_u_inject_session_context_env': ('tools.environments.local', '_inject_session_context_env'),
    'envl_u_scrub_delegated_child_kanban_env': ('tools.environments.local', '_scrub_delegated_child_kanban_env'),
    'envl_hermes_subprocess_env': ('tools.environments.local', 'hermes_subprocess_env'),
    'envl_u_find_bash': ('tools.environments.local', '_find_bash'),
    'envl_u_looks_like_msys_spawn_failure': ('tools.environments.local', '_looks_like_msys_spawn_failure'),
    'envl_u_mandatory_aslr_enabled': ('tools.environments.local', '_mandatory_aslr_enabled'),
    'envl_u_git_root_from_bash': ('tools.environments.local', '_git_root_from_bash'),
    'envl_u_git_bash_aslr_help': ('tools.environments.local', '_git_bash_aslr_help'),
    'envl_u_bash_starts': ('tools.environments.local', '_bash_starts'),
    'envl_u_git_bash_bin_dirs': ('tools.environments.local', '_git_bash_bin_dirs'),
    'envl_u_prepend_git_bash_dirs': ('tools.environments.local', '_prepend_git_bash_dirs'),
    'envl_u_find_shell': ('tools.environments.local', '_find_shell'),
    'envl_u_resolve_hermes_bin_dir': ('tools.environments.local', '_resolve_hermes_bin_dir'),
    'envl_u_prepend_hermes_bin_dir': ('tools.environments.local', '_prepend_hermes_bin_dir'),
    'envl_u_append_missing_sane_path_entries': ('tools.environments.local', '_append_missing_sane_path_entries'),
    'envl_u_apply_windows_msys_bash_env_defaults': ('tools.environments.local', '_apply_windows_msys_bash_env_defaults'),
    'envl_u_path_env_key': ('tools.environments.local', '_path_env_key'),
    'envl_u_make_run_env': ('tools.environments.local', '_make_run_env'),
    'envl_u_read_terminal_shell_init_config': ('tools.environments.local', '_read_terminal_shell_init_config'),
    'envl_u_resolve_shell_init_files': ('tools.environments.local', '_resolve_shell_init_files'),
    'envl_u_prepend_shell_init': ('tools.environments.local', '_prepend_shell_init'),
    'envl_get_temp_dir': ('tools.environments.local', 'get_temp_dir'),
    'envl_u_quote_cwd_for_cd': ('tools.environments.local', '_quote_cwd_for_cd'),
    'envl_u_quote_shell_path': ('tools.environments.local', '_quote_shell_path'),
    'envl_u_update_cwd': ('tools.environments.local', '_update_cwd'),
    'envl_u_extract_cwd_from_output': ('tools.environments.local', '_extract_cwd_from_output'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_env_local_wrappers.py <cases.json>\n"); return 2
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
