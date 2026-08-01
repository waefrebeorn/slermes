"""AUTO-GENERATED integration oracle for port_status_wrappers (gen_integration_oracle.py)."""
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
MODS['gateway.status'] = _load('gateway/status.py')

DISPATCH = {
    'gstat_u_get_starts_log_path': ('gateway.status', '_get_starts_log_path'),
    'gstat_record_start_and_check_storm': ('gateway.status', 'record_start_and_check_storm'),
    'gstat_u_get_process_hermes_home': ('gateway.status', '_get_process_hermes_home'),
    'gstat_u_canonical_hermes_home': ('gateway.status', '_canonical_hermes_home'),
    'gstat_u_same_hermes_home': ('gateway.status', '_same_hermes_home'),
    'gstat_normalize_updated_at': ('gateway.status', 'normalize_updated_at'),
    'gstat_u_clear_running_pid_cache': ('gateway.status', '_clear_running_pid_cache'),
    'gstat_u_file_cache_signature': ('gateway.status', '_file_cache_signature'),
    'gstat_u_running_pid_cache_signature': ('gateway.status', '_running_pid_cache_signature'),
    'gstat_runtime_status_is_stale': ('gateway.status', 'runtime_status_is_stale'),
    'gstat_runtime_status_pid_is_live': ('gateway.status', 'runtime_status_pid_is_live'),
    'gstat_u_validated_scoped_lock_gateway_owner': ('gateway.status', '_validated_scoped_lock_gateway_owner'),
    'gstat_u_scoped_lock_owner_state': ('gateway.status', '_scoped_lock_owner_state'),
    'gstat_u_wait_for_scoped_lock_owner_exit': ('gateway.status', '_wait_for_scoped_lock_owner_exit'),
    'gstat_u_snapshot_gateway_children': ('gateway.status', '_snapshot_gateway_children'),
    'gstat_reap_gateway_children': ('gateway.status', 'reap_gateway_children'),
    'gstat_take_over_scoped_lock_holder': ('gateway.status', 'take_over_scoped_lock_holder'),
    'gstat_u_terminate_scoped_lock_owner_once': ('gateway.status', '_terminate_scoped_lock_owner_once'),
    'gstat_get_running_pid_cached': ('gateway.status', 'get_running_pid_cached'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_status_wrappers.py <cases.json>\n"); return 2
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
