"""AUTO-GENERATED integration oracle for port_weixin_wrappers (gen_integration_oracle.py)."""
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
MODS['gateway.platforms.weixin'] = _load('gateway/platforms/weixin.py')

DISPATCH = {
    'wx_u_make_ssl_connector': ('gateway.platforms.weixin', '_make_ssl_connector'),
    'wx_save_weixin_account': ('gateway.platforms.weixin', 'save_weixin_account'),
    'wx_load_weixin_account': ('gateway.platforms.weixin', 'load_weixin_account'),
    'wx_u_api_get': ('gateway.platforms.weixin', '_api_get'),
    'wx_u_get_config': ('gateway.platforms.weixin', '_get_config'),
    'wx_u_get_upload_url': ('gateway.platforms.weixin', '_get_upload_url'),
    'wx_u_upload_ciphertext': ('gateway.platforms.weixin', '_upload_ciphertext'),
    'wx_u_download_bytes': ('gateway.platforms.weixin', '_download_bytes'),
    'wx_u_download_and_decrypt_media': ('gateway.platforms.weixin', '_download_and_decrypt_media'),
    'wx_u_save_sync_buf': ('gateway.platforms.weixin', '_save_sync_buf'),
    'wx_qr_login': ('gateway.platforms.weixin', 'qr_login'),
    'wx_u_poll_loop': ('gateway.platforms.weixin', '_poll_loop'),
    'wx_u_process_message_safe': ('gateway.platforms.weixin', '_process_message_safe'),
    'wx_u_is_dm_intake_allowed': ('gateway.platforms.weixin', '_is_dm_intake_allowed'),
    'wx_u_text_batch_key': ('gateway.platforms.weixin', '_text_batch_key'),
    'wx_u_enqueue_text_event': ('gateway.platforms.weixin', '_enqueue_text_event'),
    'wx_u_flush_text_batch': ('gateway.platforms.weixin', '_flush_text_batch'),
    'wx_u_collect_media': ('gateway.platforms.weixin', '_collect_media'),
    'wx_u_download_image': ('gateway.platforms.weixin', '_download_image'),
    'wx_u_download_video': ('gateway.platforms.weixin', '_download_video'),
    'wx_u_download_voice': ('gateway.platforms.weixin', '_download_voice'),
    'wx_u_maybe_fetch_typing_ticket': ('gateway.platforms.weixin', '_maybe_fetch_typing_ticket'),
    'wx_u_split_text': ('gateway.platforms.weixin', '_split_text'),
    'wx_u_open_rate_limit_circuit': ('gateway.platforms.weixin', '_open_rate_limit_circuit'),
    'wx_u_record_rate_limit_event': ('gateway.platforms.weixin', '_record_rate_limit_event'),
    'wx_u_reset_rate_limit_circuit': ('gateway.platforms.weixin', '_reset_rate_limit_circuit'),
    'wx_u_send_text_chunk': ('gateway.platforms.weixin', '_send_text_chunk'),
    'wx_u_send_text_chunk_locked': ('gateway.platforms.weixin', '_send_text_chunk_locked'),
    'wx_u_ensure_typing_ticket': ('gateway.platforms.weixin', '_ensure_typing_ticket'),
    'wx_u_download_remote_media': ('gateway.platforms.weixin', '_download_remote_media'),
    'wx_u_outbound_media_builder': ('gateway.platforms.weixin', '_outbound_media_builder'),
    'wx_send_weixin_direct': ('gateway.platforms.weixin', 'send_weixin_direct'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_weixin_wrappers.py <cases.json>\n"); return 2
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
