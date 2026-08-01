"""AUTO-GENERATED integration oracle for port_yuanbao_wrappers (gen_integration_oracle.py)."""
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
MODS['gateway.platforms.yuanbao'] = _load('gateway/platforms/yuanbao.py')

DISPATCH = {
    'yb_u__repr__': ('gateway.platforms.yuanbao', '__repr__'),
    'yb_use_before': ('gateway.platforms.yuanbao', 'use_before'),
    'yb_use_after': ('gateway.platforms.yuanbao', 'use_after'),
    'yb_middleware_names': ('gateway.platforms.yuanbao', 'middleware_names'),
    'yb_convert_json_msg_body': ('gateway.platforms.yuanbao', 'convert_json_msg_body'),
    'yb_parse_json_push': ('gateway.platforms.yuanbao', 'parse_json_push'),
    'yb_u_decode_single': ('gateway.platforms.yuanbao', '_decode_single'),
    'yb_u_handle_recall': ('gateway.platforms.yuanbao', '_handle_recall'),
    'yb_u_find_processing_session': ('gateway.platforms.yuanbao', '_find_processing_session'),
    'yb_u_interrupt_for_recall': ('gateway.platforms.yuanbao', '_interrupt_for_recall'),
    'yb_u_schedule_content_redact': ('gateway.platforms.yuanbao', '_schedule_content_redact'),
    'yb_u_patch_transcript': ('gateway.platforms.yuanbao', '_patch_transcript'),
    'yb_u_is_self_reference': ('gateway.platforms.yuanbao', '_is_self_reference'),
    'yb_is_dm_allowed': ('gateway.platforms.yuanbao', 'is_dm_allowed'),
    'yb_is_dm_intake_allowed': ('gateway.platforms.yuanbao', 'is_dm_intake_allowed'),
    'yb_is_group_allowed': ('gateway.platforms.yuanbao', 'is_group_allowed'),
    'yb_dm_policy': ('gateway.platforms.yuanbao', 'dm_policy'),
    'yb_group_policy': ('gateway.platforms.yuanbao', 'group_policy'),
    'yb_u_format_shared_link': ('gateway.platforms.yuanbao', '_format_shared_link'),
    'yb_u_format_link_understanding': ('gateway.platforms.yuanbao', '_format_link_understanding'),
    'yb_u_parse_resource_id': ('gateway.platforms.yuanbao', '_parse_resource_id'),
    'yb_u_rewrite_slash_command': ('gateway.platforms.yuanbao', '_rewrite_slash_command'),
    'yb_u_extract_inbound_media_refs': ('gateway.platforms.yuanbao', '_extract_inbound_media_refs'),
    'yb_u_extract_link_urls': ('gateway.platforms.yuanbao', '_extract_link_urls'),
    'yb_u_extract_forwarded_records': ('gateway.platforms.yuanbao', '_extract_forwarded_records'),
    'yb_is_skippable_placeholder': ('gateway.platforms.yuanbao', 'is_skippable_placeholder'),
    'yb_u_rewrite_slash_command_2': ('gateway.platforms.yuanbao', '_rewrite_slash_command'),
    'yb_u_detect_owner_command': ('gateway.platforms.yuanbao', '_detect_owner_command'),
    'yb_u_is_at_bot': ('gateway.platforms.yuanbao', '_is_at_bot'),
    'yb_u_extract_bot_mention_text': ('gateway.platforms.yuanbao', '_extract_bot_mention_text'),
    'yb_u_build_group_channel_prompt': ('gateway.platforms.yuanbao', '_build_group_channel_prompt'),
    'yb_u_observe_group_message': ('gateway.platforms.yuanbao', '_observe_group_message'),
    'yb_u_extract_quote_context': ('gateway.platforms.yuanbao', '_extract_quote_context'),
    'yb_u_extract_media_refs_from_transcript': ('gateway.platforms.yuanbao', '_extract_media_refs_from_transcript'),
    'yb_u_send_loading_heartbeat': ('gateway.platforms.yuanbao', '_send_loading_heartbeat'),
    'yb_u_media_marker': ('gateway.platforms.yuanbao', '_media_marker'),
    'yb_u_walk_forward_msgs': ('gateway.platforms.yuanbao', '_walk_forward_msgs'),
    'yb_build_forward_text': ('gateway.platforms.yuanbao', 'build_forward_text'),
    'yb_u_get_cached_resource': ('gateway.platforms.yuanbao', '_get_cached_resource'),
    'yb_u_put_cached_resource': ('gateway.platforms.yuanbao', '_put_cached_resource'),
    'yb_u_append_cached_resource': ('gateway.platforms.yuanbao', '_append_cached_resource'),
    'yb_u_guess_image_ext_from_url': ('gateway.platforms.yuanbao', '_guess_image_ext_from_url'),
    'yb_u_fetch_resource_url': ('gateway.platforms.yuanbao', '_fetch_resource_url'),
    'yb_u_resolve_download_url': ('gateway.platforms.yuanbao', '_resolve_download_url'),
    'yb_u_download_and_cache': ('gateway.platforms.yuanbao', '_download_and_cache'),
    'yb_u_resolve_media_urls': ('gateway.platforms.yuanbao', '_resolve_media_urls'),
    'yb_u_resolve_ybres_refs': ('gateway.platforms.yuanbao', '_resolve_ybres_refs'),
    'yb_u_collect_observed_media': ('gateway.platforms.yuanbao', '_collect_observed_media'),
    'yb_u_resolve_quote_media': ('gateway.platforms.yuanbao', '_resolve_quote_media'),
    'yb_u_collect_quote_local_media': ('gateway.platforms.yuanbao', '_collect_quote_local_media'),
    'yb_u_consume_group_queue': ('gateway.platforms.yuanbao', '_consume_group_queue'),
    'yb_build': ('gateway.platforms.yuanbao', 'build'),
    'yb_connect_id': ('gateway.platforms.yuanbao', 'connect_id'),
    'yb_reconnect_attempts': ('gateway.platforms.yuanbao', 'reconnect_attempts'),
    'yb_u_extract_connect_id': ('gateway.platforms.yuanbao', '_extract_connect_id'),
    'yb_u_heartbeat_loop': ('gateway.platforms.yuanbao', '_heartbeat_loop'),
    'yb_u_receive_loop': ('gateway.platforms.yuanbao', '_receive_loop'),
    'yb_u_extract_sender_key': ('gateway.platforms.yuanbao', '_extract_sender_key'),
    'yb_u_push_to_inbound': ('gateway.platforms.yuanbao', '_push_to_inbound'),
    'yb_u_flush_inbound_buffer': ('gateway.platforms.yuanbao', '_flush_inbound_buffer'),
    'yb_send_biz_request': ('gateway.platforms.yuanbao', 'send_biz_request'),
    'yb_schedule_reconnect': ('gateway.platforms.yuanbao', 'schedule_reconnect'),
    'yb_u_reconnect_with_backoff': ('gateway.platforms.yuanbao', '_reconnect_with_backoff'),
    'yb_u_do_reconnect': ('gateway.platforms.yuanbao', '_do_reconnect'),
    'yb_u_cleanup_ws': ('gateway.platforms.yuanbao', '_cleanup_ws'),
    'yb_acquire_file': ('gateway.platforms.yuanbao', 'acquire_file'),
    'yb_build_msg_body': ('gateway.platforms.yuanbao', 'build_msg_body'),
    'yb_needs_cos_upload': ('gateway.platforms.yuanbao', 'needs_cos_upload'),
    'yb_acquire_file_2': ('gateway.platforms.yuanbao', 'acquire_file'),
    'yb_build_msg_body_2': ('gateway.platforms.yuanbao', 'build_msg_body'),
    'yb_acquire_file_3': ('gateway.platforms.yuanbao', 'acquire_file'),
    'yb_build_msg_body_3': ('gateway.platforms.yuanbao', 'build_msg_body'),
    'yb_acquire_file_4': ('gateway.platforms.yuanbao', 'acquire_file'),
    'yb_build_msg_body_4': ('gateway.platforms.yuanbao', 'build_msg_body'),
    'yb_acquire_file_5': ('gateway.platforms.yuanbao', 'acquire_file'),
    'yb_build_msg_body_5': ('gateway.platforms.yuanbao', 'build_msg_body'),
    'yb_needs_cos_upload_2': ('gateway.platforms.yuanbao', 'needs_cos_upload'),
    'yb_acquire_file_6': ('gateway.platforms.yuanbao', 'acquire_file'),
    'yb_build_msg_body_6': ('gateway.platforms.yuanbao', 'build_msg_body'),
    'yb_query_group_info_raw': ('gateway.platforms.yuanbao', 'query_group_info_raw'),
    'yb_get_group_member_list_raw': ('gateway.platforms.yuanbao', 'get_group_member_list_raw'),
    'yb_query_session_members': ('gateway.platforms.yuanbao', 'query_session_members'),
    'yb_send_heartbeat_once': ('gateway.platforms.yuanbao', 'send_heartbeat_once'),
    'yb_u_worker': ('gateway.platforms.yuanbao', '_worker'),
    'yb_u_notifier': ('gateway.platforms.yuanbao', '_notifier'),
    'yb_cancel': ('gateway.platforms.yuanbao', 'cancel'),
    'yb_register_handler': ('gateway.platforms.yuanbao', 'register_handler'),
    'yb_get_chat_lock': ('gateway.platforms.yuanbao', 'get_chat_lock'),
    'yb_send_media': ('gateway.platforms.yuanbao', 'send_media'),
    'yb_send_direct': ('gateway.platforms.yuanbao', 'send_direct'),
    'yb_dispatch_msg_body': ('gateway.platforms.yuanbao', 'dispatch_msg_body'),
    'yb_send_text_chunk': ('gateway.platforms.yuanbao', 'send_text_chunk'),
    'yb_send_c2c_message': ('gateway.platforms.yuanbao', 'send_c2c_message'),
    'yb_send_group_message': ('gateway.platforms.yuanbao', 'send_group_message'),
    'yb_u_build_msg_body_with_mentions': ('gateway.platforms.yuanbao', '_build_msg_body_with_mentions'),
    'yb_send_c2c_msg_body': ('gateway.platforms.yuanbao', 'send_c2c_msg_body'),
    'yb_send_group_msg_body': ('gateway.platforms.yuanbao', 'send_group_msg_body'),
    'yb_u_dispatch_encoded': ('gateway.platforms.yuanbao', '_dispatch_encoded'),
    'yb_validate_media': ('gateway.platforms.yuanbao', 'validate_media'),
    'yb_strip_cron_wrapper': ('gateway.platforms.yuanbao', 'strip_cron_wrapper'),
    'yb_u_handle_send_start': ('gateway.platforms.yuanbao', '_handle_send_start'),
    'yb_u_handle_send_finish': ('gateway.platforms.yuanbao', '_handle_send_finish'),
    'yb_send_media_2': ('gateway.platforms.yuanbao', 'send_media'),
    'yb_send_direct_2': ('gateway.platforms.yuanbao', 'send_direct'),
    'yb_start_typing': ('gateway.platforms.yuanbao', 'start_typing'),
    'yb_start_slow_notifier': ('gateway.platforms.yuanbao', 'start_slow_notifier'),
    'yb_cancel_slow_notifier': ('gateway.platforms.yuanbao', 'cancel_slow_notifier'),
    'yb_get_chat_lock_2': ('gateway.platforms.yuanbao', 'get_chat_lock'),
    'yb_u_chat_locks': ('gateway.platforms.yuanbao', '_chat_locks'),
    'yb_validate_media_2': ('gateway.platforms.yuanbao', 'validate_media'),
    'yb_set_active': ('gateway.platforms.yuanbao', 'set_active'),
    'yb_u_track_task': ('gateway.platforms.yuanbao', '_track_task'),
    'yb_u_sender_may_designate_home': ('gateway.platforms.yuanbao', '_sender_may_designate_home'),
    'yb_u_process_message_background': ('gateway.platforms.yuanbao', '_process_message_background'),
    'yb_u_get_cached_token': ('gateway.platforms.yuanbao', '_get_cached_token'),
    'yb_send_yuanbao_direct': ('gateway.platforms.yuanbao', 'send_yuanbao_direct'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_yuanbao_wrappers.py <cases.json>\n"); return 2
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
