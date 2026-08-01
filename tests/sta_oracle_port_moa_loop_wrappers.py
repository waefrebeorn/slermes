"""AUTO-GENERATED integration oracle for port_moa_loop_wrappers (gen_integration_oracle.py)."""
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
MODS['agent.moa_loop'] = _load('agent/moa_loop.py')

DISPATCH = {
    'moa_u_redact_reference_text': ('agent.moa_loop', '_redact_reference_text'),
    'moa_u_moa_privacy_mode': ('agent.moa_loop', '_moa_privacy_mode'),
    'moa_u_redact_reference_outputs': ('agent.moa_loop', '_redact_reference_outputs'),
    'moa_u_redact_trace_messages': ('agent.moa_loop', '_redact_trace_messages'),
    'moa_u_redact_trace_accounting': ('agent.moa_loop', '_redact_trace_accounting'),
    'moa_u_slot_label': ('agent.moa_loop', '_slot_label'),
    'moa_u_slot_reasoning_config': ('agent.moa_loop', '_slot_reasoning_config'),
    'moa_u_aggregator_reasoning_config': ('agent.moa_loop', '_aggregator_reasoning_config'),
    'moa_u_slot_runtime': ('agent.moa_loop', '_slot_runtime'),
    'moa_u_merge_slot_extra_body': ('agent.moa_loop', '_merge_slot_extra_body'),
    'moa_u_maybe_apply_moa_cache_control': ('agent.moa_loop', '_maybe_apply_moa_cache_control'),
    'moa_u_run_reference': ('agent.moa_loop', '_run_reference'),
    'moa_u_trim_messages_for_reference': ('agent.moa_loop', '_trim_messages_for_reference'),
    'moa_u_run_references_parallel': ('agent.moa_loop', '_run_references_parallel'),
    'moa_u_truncate_tool_result': ('agent.moa_loop', '_truncate_tool_result'),
    'moa_u_render_tool_calls': ('agent.moa_loop', '_render_tool_calls'),
    'moa_u_reference_messages': ('agent.moa_loop', '_reference_messages'),
    'moa_u_preset_temperature': ('agent.moa_loop', '_preset_temperature'),
    'moa_u_is_failed_reference': ('agent.moa_loop', '_is_failed_reference'),
    'moa_u_successful_references': ('agent.moa_loop', '_successful_references'),
    'moa_u_failed_reference_labels': ('agent.moa_loop', '_failed_reference_labels'),
    'moa_u_degraded_notice': ('agent.moa_loop', '_degraded_notice'),
    'moa_aggregate_moa_context': ('agent.moa_loop', 'aggregate_moa_context'),
    'moa_u_attach_reference_guidance': ('agent.moa_loop', '_attach_reference_guidance'),
    'moa_consume_reference_usage': ('agent.moa_loop', 'consume_reference_usage'),
    'moa_u_record_late_reference_accounting': ('agent.moa_loop', '_record_late_reference_accounting'),
    'moa_consume_and_save_trace': ('agent.moa_loop', 'consume_and_save_trace'),
    'moa_prepare': ('agent.moa_loop', 'prepare'),
    'moa_rebase_prepared_request': ('agent.moa_loop', 'rebase_prepared_request'),
    'moa_u_call_prepared_aggregator': ('agent.moa_loop', '_call_prepared_aggregator'),
    'moa_consume_reference_usage_2': ('agent.moa_loop', 'consume_reference_usage'),
    'moa_last_aggregator_slot': ('agent.moa_loop', 'last_aggregator_slot'),
    'moa_consume_and_save_trace_2': ('agent.moa_loop', 'consume_and_save_trace'),
    'moa_build_moa_facade': ('agent.moa_loop', 'build_moa_facade'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_moa_loop_wrappers.py <cases.json>\n"); return 2
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
