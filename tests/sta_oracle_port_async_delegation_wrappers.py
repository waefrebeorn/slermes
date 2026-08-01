"""AUTO-GENERATED integration oracle for port_async_delegation_wrappers (gen_integration_oracle.py)."""
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
MODS['tools.async_delegation'] = _load('tools/async_delegation.py')

DISPATCH = {
    'adel_u_db_path': ('tools.async_delegation', '_db_path'),
    'adel_u_connect': ('tools.async_delegation', '_connect'),
    'adel_u_initialize_schema': ('tools.async_delegation', '_initialize_schema'),
    'adel_u_transaction': ('tools.async_delegation', '_transaction'),
    'adel_u_persist_dispatch': ('tools.async_delegation', '_persist_dispatch'),
    'adel_u_delete_durable_delegation': ('tools.async_delegation', '_delete_durable_delegation'),
    'adel_u_prune_durable_records': ('tools.async_delegation', '_prune_durable_records'),
    'adel_u_persist_completion': ('tools.async_delegation', '_persist_completion'),
    'adel_u_note_delivery_attempt': ('tools.async_delegation', '_note_delivery_attempt'),
    'adel_recover_abandoned_delegations': ('tools.async_delegation', 'recover_abandoned_delegations'),
    'adel_restore_undelivered_completions': ('tools.async_delegation', 'restore_undelivered_completions'),
    'adel_mark_completion_delivered': ('tools.async_delegation', 'mark_completion_delivered'),
    'adel_claim_completion_delivery': ('tools.async_delegation', 'claim_completion_delivery'),
    'adel_claim_event_delivery': ('tools.async_delegation', 'claim_event_delivery'),
    'adel_release_completion_delivery': ('tools.async_delegation', 'release_completion_delivery'),
    'adel_drop_completion_delivery': ('tools.async_delegation', 'drop_completion_delivery'),
    'adel_complete_completion_delivery': ('tools.async_delegation', 'complete_completion_delivery'),
    'adel_complete_event_delivery': ('tools.async_delegation', 'complete_event_delivery'),
    'adel_release_event_delivery': ('tools.async_delegation', 'release_event_delivery'),
    'adel_get_durable_delegation': ('tools.async_delegation', 'get_durable_delegation'),
    'adel_u_get_executor': ('tools.async_delegation', '_get_executor'),
    'adel_u_new_delegation_id': ('tools.async_delegation', '_new_delegation_id'),
    'adel_u_current_origin_session_id': ('tools.async_delegation', '_current_origin_session_id'),
    'adel_dispatch_async_delegation': ('tools.async_delegation', 'dispatch_async_delegation'),
    'adel_u_push_completion_event': ('tools.async_delegation', '_push_completion_event'),
    'adel_dispatch_async_delegation_batch': ('tools.async_delegation', 'dispatch_async_delegation_batch'),
    'adel_u_finalize_batch': ('tools.async_delegation', '_finalize_batch'),
    'adel_list_async_delegations': ('tools.async_delegation', 'list_async_delegations'),
    'adel_interrupt_for_session': ('tools.async_delegation', 'interrupt_for_session'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_async_delegation_wrappers.py <cases.json>\n"); return 2
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
