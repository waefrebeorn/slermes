"""AUTO-GENERATED integration oracle for port_session_export_md_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.session_export_md'] = _load('hermes_cli/session_export_md.py')

DISPATCH = {
    'sexmd_u_iso_timestamp': ('hermes_cli.session_export_md', '_iso_timestamp'),
    'sexmd_u_frontmatter_value': ('hermes_cli.session_export_md', '_frontmatter_value'),
    'sexmd_u_frontmatter_line': ('hermes_cli.session_export_md', '_frontmatter_line'),
    'sexmd_u_message_heading': ('hermes_cli.session_export_md', '_message_heading'),
    'sexmd_u_render_content': ('hermes_cli.session_export_md', '_render_content'),
    'sexmd_u_render_tool_calls': ('hermes_cli.session_export_md', '_render_tool_calls'),
    'sexmd_u_session_id': ('hermes_cli.session_export_md', '_session_id'),
    'sexmd_u_segments': ('hermes_cli.session_export_md', '_segments'),
    'sexmd_u_message_count': ('hermes_cli.session_export_md', '_message_count'),
    'sexmd_u_render_messages': ('hermes_cli.session_export_md', '_render_messages'),
    'sexmd_u_export_body_without_hash': ('hermes_cli.session_export_md', '_export_body_without_hash'),
    'sexmd_u_body_for_digest': ('hermes_cli.session_export_md', '_body_for_digest'),
    'sexmd_render_session_markdown': ('hermes_cli.session_export_md', 'render_session_markdown'),
    'sexmd_safe_session_filename': ('hermes_cli.session_export_md', 'safe_session_filename'),
    'sexmd_file_sha256': ('hermes_cli.session_export_md', 'file_sha256'),
    'sexmd_verify_export_file': ('hermes_cli.session_export_md', 'verify_export_file'),
    'sexmd_redact_session_data': ('hermes_cli.session_export_md', 'redact_session_data'),
    'sexmd_write_session_markdown': ('hermes_cli.session_export_md', 'write_session_markdown'),
    'sexmd_append_manifest_entry': ('hermes_cli.session_export_md', 'append_manifest_entry'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_session_export_md_wrappers.py <cases.json>\n"); return 2
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
