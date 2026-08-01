"""AUTO-GENERATED integration oracle for port_agent_auxiliary_client_helpers (gen_integration_oracle.py)."""
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
MODS['agent.auxiliary_client'] = _load('agent/auxiliary_client.py')

DISPATCH = {
    'aux__is_anthropic_compatible_host': ('agent.auxiliary_client', '_is_anthropic_compatible_host'),
    'aux__is_invalid_aux_response_error': ('agent.auxiliary_client', '_is_invalid_aux_response_error'),
    'aux__resolve_aux_verify': ('agent.auxiliary_client', '_resolve_aux_verify'),
    'aux__maybe_wrap_anthropic': ('agent.auxiliary_client', '_maybe_wrap_anthropic'),
    'aux__evict_cached_clients': ('agent.auxiliary_client', '_evict_cached_clients'),
    'aux__recoverable_pool_provider': ('agent.auxiliary_client', '_recoverable_pool_provider'),
    'aux__recover_provider_pool': ('agent.auxiliary_client', '_recover_provider_pool'),
    'aux__is_openrouter_client': ('agent.auxiliary_client', '_is_openrouter_client'),
    'aux__cached_client_accepts_slash_models': ('agent.auxiliary_client', '_cached_client_accepts_slash_models'),
    'aux__refresh_provider_credentials': ('agent.auxiliary_client', '_refresh_provider_credentials'),
    'aux__task_minimum_context_length': ('agent.auxiliary_client', '_task_minimum_context_length'),
    'aux__validate_llm_response': ('agent.auxiliary_client', '_validate_llm_response'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_agent_auxiliary_client_helpers.py <cases.json>\n"); return 2
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
