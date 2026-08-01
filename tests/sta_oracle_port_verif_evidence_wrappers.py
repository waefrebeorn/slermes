"""AUTO-GENERATED integration oracle for port_verif_evidence_wrappers (gen_integration_oracle.py)."""
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
MODS['agent.verification_evidence'] = _load('agent/verification_evidence.py')

DISPATCH = {
    'vev_u_retention_cutoff': ('agent.verification_evidence', '_retention_cutoff'),
    'vev_u_db_path': ('agent.verification_evidence', '_db_path'),
    'vev_u_connect': ('agent.verification_evidence', '_connect'),
    'vev_u_transaction': ('agent.verification_evidence', '_transaction'),
    'vev_u_ensure_schema': ('agent.verification_evidence', '_ensure_schema'),
    'vev_u_split_segment_tokens': ('agent.verification_evidence', '_split_segment_tokens'),
    'vev_u_clean_token': ('agent.verification_evidence', '_clean_token'),
    'vev_u_canonical_tokens': ('agent.verification_evidence', '_canonical_tokens'),
    'vev_u_strip_command_prefix': ('agent.verification_evidence', '_strip_command_prefix'),
    'vev_u_equivalent_needles': ('agent.verification_evidence', '_equivalent_needles'),
    'vev_u_is_under_root': ('agent.verification_evidence', '_is_under_root'),
    'vev_u_ad_hoc_script_args': ('agent.verification_evidence', '_ad_hoc_script_args'),
    'vev_u_summarize_output': ('agent.verification_evidence', '_summarize_output'),
    'vev_u_prune_old_events': ('agent.verification_evidence', '_prune_old_events'),
    'vev_classify_verification_command': ('agent.verification_evidence', 'classify_verification_command'),
    'vev_record_terminal_result': ('agent.verification_evidence', 'record_terminal_result'),
    'vev_mark_workspace_edited': ('agent.verification_evidence', 'mark_workspace_edited'),
    'vev_verification_status': ('agent.verification_evidence', 'verification_status'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_verif_evidence_wrappers.py <cases.json>\n"); return 2
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
