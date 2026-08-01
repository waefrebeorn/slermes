"""AUTO-GENERATED integration oracle for port_mcp_oauth_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.mcp_oauth'] = _load('tools/mcp_oauth.py')

DISPATCH = {
    'mcpo_u_get_token_dir': ('tools.mcp_oauth', '_get_token_dir'),
    'mcpo_u_safe_filename': ('tools.mcp_oauth', '_safe_filename'),
    'mcpo_u_find_free_port': ('tools.mcp_oauth', '_find_free_port'),
    'mcpo_u_reserve_callback_port': ('tools.mcp_oauth', '_reserve_callback_port'),
    'mcpo_u_cached_redirect_port': ('tools.mcp_oauth', '_cached_redirect_port'),
    'mcpo_u_cached_redirect_uri': ('tools.mcp_oauth', '_cached_redirect_uri'),
    'mcpo_u_is_interactive': ('tools.mcp_oauth', '_is_interactive'),
    'mcpo_u_raise_if_non_interactive': ('tools.mcp_oauth', '_raise_if_non_interactive'),
    'mcpo_force_interactive_oauth': ('tools.mcp_oauth', 'force_interactive_oauth'),
    'mcpo_suppress_interactive_oauth': ('tools.mcp_oauth', 'suppress_interactive_oauth'),
    'mcpo_u_can_open_browser': ('tools.mcp_oauth', '_can_open_browser'),
    'mcpo_u_read_json': ('tools.mcp_oauth', '_read_json'),
    'mcpo_u_write_json': ('tools.mcp_oauth', '_write_json'),
    'mcpo_u_tokens_path': ('tools.mcp_oauth', '_tokens_path'),
    'mcpo_u_client_info_path': ('tools.mcp_oauth', '_client_info_path'),
    'mcpo_u_meta_path': ('tools.mcp_oauth', '_meta_path'),
    'mcpo_get_tokens': ('tools.mcp_oauth', 'get_tokens'),
    'mcpo_set_tokens': ('tools.mcp_oauth', 'set_tokens'),
    'mcpo_get_client_info': ('tools.mcp_oauth', 'get_client_info'),
    'mcpo_set_client_info': ('tools.mcp_oauth', 'set_client_info'),
    'mcpo_save_oauth_metadata': ('tools.mcp_oauth', 'save_oauth_metadata'),
    'mcpo_load_oauth_metadata': ('tools.mcp_oauth', 'load_oauth_metadata'),
    'mcpo_poison_client_registration': ('tools.mcp_oauth', 'poison_client_registration'),
    'mcpo_has_cached_tokens': ('tools.mcp_oauth', 'has_cached_tokens'),
    'mcpo_u_make_callback_handler': ('tools.mcp_oauth', '_make_callback_handler'),
    'mcpo_u_make_redirect_handler': ('tools.mcp_oauth', '_make_redirect_handler'),
    'mcpo_u_wait_for_callback': ('tools.mcp_oauth', '_wait_for_callback'),
    'mcpo_u_make_callback_waiter': ('tools.mcp_oauth', '_make_callback_waiter'),
    'mcpo_u_paste_callback_reader': ('tools.mcp_oauth', '_paste_callback_reader'),
    'mcpo_remove_oauth_tokens': ('tools.mcp_oauth', 'remove_oauth_tokens'),
    'mcpo_u_configure_callback_port': ('tools.mcp_oauth', '_configure_callback_port'),
    'mcpo_u_resolve_redirect_uri': ('tools.mcp_oauth', '_resolve_redirect_uri'),
    'mcpo_u_build_client_metadata': ('tools.mcp_oauth', '_build_client_metadata'),
    'mcpo_u_maybe_preregister_client': ('tools.mcp_oauth', '_maybe_preregister_client'),
    'mcpo_build_oauth_auth': ('tools.mcp_oauth', 'build_oauth_auth'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_mcp_oauth_wrappers.py <cases.json>\n"); return 2
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
