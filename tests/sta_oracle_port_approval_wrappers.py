"""AUTO-GENERATED integration oracle for port_approval_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.approval'] = _load('tools/approval.py')

DISPATCH = {
    'appr_u_prepare_smart_approval_observer': ('tools.approval', '_prepare_smart_approval_observer'),
    'appr_u_observe_smart_approval_verdict': ('tools.approval', '_observe_smart_approval_verdict'),
    'appr_u_match_user_deny_rule': ('tools.approval', '_match_user_deny_rule'),
    'appr_u_user_deny_block_result': ('tools.approval', '_user_deny_block_result'),
    'appr_u_command_parser_limit_exceeded': ('tools.approval', '_command_parser_limit_exceeded'),
    'appr_u_shell_tokens_with_spans': ('tools.approval', '_shell_tokens_with_spans'),
    'appr_u_quoted_grep_pattern_spans': ('tools.approval', '_quoted_grep_pattern_spans'),
    'appr_u_grep_safe_detection_variant': ('tools.approval', '_grep_safe_detection_variant'),
    'appr_u_interpreter_family': ('tools.approval', '_interpreter_family'),
    'appr_u_shell_segment_tokens': ('tools.approval', '_shell_segment_tokens'),
    'appr_u_iter_top_level_shell_segments': ('tools.approval', '_iter_top_level_shell_segments'),
    'appr_u_split_option': ('tools.approval', '_split_option'),
    'appr_u_interpreter_exec_flag': ('tools.approval', '_interpreter_exec_flag'),
    'appr_u_bash_exec_payload': ('tools.approval', '_bash_exec_payload'),
    'appr_u_read_tool_exec_flag': ('tools.approval', '_read_tool_exec_flag'),
    'appr_u_execution_flag_findings': ('tools.approval', '_execution_flag_findings'),
    'appr_u_is_verification_artifact_cleanup': ('tools.approval', '_is_verification_artifact_cleanup'),
    'appr_u_run_approval_gate': ('tools.approval', '_run_approval_gate'),
    'appr_request_tool_approval': ('tools.approval', 'request_tool_approval'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_approval_wrappers.py <cases.json>\n"); return 2
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
