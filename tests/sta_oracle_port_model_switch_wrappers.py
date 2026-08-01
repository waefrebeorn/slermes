"""AUTO-GENERATED integration oracle for port_model_switch_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.model_switch'] = _load('hermes_cli/model_switch.py')

DISPATCH = {
    'msw_u_declared_model_ids': ('hermes_cli.model_switch', '_declared_model_ids'),
    'msw_u_save_discovered_models_to_config': ('hermes_cli.model_switch', '_save_discovered_models_to_config'),
    'msw_u_bare_custom_provider_def': ('hermes_cli.model_switch', '_bare_custom_provider_def'),
    'msw_format_model_for_display': ('hermes_cli.model_switch', 'format_model_for_display'),
    'msw_is_nous_hermes_non_agentic': ('hermes_cli.model_switch', 'is_nous_hermes_non_agentic'),
    'msw_u_check_hermes_model_warning': ('hermes_cli.model_switch', '_check_hermes_model_warning'),
    'msw_u_load_direct_aliases': ('hermes_cli.model_switch', '_load_direct_aliases'),
    'msw_u_ensure_direct_aliases': ('hermes_cli.model_switch', '_ensure_direct_aliases'),
    'msw_parse_model_flags_detailed': ('hermes_cli.model_switch', 'parse_model_flags_detailed'),
    'msw_u_model_sort_key': ('hermes_cli.model_switch', '_model_sort_key'),
    'msw_get_authenticated_provider_slugs': ('hermes_cli.model_switch', 'get_authenticated_provider_slugs'),
    'msw_u_resolve_alias_fallback': ('hermes_cli.model_switch', '_resolve_alias_fallback'),
    'msw_resolve_display_context_length': ('hermes_cli.model_switch', 'resolve_display_context_length'),
    'msw_u_configured_provider_matches': ('hermes_cli.model_switch', '_configured_provider_matches'),
    'msw_u_resolve_named_custom_model_id': ('hermes_cli.model_switch', '_resolve_named_custom_model_id'),
    'msw_u_credential_pool_is_usable': ('hermes_cli.model_switch', '_credential_pool_is_usable'),
    'msw_u_extra_headers_from_config': ('hermes_cli.model_switch', '_extra_headers_from_config'),
    'msw_prewarm_picker_cache_async': ('hermes_cli.model_switch', 'prewarm_picker_cache_async'),
    'msw_u_prepend_moa_picker_provider': ('hermes_cli.model_switch', '_prepend_moa_picker_provider'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_model_switch_wrappers.py <cases.json>\n"); return 2
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
