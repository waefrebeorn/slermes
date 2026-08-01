"""AUTO-GENERATED integration oracle for port_memory_manager_wrappers (gen_integration_oracle.py)."""
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
MODS['agent.memory_manager'] = _load('agent/memory_manager.py')

DISPATCH = {
    'mm_memory_provider_tools_enabled': ('agent.memory_manager', 'memory_provider_tools_enabled'),
    'mm_inject_memory_provider_tools': ('agent.memory_manager', 'inject_memory_provider_tools'),
    'mm_u_find_boundary_open_tag': ('agent.memory_manager', '_find_boundary_open_tag'),
    'mm_u_max_pending_open_suffix': ('agent.memory_manager', '_max_pending_open_suffix'),
    'mm_u_has_block_opener_suffix': ('agent.memory_manager', '_has_block_opener_suffix'),
    'mm_u_append_visible': ('agent.memory_manager', '_append_visible'),
    'mm_u_update_block_boundary': ('agent.memory_manager', '_update_block_boundary'),
    'mm_add_provider': ('agent.memory_manager', 'add_provider'),
    'mm_prefetch_all': ('agent.memory_manager', 'prefetch_all'),
    'mm_u_prefetch_provider': ('agent.memory_manager', '_prefetch_provider'),
    'mm_queue_prefetch_all': ('agent.memory_manager', 'queue_prefetch_all'),
    'mm_u_provider_sync_accepts_messages': ('agent.memory_manager', '_provider_sync_accepts_messages'),
    'mm_sync_all': ('agent.memory_manager', 'sync_all'),
    'mm_u_submit_background': ('agent.memory_manager', '_submit_background'),
    'mm_u_forget_background_future': ('agent.memory_manager', '_forget_background_future'),
    'mm_u_get_sync_executor': ('agent.memory_manager', '_get_sync_executor'),
    'mm_flush_pending': ('agent.memory_manager', 'flush_pending'),
    'mm_get_all_tool_schemas': ('agent.memory_manager', 'get_all_tool_schemas'),
    'mm_get_all_tool_names': ('agent.memory_manager', 'get_all_tool_names'),
    'mm_on_turn_start': ('agent.memory_manager', 'on_turn_start'),
    'mm_commit_session_boundary_async': ('agent.memory_manager', 'commit_session_boundary_async'),
    'mm_on_session_switch': ('agent.memory_manager', 'on_session_switch'),
    'mm_on_pre_compress': ('agent.memory_manager', 'on_pre_compress'),
    'mm_u_provider_memory_write_metadata_mode': ('agent.memory_manager', '_provider_memory_write_metadata_mode'),
    'mm_on_memory_write': ('agent.memory_manager', 'on_memory_write'),
    'mm_u_memory_tool_result_succeeded': ('agent.memory_manager', '_memory_tool_result_succeeded'),
    'mm_notify_memory_tool_write': ('agent.memory_manager', 'notify_memory_tool_write'),
    'mm_shutdown_drain_state': ('agent.memory_manager', 'shutdown_drain_state'),
    'mm_u_drain_sync_executor': ('agent.memory_manager', '_drain_sync_executor'),
    'mm_initialize_all': ('agent.memory_manager', 'initialize_all'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_memory_manager_wrappers.py <cases.json>\n"); return 2
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
