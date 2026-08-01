"""AUTO-GENERATED integration oracle for port_terminal_tool_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.terminal_tool'] = _load('tools/terminal_tool.py')

DISPATCH = {
    'tt_u_safe_parse_import_env': ('tools.terminal_tool', '_safe_parse_import_env'),
    'tt_u_get_sudo_password_callback': ('tools.terminal_tool', '_get_sudo_password_callback'),
    'tt_u_get_approval_callback': ('tools.terminal_tool', '_get_approval_callback'),
    'tt_u_get_sudo_password_cache_scope': ('tools.terminal_tool', '_get_sudo_password_cache_scope'),
    'tt_u_get_cached_sudo_password': ('tools.terminal_tool', '_get_cached_sudo_password'),
    'tt_u_set_cached_sudo_password': ('tools.terminal_tool', '_set_cached_sudo_password'),
    'tt_u_reset_cached_sudo_passwords': ('tools.terminal_tool', '_reset_cached_sudo_passwords'),
    'tt_u_docker_volume_uses_host_path': ('tools.terminal_tool', '_docker_volume_uses_host_path'),
    'tt_u_docker_has_host_access': ('tools.terminal_tool', '_docker_has_host_access'),
    'tt_u_check_all_guards': ('tools.terminal_tool', '_check_all_guards'),
    'tt_u_sudo_wrong_password_failure': ('tools.terminal_tool', '_sudo_wrong_password_failure'),
    'tt_u_invalidate_cached_sudo_on_auth_failure': ('tools.terminal_tool', '_invalidate_cached_sudo_on_auth_failure'),
    'tt_u_count_real_sudo_invocations': ('tools.terminal_tool', '_count_real_sudo_invocations'),
    'tt_record_session_cwd': ('tools.terminal_tool', 'record_session_cwd'),
    'tt_get_session_cwd': ('tools.terminal_tool', 'get_session_cwd'),
    'tt_register_task_env_overrides': ('tools.terminal_tool', 'register_task_env_overrides'),
    'tt_clear_task_env_overrides': ('tools.terminal_tool', 'clear_task_env_overrides'),
    'tt_u_resolve_container_task_id': ('tools.terminal_tool', '_resolve_container_task_id'),
    'tt_resolve_task_overrides': ('tools.terminal_tool', 'resolve_task_overrides'),
    'tt_u_parse_env_var': ('tools.terminal_tool', '_parse_env_var'),
    'tt_u_safe_getcwd': ('tools.terminal_tool', '_safe_getcwd'),
    'tt_u_is_ssh_remote_tilde_cwd': ('tools.terminal_tool', '_is_ssh_remote_tilde_cwd'),
    'tt_u_is_unusable_container_cwd': ('tools.terminal_tool', '_is_unusable_container_cwd'),
    'tt_u_ensure_terminal_env_bridged': ('tools.terminal_tool', '_ensure_terminal_env_bridged'),
    'tt_u_get_modal_backend_state': ('tools.terminal_tool', '_get_modal_backend_state'),
    'tt_u_cleanup_thread_worker': ('tools.terminal_tool', '_cleanup_thread_worker'),
    'tt_u_start_cleanup_thread': ('tools.terminal_tool', '_start_cleanup_thread'),
    'tt_u_stop_cleanup_thread': ('tools.terminal_tool', '_stop_cleanup_thread'),
    'tt_u_atexit_cleanup': ('tools.terminal_tool', '_atexit_cleanup'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_terminal_tool_wrappers.py <cases.json>\n"); return 2
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
