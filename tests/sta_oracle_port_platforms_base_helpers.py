"""AUTO-GENERATED integration oracle for port_platforms_base_helpers (gen_integration_oracle.py)."""
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
MODS['gateway.platforms.base'] = _load('gateway/platforms/base.py')

DISPATCH = {
    'gw_base__is_animation_url': ('gateway.platforms.base', '_is_animation_url'),
    'gw_base__is_command': ('gateway.platforms.base', 'is_command'),
    'gw_base__ssrf_redirect_guard': ('gateway.platforms.base', '_ssrf_redirect_guard'),
    'gw_base__should_auto_tts_for_chat': ('gateway.platforms.base', '_should_auto_tts_for_chat'),
    'gw_base__is_retryable_error': ('gateway.platforms.base', '_is_retryable_error'),
    'gw_base__stop_typing_refresh': ('gateway.platforms.base', '_stop_typing_refresh'),
    'gw_base__discard_text_debounce': ('gateway.platforms.base', '_discard_text_debounce'),
    'gw_base__release_session_guard': ('gateway.platforms.base', '_release_session_guard'),
    'gw_base__session_task_is_stale': ('gateway.platforms.base', '_session_task_is_stale'),
    'gw_base__heal_stale_session_lock': ('gateway.platforms.base', '_heal_stale_session_lock'),
    'gw_base__drain_pending_after_session_command': ('gateway.platforms.base', '_drain_pending_after_session_command'),
    'gw_base__process_message_background': ('gateway.platforms.base', '_process_message_background'),
    'gw_base__cleanup_finished_session_task': ('gateway.platforms.base', '_cleanup_finished_session_task'),
    'gw_base__has_pending_interrupt': ('gateway.platforms.base', 'has_pending_interrupt'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_platforms_base_helpers.py <cases.json>\n"); return 2
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
