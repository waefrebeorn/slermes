"""AUTO-GENERATED integration oracle for port_bluebubbles_wrappers (gen_integration_oracle.py)."""
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
MODS['gateway.platforms.bluebubbles'] = _load('gateway/platforms/bluebubbles.py')

DISPATCH = {
    'bb_check_bluebubbles_requirements': ('gateway.platforms.bluebubbles', 'check_bluebubbles_requirements'),
    'bb_u_normalize_server_url': ('gateway.platforms.bluebubbles', '_normalize_server_url'),
    'bb_u_api_url': ('gateway.platforms.bluebubbles', '_api_url'),
    'bb_u_compile_mention_patterns': ('gateway.platforms.bluebubbles', '_compile_mention_patterns'),
    'bb_u_message_matches_mention_patterns': ('gateway.platforms.bluebubbles', '_message_matches_mention_patterns'),
    'bb_u_clean_mention_text': ('gateway.platforms.bluebubbles', '_clean_mention_text'),
    'bb_u_api_post': ('gateway.platforms.bluebubbles', '_api_post'),
    'bb_u_webhook_url': ('gateway.platforms.bluebubbles', '_webhook_url'),
    'bb_u_webhook_register_url': ('gateway.platforms.bluebubbles', '_webhook_register_url'),
    'bb_u_webhook_register_url_for_log': ('gateway.platforms.bluebubbles', '_webhook_register_url_for_log'),
    'bb_u_find_registered_webhooks': ('gateway.platforms.bluebubbles', '_find_registered_webhooks'),
    'bb_u_register_webhook': ('gateway.platforms.bluebubbles', '_register_webhook'),
    'bb_u_unregister_webhook': ('gateway.platforms.bluebubbles', '_unregister_webhook'),
    'bb_u_resolve_chat_guid': ('gateway.platforms.bluebubbles', '_resolve_chat_guid'),
    'bb_u_create_chat_for_handle': ('gateway.platforms.bluebubbles', '_create_chat_for_handle'),
    'bb_mark_read': ('gateway.platforms.bluebubbles', 'mark_read'),
    'bb_u_download_attachment': ('gateway.platforms.bluebubbles', '_download_attachment'),
    'bb_u_extract_payload_record': ('gateway.platforms.bluebubbles', '_extract_payload_record'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_bluebubbles_wrappers.py <cases.json>\n"); return 2
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
