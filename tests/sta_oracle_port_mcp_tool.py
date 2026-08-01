"""AUTO-GENERATED integration oracle for port_mcp_tool (gen_integration_oracle.py)."""
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
MODS['tools.mcp_tool'] = _load('tools/mcp_tool.py')

DISPATCH = {
    'mcp_tool_check_message_handler_support': ('tools.mcp_tool', '_check_message_handler_support'),
    'mcp_tool_is_method_not_found_error': ('tools.mcp_tool', '_is_method_not_found_error'),
    'mcp_tool_validate_remote_mcp_url': ('tools.mcp_tool', '_validate_remote_mcp_url'),
    'mcp_tool_is_http': ('tools.mcp_tool', '_is_http'),
    'mcp_tool_advertises_tools': ('tools.mcp_tool', '_advertises_tools'),
    'mcp_tool_refresh_tools': ('tools.mcp_tool', '_refresh_tools'),
    'mcp_tool_keepalive_probe': ('tools.mcp_tool', '_keepalive_probe'),
    'mcp_tool_preflight_content_type': ('tools.mcp_tool', '_preflight_content_type'),
    'mcp_tool_is_session_expired_error': ('tools.mcp_tool', '_is_session_expired_error'),
    'mcp_tool_parse_boolish': ('tools.mcp_tool', '_parse_boolish'),
    'mcp_tool_validate_server_config': ('tools.mcp_tool', '_validate_server_config'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_mcp_tool.py <cases.json>\n"); return 2
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
