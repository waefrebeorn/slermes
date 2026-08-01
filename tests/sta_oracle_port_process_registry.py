"""AUTO-GENERATED integration oracle for port_process_registry (gen_integration_oracle.py)."""
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
MODS['tools.process_registry'] = _load('tools/process_registry.py')

DISPATCH = {
    'process_registry_has_active_for_session': ('tools.process_registry', 'has_active_for_session'),
    'process_registry_has_active_processes': ('tools.process_registry', 'has_active_processes'),
    'process_registry_kill_all': ('tools.process_registry', 'kill_all'),
    'process_registry_is_completion_consumed': ('tools.process_registry', 'is_completion_consumed'),
    'process_registry_is_session_waiting': ('tools.process_registry', 'is_session_waiting'),
    'process_registry_drain_should_skip': ('tools.process_registry', '_drain_should_skip'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_process_registry.py <cases.json>\n"); return 2
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
