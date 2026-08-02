"""AUTO-GENERATED integration oracle for port_agent_bedrock_adapter (gen_integration_oracle.py)."""
import os, sys, json, importlib.util

MODS = {}
def _load(rel):
    repo = os.path.realpath(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    devroot = os.path.dirname(repo)  # hermes_cli/ lives in the dev-tree parent of slermes
    for p in (repo, devroot):
        if p not in sys.path: sys.path.insert(0, p)
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
MODS['agent.bedrock_adapter'] = _load('agent/bedrock_adapter.py')

DISPATCH = {
    'cli_agent_bedrock_adapter_invalidate_runtime_client': ('agent.bedrock_adapter', 'invalidate_runtime_client'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_agent_bedrock_adapter.py <cases.json>\n"); return 2
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
