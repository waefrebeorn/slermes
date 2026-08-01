"""AUTO-GENERATED integration oracle for port_gateway_run (gen_integration_oracle.py)."""
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
MODS['gateway.run'] = _load('gateway/run.py')

DISPATCH = {
    'gw_surface_passes_raw_text': ('gateway.run', '_gateway_surface_passes_raw_text'),
    'gw_is_transient_network_error': ('gateway.run', '_is_transient_network_error'),
    'gw_looks_like_provider_error': ('gateway.run', '_looks_like_gateway_provider_error'),
    'gw_uses_observed_group_context': ('gateway.run', '_uses_telegram_observed_group_context'),
    'gw_message_timestamps_enabled': ('gateway.run', '_message_timestamps_enabled'),
    'gw_is_auto_continue_noise': ('gateway.run', '_is_auto_continue_noise'),
    'gw_is_control_interrupt_message': ('gateway.run', '_is_control_interrupt_message'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_gateway_run.py <cases.json>\n"); return 2
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
