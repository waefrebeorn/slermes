"""AUTO-GENERATED integration oracle for port_qqbot_wrappers (gen_integration_oracle.py)."""
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
MODS['gateway.platforms.qqbot.adapter'] = _load('gateway/platforms/qqbot/adapter.py')

DISPATCH = {
    'qqbot_check_qq_requirements': ('gateway.platforms.qqbot.adapter', 'check_qq_requirements'),
    'qqbot_u_coerce_list': ('gateway.platforms.qqbot.adapter', '_coerce_list'),
    'qqbot_u_log_tag': ('gateway.platforms.qqbot.adapter', '_log_tag'),
    'qqbot_u_fail_pending': ('gateway.platforms.qqbot.adapter', '_fail_pending'),
    'qqbot_u_mark_transport_disconnected': ('gateway.platforms.qqbot.adapter', '_mark_transport_disconnected'),
    'qqbot_u_ensure_token': ('gateway.platforms.qqbot.adapter', '_ensure_token'),
    'qqbot_u_open_ws': ('gateway.platforms.qqbot.adapter', '_open_ws'),
    'qqbot_u_listen_loop': ('gateway.platforms.qqbot.adapter', '_listen_loop'),
    'qqbot_u_reconnect': ('gateway.platforms.qqbot.adapter', '_reconnect'),
    'qqbot_u_read_events': ('gateway.platforms.qqbot.adapter', '_read_events'),
    'qqbot_u_heartbeat_loop': ('gateway.platforms.qqbot.adapter', '_heartbeat_loop'),
    'qqbot_u_send_identify': ('gateway.platforms.qqbot.adapter', '_send_identify'),
    'qqbot_u_send_resume': ('gateway.platforms.qqbot.adapter', '_send_resume'),
    'qqbot_u_dispatch_payload': ('gateway.platforms.qqbot.adapter', '_dispatch_payload'),
    'qqbot_u_handle_ready': ('gateway.platforms.qqbot.adapter', '_handle_ready'),
    'qqbot_u_parse_json': ('gateway.platforms.qqbot.adapter', '_parse_json'),
    'qqbot_u_next_msg_seq': ('gateway.platforms.qqbot.adapter', '_next_msg_seq'),
    'qqbot_u_on_message': ('gateway.platforms.qqbot.adapter', '_on_message'),
    'qqbot_set_interaction_callback': ('gateway.platforms.qqbot.adapter', 'set_interaction_callback'),
    'qqbot_u_on_interaction': ('gateway.platforms.qqbot.adapter', '_on_interaction'),
    'qqbot_u_acknowledge_interaction': ('gateway.platforms.qqbot.adapter', '_acknowledge_interaction'),
    'qqbot_u_parse_gateway_session_key': ('gateway.platforms.qqbot.adapter', '_parse_gateway_session_key'),
    'qqbot_u_is_authorized_interaction_for_session': ('gateway.platforms.qqbot.adapter', '_is_authorized_interaction_for_session'),
    'qqbot_u_default_interaction_dispatch': ('gateway.platforms.qqbot.adapter', '_default_interaction_dispatch'),
    'qqbot_u_write_update_response': ('gateway.platforms.qqbot.adapter', '_write_update_response'),
    'qqbot_u_handle_c2c_message': ('gateway.platforms.qqbot.adapter', '_handle_c2c_message'),
    'qqbot_u_handle_group_message': ('gateway.platforms.qqbot.adapter', '_handle_group_message'),
    'qqbot_u_handle_guild_message': ('gateway.platforms.qqbot.adapter', '_handle_guild_message'),
    'qqbot_u_handle_dm_message': ('gateway.platforms.qqbot.adapter', '_handle_dm_message'),
    'qqbot_u_process_quoted_context': ('gateway.platforms.qqbot.adapter', '_process_quoted_context'),
    'qqbot_u_merge_quote_into': ('gateway.platforms.qqbot.adapter', '_merge_quote_into'),
    'qqbot_u_detect_message_type': ('gateway.platforms.qqbot.adapter', '_detect_message_type'),
    'qqbot_u_process_attachments': ('gateway.platforms.qqbot.adapter', '_process_attachments'),
    'qqbot_u_download_and_cache': ('gateway.platforms.qqbot.adapter', '_download_and_cache'),
    'qqbot_u_is_voice_content_type': ('gateway.platforms.qqbot.adapter', '_is_voice_content_type'),
    'qqbot_u_qq_media_headers': ('gateway.platforms.qqbot.adapter', '_qq_media_headers'),
    'qqbot_u_stt_voice_attachment': ('gateway.platforms.qqbot.adapter', '_stt_voice_attachment'),
    'qqbot_u_convert_audio_to_wav_file': ('gateway.platforms.qqbot.adapter', '_convert_audio_to_wav_file'),
    'qqbot_u_guess_ext_from_data': ('gateway.platforms.qqbot.adapter', '_guess_ext_from_data'),
    'qqbot_u_looks_like_silk': ('gateway.platforms.qqbot.adapter', '_looks_like_silk'),
    'qqbot_u_convert_silk_to_wav': ('gateway.platforms.qqbot.adapter', '_convert_silk_to_wav'),
    'qqbot_u_convert_raw_to_wav': ('gateway.platforms.qqbot.adapter', '_convert_raw_to_wav'),
    'qqbot_u_convert_ffmpeg_to_wav': ('gateway.platforms.qqbot.adapter', '_convert_ffmpeg_to_wav'),
    'qqbot_u_resolve_stt_config': ('gateway.platforms.qqbot.adapter', '_resolve_stt_config'),
    'qqbot_u_call_stt': ('gateway.platforms.qqbot.adapter', '_call_stt'),
    'qqbot_u_convert_audio_to_wav': ('gateway.platforms.qqbot.adapter', '_convert_audio_to_wav'),
    'qqbot_u_api_request': ('gateway.platforms.qqbot.adapter', '_api_request'),
    'qqbot_u_upload_media': ('gateway.platforms.qqbot.adapter', '_upload_media'),
    'qqbot_u_wait_for_reconnection': ('gateway.platforms.qqbot.adapter', '_wait_for_reconnection'),
    'qqbot_u_send_chunk': ('gateway.platforms.qqbot.adapter', '_send_chunk'),
    'qqbot_u_send_c2c_text': ('gateway.platforms.qqbot.adapter', '_send_c2c_text'),
    'qqbot_u_send_group_text': ('gateway.platforms.qqbot.adapter', '_send_group_text'),
    'qqbot_u_send_guild_text': ('gateway.platforms.qqbot.adapter', '_send_guild_text'),
    'qqbot_send_approval_request': ('gateway.platforms.qqbot.adapter', 'send_approval_request'),
    'qqbot_send_exec_approval': ('gateway.platforms.qqbot.adapter', 'send_exec_approval'),
    'qqbot_u_build_text_body': ('gateway.platforms.qqbot.adapter', '_build_text_body'),
    'qqbot_u_send_media': ('gateway.platforms.qqbot.adapter', '_send_media'),
    'qqbot_u_upload_local_file': ('gateway.platforms.qqbot.adapter', '_upload_local_file'),
    'qqbot_u_load_media': ('gateway.platforms.qqbot.adapter', '_load_media'),
    'qqbot_u_is_url': ('gateway.platforms.qqbot.adapter', '_is_url'),
    'qqbot_u_guess_chat_type': ('gateway.platforms.qqbot.adapter', '_guess_chat_type'),
    'qqbot_u_strip_at_mention': ('gateway.platforms.qqbot.adapter', '_strip_at_mention'),
    'qqbot_u_is_dm_allowed': ('gateway.platforms.qqbot.adapter', '_is_dm_allowed'),
    'qqbot_u_is_dm_intake_allowed': ('gateway.platforms.qqbot.adapter', '_is_dm_intake_allowed'),
    'qqbot_u_is_group_allowed': ('gateway.platforms.qqbot.adapter', '_is_group_allowed'),
    'qqbot_u_entry_matches': ('gateway.platforms.qqbot.adapter', '_entry_matches'),
    'qqbot_u_parse_qq_timestamp': ('gateway.platforms.qqbot.adapter', '_parse_qq_timestamp'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_qqbot_wrappers.py <cases.json>\n"); return 2
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
