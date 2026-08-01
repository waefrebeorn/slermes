"""AUTO-GENERATED integration oracle for port_model_setup_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.model_setup_flows'] = _load('hermes_cli/model_setup_flows.py')

DISPATCH = {
    'msf_bedrock_region_geo_prefix': ('hermes_cli.model_setup_flows', 'bedrock_region_geo_prefix'),
    'msf_bedrock_model_routable_from_region': ('hermes_cli.model_setup_flows', 'bedrock_model_routable_from_region'),
    'msf_u_prune_replaced_custom_model_config_credentials': ('hermes_cli.model_setup_flows', '_prune_replaced_custom_model_config_credentials'),
    'msf_u_prompt_auth_credentials_choice': ('hermes_cli.model_setup_flows', '_prompt_auth_credentials_choice'),
    'msf_u_model_flow_openrouter': ('hermes_cli.model_setup_flows', '_model_flow_openrouter'),
    'msf_u_print_moa_preset': ('hermes_cli.model_setup_flows', '_print_moa_preset'),
    'msf_u_model_flow_moa': ('hermes_cli.model_setup_flows', '_model_flow_moa'),
    'msf_u_model_flow_nous': ('hermes_cli.model_setup_flows', '_model_flow_nous'),
    'msf_u_model_flow_openai_codex': ('hermes_cli.model_setup_flows', '_model_flow_openai_codex'),
    'msf_u_model_flow_xai_oauth': ('hermes_cli.model_setup_flows', '_model_flow_xai_oauth'),
    'msf_u_model_flow_qwen_oauth': ('hermes_cli.model_setup_flows', '_model_flow_qwen_oauth'),
    'msf_u_model_flow_minimax_oauth': ('hermes_cli.model_setup_flows', '_model_flow_minimax_oauth'),
    'msf_u_model_flow_custom': ('hermes_cli.model_setup_flows', '_model_flow_custom'),
    'msf_u_model_flow_azure_foundry': ('hermes_cli.model_setup_flows', '_model_flow_azure_foundry'),
    'msf_u_model_flow_named_custom': ('hermes_cli.model_setup_flows', '_model_flow_named_custom'),
    'msf_u_model_flow_copilot': ('hermes_cli.model_setup_flows', '_model_flow_copilot'),
    'msf_u_model_flow_copilot_acp': ('hermes_cli.model_setup_flows', '_model_flow_copilot_acp'),
    'msf_u_model_flow_kimi': ('hermes_cli.model_setup_flows', '_model_flow_kimi'),
    'msf_u_model_flow_stepfun': ('hermes_cli.model_setup_flows', '_model_flow_stepfun'),
    'msf_u_model_flow_bedrock_api_key': ('hermes_cli.model_setup_flows', '_model_flow_bedrock_api_key'),
    'msf_u_model_flow_bedrock': ('hermes_cli.model_setup_flows', '_model_flow_bedrock'),
    'msf_u_model_flow_vertex': ('hermes_cli.model_setup_flows', '_model_flow_vertex'),
    'msf_u_select_zai_endpoint': ('hermes_cli.model_setup_flows', '_select_zai_endpoint'),
    'msf_u_model_flow_anthropic': ('hermes_cli.model_setup_flows', '_model_flow_anthropic'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_model_setup_wrappers.py <cases.json>\n"); return 2
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
