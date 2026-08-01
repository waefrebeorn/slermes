"""AUTO-GENERATED integration oracle for port_windows_ssh_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.windows_ssh_runtime'] = _load('hermes_cli/windows_ssh_runtime.py')

DISPATCH = {
    'wssr_u_win32': ('hermes_cli.windows_ssh_runtime', '_win32'),
    'wssr_u_ownership': ('hermes_cli.windows_ssh_runtime', '_ownership'),
    'wssr_u_nonce': ('hermes_cli.windows_ssh_runtime', '_nonce'),
    'wssr_u_root': ('hermes_cli.windows_ssh_runtime', '_root'),
    'wssr_u_directory': ('hermes_cli.windows_ssh_runtime', '_directory'),
    'wssr_u_log_path': ('hermes_cli.windows_ssh_runtime', '_log_path'),
    'wssr_u_current_sid': ('hermes_cli.windows_ssh_runtime', '_current_sid'),
    'wssr_u_system_sid': ('hermes_cli.windows_ssh_runtime', '_system_sid'),
    'wssr_u_security_attributes': ('hermes_cli.windows_ssh_runtime', '_security_attributes'),
    'wssr_u_allowed_sids': ('hermes_cli.windows_ssh_runtime', '_allowed_sids'),
    'wssr_u_verify_security': ('hermes_cli.windows_ssh_runtime', '_verify_security'),
    'wssr_u_open': ('hermes_cli.windows_ssh_runtime', '_open'),
    'wssr_u_ensure_directory': ('hermes_cli.windows_ssh_runtime', '_ensure_directory'),
    'wssr_u_ensure_scope': ('hermes_cli.windows_ssh_runtime', '_ensure_scope'),
    'wssr_upload_token': ('hermes_cli.windows_ssh_runtime', 'upload_token'),
    'wssr_read_token': ('hermes_cli.windows_ssh_runtime', 'read_token'),
    'wssr_u_read_json_stdin': ('hermes_cli.windows_ssh_runtime', '_read_json_stdin'),
    'wssr_read_lock': ('hermes_cli.windows_ssh_runtime', 'read_lock'),
    'wssr_write_lock': ('hermes_cli.windows_ssh_runtime', 'write_lock'),
    'wssr_remove_artifact': ('hermes_cli.windows_ssh_runtime', 'remove_artifact'),
    'wssr_process_state': ('hermes_cli.windows_ssh_runtime', 'process_state'),
    'wssr_terminate_owned': ('hermes_cli.windows_ssh_runtime', 'terminate_owned'),
    'wssr_u_resolve_direct_interpreter': ('hermes_cli.windows_ssh_runtime', '_resolve_direct_interpreter'),
    'wssr_spawn_backend': ('hermes_cli.windows_ssh_runtime', 'spawn_backend'),
    'wssr_inspect_hermes': ('hermes_cli.windows_ssh_runtime', 'inspect_hermes'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_windows_ssh_wrappers.py <cases.json>\n"); return 2
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
