"""AUTO-GENERATED integration oracle for port_session_export_wrappers (gen_integration_oracle.py)."""
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
MODS['hermes_cli.session_export'] = _load('hermes_cli/session_export.py')

DISPATCH = {
    'sexp_normalize_export_format': ('hermes_cli.session_export', 'normalize_export_format'),
    'sexp_normalize_export_only': ('hermes_cli.session_export', 'normalize_export_only'),
    'sexp_render_sessions_export': ('hermes_cli.session_export', 'render_sessions_export'),
    'sexp_export_record_count': ('hermes_cli.session_export', 'export_record_count'),
    'sexp_iter_user_prompt_records': ('hermes_cli.session_export', 'iter_user_prompt_records'),
    'sexp_u_render_jsonl': ('hermes_cli.session_export', '_render_jsonl'),
    'sexp_u_render_markdown': ('hermes_cli.session_export', '_render_markdown'),
    'sexp_u_render_user_prompts_markdown': ('hermes_cli.session_export', '_render_user_prompts_markdown'),
    'sexp_u_append_prompt_records': ('hermes_cli.session_export', '_append_prompt_records'),
    'sexp_u_render_full_markdown': ('hermes_cli.session_export', '_render_full_markdown'),
    'sexp_u_append_session_messages': ('hermes_cli.session_export', '_append_session_messages'),
    'sexp_u_messages': ('hermes_cli.session_export', '_messages'),
    'sexp_u_message_text': ('hermes_cli.session_export', '_message_text'),
    'sexp_u_content_part_text': ('hermes_cli.session_export', '_content_part_text'),
    'sexp_u_session_metadata_lines': ('hermes_cli.session_export', '_session_metadata_lines'),
    'sexp_u_session_id': ('hermes_cli.session_export', '_session_id'),
    'sexp_u_session_title_or_id': ('hermes_cli.session_export', '_session_title_or_id'),
    'sexp_u_heading_text': ('hermes_cli.session_export', '_heading_text'),
    'sexp_u_inline_text': ('hermes_cli.session_export', '_inline_text'),
    'sexp_u_fenced_text': ('hermes_cli.session_export', '_fenced_text'),
    'sexp_u_finish_markdown': ('hermes_cli.session_export', '_finish_markdown'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_session_export_wrappers.py <cases.json>\n"); return 2
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
