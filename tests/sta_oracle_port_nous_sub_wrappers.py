"""AUTO-GENERATED integration oracle for port_nous_sub_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.nous_subscription'] = _load('hermes_cli/nous_subscription.py')

DISPATCH = {
    'nsub_u_uses_gateway': ('hermes_cli.nous_subscription', '_uses_gateway'),
    'nsub_image_gen': ('hermes_cli.nous_subscription', 'image_gen'),
    'nsub_video_gen': ('hermes_cli.nous_subscription', 'video_gen'),
    'nsub_u_toolset_enabled': ('hermes_cli.nous_subscription', '_toolset_enabled'),
    'nsub_u_has_agent_browser': ('hermes_cli.nous_subscription', '_has_agent_browser'),
    'nsub_u_local_browser_runnable': ('hermes_cli.nous_subscription', '_local_browser_runnable'),
    'nsub_u_browser_label': ('hermes_cli.nous_subscription', '_browser_label'),
    'nsub_u_tts_label': ('hermes_cli.nous_subscription', '_tts_label'),
    'nsub_u_stt_label': ('hermes_cli.nous_subscription', '_stt_label'),
    'nsub_u_local_stt_backend_available': ('hermes_cli.nous_subscription', '_local_stt_backend_available'),
    'nsub_u_resolve_browser_feature_state': ('hermes_cli.nous_subscription', '_resolve_browser_feature_state'),
    'nsub_apply_nous_managed_defaults': ('hermes_cli.nous_subscription', 'apply_nous_managed_defaults'),
    'nsub_u_get_gateway_direct_credentials': ('hermes_cli.nous_subscription', '_get_gateway_direct_credentials'),
    'nsub_get_gateway_eligible_tools': ('hermes_cli.nous_subscription', 'get_gateway_eligible_tools'),
    'nsub_apply_gateway_defaults': ('hermes_cli.nous_subscription', 'apply_gateway_defaults'),
    'nsub_prompt_enable_tool_gateway': ('hermes_cli.nous_subscription', 'prompt_enable_tool_gateway'),
    'nsub_ensure_nous_portal_access': ('hermes_cli.nous_subscription', 'ensure_nous_portal_access'),
    'nsub_u_run_nous_portal_login_only': ('hermes_cli.nous_subscription', '_run_nous_portal_login_only'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_nous_sub_wrappers.py <cases.json>\n"); return 2
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
