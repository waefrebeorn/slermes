"""AUTO-GENERATED integration oracle for port_context_compressor_wrappers (gen_integration_oracle.py)."""
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
MODS['agent.context_compressor'] = _load('agent/context_compressor.py')

DISPATCH = {
    'ctxc_u_begin_compression_telemetry': ('agent.context_compressor', '_begin_compression_telemetry'),
    'ctxc_u_record_compression_regions': ('agent.context_compressor', '_record_compression_regions'),
    'ctxc_u_record_aux_compression_call': ('agent.context_compressor', '_record_aux_compression_call'),
    'ctxc_u_load_fallback_compression_streak': ('agent.context_compressor', '_load_fallback_compression_streak'),
    'ctxc_u_persist_fallback_compression_streak': ('agent.context_compressor', '_persist_fallback_compression_streak'),
    'ctxc_u_load_ineffective_compression_count': ('agent.context_compressor', '_load_ineffective_compression_count'),
    'ctxc_u_persist_ineffective_compression_count': ('agent.context_compressor', '_persist_ineffective_compression_count'),
    'ctxc_u_record_ineffective_compression_verdict': ('agent.context_compressor', '_record_ineffective_compression_verdict'),
    'ctxc_record_completed_compaction': ('agent.context_compressor', 'record_completed_compaction'),
    'ctxc_snapshot_preflight_display_tokens': ('agent.context_compressor', 'snapshot_preflight_display_tokens'),
    'ctxc_rollback_interrupted_preflight_display_tokens': ('agent.context_compressor', 'rollback_interrupted_preflight_display_tokens'),
    'ctxc_should_compress_info': ('agent.context_compressor', 'should_compress_info'),
    'ctxc_u_compression_block_reason': ('agent.context_compressor', '_compression_block_reason'),
    'ctxc_u_refresh_durable_guards': ('agent.context_compressor', '_refresh_durable_guards'),
    'ctxc_u_automatic_compression_blocked': ('agent.context_compressor', '_automatic_compression_blocked'),
    'ctxc_u_automatic_compression_blocked_locally': ('agent.context_compressor', '_automatic_compression_blocked_locally'),
    'ctxc_prune_tool_results_only': ('agent.context_compressor', 'prune_tool_results_only'),
    'ctxc_u_bound_summary_input': ('agent.context_compressor', '_bound_summary_input'),
    'ctxc_u_validate_summary_user_provenance': ('agent.context_compressor', '_validate_summary_user_provenance'),
    'ctxc_u_latest_user_task_snapshot': ('agent.context_compressor', '_latest_user_task_snapshot'),
    'ctxc_u_ground_historical_task_snapshot': ('agent.context_compressor', '_ground_historical_task_snapshot'),
    'ctxc_u_ensure_last_n_user_messages_in_tail': ('agent.context_compressor', '_ensure_last_n_user_messages_in_tail'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_context_compressor_wrappers.py <cases.json>\n"); return 2
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
