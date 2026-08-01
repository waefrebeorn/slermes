"""AUTO-GENERATED integration oracle for port_managed_uv_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.managed_uv'] = _load('hermes_cli/managed_uv.py')

DISPATCH = {
    'muv_managed_uv_path': ('hermes_cli.managed_uv', 'managed_uv_path'),
    'muv_managed_python_install_dir': ('hermes_cli.managed_uv', 'managed_python_install_dir'),
    'muv_managed_python_env': ('hermes_cli.managed_uv', 'managed_python_env'),
    'muv_repaired': ('hermes_cli.managed_uv', 'repaired'),
    'muv_u_report_runtime_repair_failure': ('hermes_cli.managed_uv', '_report_runtime_repair_failure'),
    'muv_u__new__': ('hermes_cli.managed_uv', '__new__'),
    'muv_u__iter__': ('hermes_cli.managed_uv', '__iter__'),
    'muv_u_ensure_uv_path': ('hermes_cli.managed_uv', '_ensure_uv_path'),
    'muv_u_venv_python': ('hermes_cli.managed_uv', '_venv_python'),
    'muv_u_remove_tree': ('hermes_cli.managed_uv', '_remove_tree'),
    'muv_u_make_world_traversable': ('hermes_cli.managed_uv', '_make_world_traversable'),
    'muv_u_runtime_request': ('hermes_cli.managed_uv', '_runtime_request'),
    'muv_u_install_safe_python_generation': ('hermes_cli.managed_uv', '_install_safe_python_generation'),
    'muv_u_smoke_candidate_venv': ('hermes_cli.managed_uv', '_smoke_candidate_venv'),
    'muv_u_stage_candidate_venv': ('hermes_cli.managed_uv', '_stage_candidate_venv'),
    'muv_u_rename_with_retry': ('hermes_cli.managed_uv', '_rename_with_retry'),
    'muv_u_cut_over_candidate': ('hermes_cli.managed_uv', '_cut_over_candidate'),
    'muv_u_acquire_repair_lock': ('hermes_cli.managed_uv', '_acquire_repair_lock'),
    'muv_u_release_repair_lock': ('hermes_cli.managed_uv', '_release_repair_lock'),
    'muv_u_windows_runtime_holders': ('hermes_cli.managed_uv', '_windows_runtime_holders'),
    'muv_repair_vulnerable_runtime': ('hermes_cli.managed_uv', 'repair_vulnerable_runtime'),
    'muv_u_install_uv': ('hermes_cli.managed_uv', '_install_uv'),
    'muv_u_install_uv_posix': ('hermes_cli.managed_uv', '_install_uv_posix'),
    'muv_u_install_uv_windows': ('hermes_cli.managed_uv', '_install_uv_windows'),
    'muv_rebuild_venv': ('hermes_cli.managed_uv', 'rebuild_venv'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_managed_uv_wrappers.py <cases.json>\n"); return 2
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
