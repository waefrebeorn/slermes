"""AUTO-GENERATED integration oracle for port_env_base_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.environments.base'] = _load('tools/environments/base.py')

DISPATCH = {
    'envb_buffered_chars': ('tools.environments.base', 'buffered_chars'),
    'envb_total_chars': ('tools.environments.base', 'total_chars'),
    'envb_append': ('tools.environments.base', 'append'),
    'envb_set_activity_callback': ('tools.environments.base', 'set_activity_callback'),
    'envb_u_get_activity_callback': ('tools.environments.base', '_get_activity_callback'),
    'envb_touch_activity_if_due': ('tools.environments.base', 'touch_activity_if_due'),
    'envb_get_sandbox_dir': ('tools.environments.base', 'get_sandbox_dir'),
    'envb_u_pipe_stdin': ('tools.environments.base', '_pipe_stdin'),
    'envb_u_popen_bash': ('tools.environments.base', '_popen_bash'),
    'envb_u_load_json_store': ('tools.environments.base', '_load_json_store'),
    'envb_u_save_json_store': ('tools.environments.base', '_save_json_store'),
    'envb_u_file_mtime_key': ('tools.environments.base', '_file_mtime_key'),
    'envb_stdout': ('tools.environments.base', 'stdout'),
    'envb_returncode': ('tools.environments.base', 'returncode'),
    'envb_stdout_2': ('tools.environments.base', 'stdout'),
    'envb_returncode_2': ('tools.environments.base', 'returncode'),
    'envb_u_cwd_marker': ('tools.environments.base', '_cwd_marker'),
    'envb_get_temp_dir': ('tools.environments.base', 'get_temp_dir'),
    'envb_init_session': ('tools.environments.base', 'init_session'),
    'envb_u_quote_cwd_for_cd': ('tools.environments.base', '_quote_cwd_for_cd'),
    'envb_u_quote_shell_path': ('tools.environments.base', '_quote_shell_path'),
    'envb_u_wrap_command': ('tools.environments.base', '_wrap_command'),
    'envb_u_embed_stdin_heredoc': ('tools.environments.base', '_embed_stdin_heredoc'),
    'envb_u_wait_for_process': ('tools.environments.base', '_wait_for_process'),
    'envb_u_update_cwd': ('tools.environments.base', '_update_cwd'),
    'envb_u_extract_cwd_from_output': ('tools.environments.base', '_extract_cwd_from_output'),
    'envb_u__del__': ('tools.environments.base', '__del__'),
    'envb_u_prepare_command': ('tools.environments.base', '_prepare_command'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_env_base_wrappers.py <cases.json>\n"); return 2
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
