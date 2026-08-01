"""AUTO-GENERATED integration oracle for port_gateway_remaining_wrappers (gen_integration_oracle.py)."""
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
MODS['gateway.authz_mixin'] = _load('gateway/authz_mixin.py')
MODS['gateway.channel_directory'] = _load('gateway/channel_directory.py')
MODS['gateway.config'] = _load('gateway/config.py')
MODS['gateway.cwd_placeholder'] = _load('gateway/cwd_placeholder.py')
MODS['gateway.delivery'] = _load('gateway/delivery.py')
MODS['gateway.delivery_ledger'] = _load('gateway/delivery_ledger.py')
MODS['gateway.kanban_watchers'] = _load('gateway/kanban_watchers.py')
MODS['gateway.platform_registry'] = _load('gateway/platform_registry.py')
MODS['gateway.platforms.helpers'] = _load('gateway/platforms/helpers.py')
MODS['gateway.platforms.qqbot.chunked_upload'] = _load('gateway/platforms/qqbot/chunked_upload.py')
MODS['gateway.platforms.signal'] = _load('gateway/platforms/signal.py')
MODS['gateway.platforms.webhook_filters'] = _load('gateway/platforms/webhook_filters.py')
MODS['gateway.profile_routing'] = _load('gateway/profile_routing.py')
MODS['gateway.readiness'] = _load('gateway/readiness.py')
MODS['gateway.relay.adapter'] = _load('gateway/relay/adapter.py')
MODS['gateway.relay.ws_transport'] = _load('gateway/relay/ws_transport.py')
MODS['gateway.response_filters'] = _load('gateway/response_filters.py')
MODS['gateway.restart'] = _load('gateway/restart.py')
MODS['gateway.session_context'] = _load('gateway/session_context.py')
MODS['gateway.shutdown_watchdog'] = _load('gateway/shutdown_watchdog.py')
MODS['gateway.status_phrases'] = _load('gateway/status_phrases.py')
MODS['gateway.systemd_notify'] = _load('gateway/systemd_notify.py')
MODS['gateway.turn_lease'] = _load('gateway/turn_lease.py')
MODS['gateway.wake'] = _load('gateway/wake.py')

DISPATCH = {
    'gateway_platforms_signal_u_render_mentions': ('gateway.platforms.signal', '_render_mentions'),
    'gateway_platforms_signal_validate_signal_config': ('gateway.platforms.signal', 'validate_signal_config'),
    'gateway_platforms_signal_u_sse_listener': ('gateway.platforms.signal', '_sse_listener'),
    'gateway_platforms_signal_u_health_monitor': ('gateway.platforms.signal', '_health_monitor'),
    'gateway_platforms_signal_u_force_reconnect': ('gateway.platforms.signal', '_force_reconnect'),
    'gateway_platforms_signal_u_handle_envelope': ('gateway.platforms.signal', '_handle_envelope'),
    'gateway_platforms_signal_u_remember_recipient_identifiers': ('gateway.platforms.signal', '_remember_recipient_identifiers'),
    'gateway_platforms_signal_u_extract_contact_uuid': ('gateway.platforms.signal', '_extract_contact_uuid'),
    'gateway_platforms_signal_u_resolve_recipient': ('gateway.platforms.signal', '_resolve_recipient'),
    'gateway_platforms_signal_u_fetch_attachment': ('gateway.platforms.signal', '_fetch_attachment'),
    'gateway_platforms_signal_u_rpc': ('gateway.platforms.signal', '_rpc'),
    'gateway_platforms_signal_u_track_sent_timestamp': ('gateway.platforms.signal', '_track_sent_timestamp'),
    'gateway_platforms_signal_u_notify_batch_pacing': ('gateway.platforms.signal', '_notify_batch_pacing'),
    'gateway_platforms_signal_u_stop_typing_indicator': ('gateway.platforms.signal', '_stop_typing_indicator'),
    'gateway_platforms_signal_remove_reaction': ('gateway.platforms.signal', 'remove_reaction'),
    'gateway_platforms_signal_u_extract_reaction_target': ('gateway.platforms.signal', '_extract_reaction_target'),
    'gateway_platforms_signal_u_reactions_enabled': ('gateway.platforms.signal', '_reactions_enabled'),
    'gateway_delivery_ledger_u_db_path': ('gateway.delivery_ledger', '_db_path'),
    'gateway_delivery_ledger_u_connect': ('gateway.delivery_ledger', '_connect'),
    'gateway_delivery_ledger_u_initialize_schema': ('gateway.delivery_ledger', '_initialize_schema'),
    'gateway_delivery_ledger_u_transaction': ('gateway.delivery_ledger', '_transaction'),
    'gateway_delivery_ledger_u_owner_stamp': ('gateway.delivery_ledger', '_owner_stamp'),
    'gateway_delivery_ledger_u_owner_alive': ('gateway.delivery_ledger', '_owner_alive'),
    'gateway_delivery_ledger_compute_obligation_id': ('gateway.delivery_ledger', 'compute_obligation_id'),
    'gateway_delivery_ledger_record_obligation': ('gateway.delivery_ledger', 'record_obligation'),
    'gateway_delivery_ledger_mark_attempting': ('gateway.delivery_ledger', 'mark_attempting'),
    'gateway_delivery_ledger_mark_delivered': ('gateway.delivery_ledger', 'mark_delivered'),
    'gateway_delivery_ledger_mark_failed': ('gateway.delivery_ledger', 'mark_failed'),
    'gateway_delivery_ledger_u_update_state': ('gateway.delivery_ledger', '_update_state'),
    'gateway_delivery_ledger_sweep_recoverable': ('gateway.delivery_ledger', 'sweep_recoverable'),
    'gateway_delivery_ledger_ledger_enabled': ('gateway.delivery_ledger', 'ledger_enabled'),
    'gateway_delivery_ledger_debug_rows': ('gateway.delivery_ledger', 'debug_rows'),
    'gateway_shutdown_watchdog_u_schedule': ('gateway.shutdown_watchdog', '_schedule'),
    'gateway_shutdown_watchdog_u_tick': ('gateway.shutdown_watchdog', '_tick'),
    'gateway_shutdown_watchdog_cancel': ('gateway.shutdown_watchdog', 'cancel'),
    'gateway_shutdown_watchdog_is_alive': ('gateway.shutdown_watchdog', 'is_alive'),
    'gateway_shutdown_watchdog_u_arm_loop_floor_timer': ('gateway.shutdown_watchdog', '_arm_loop_floor_timer'),
    'gateway_shutdown_watchdog_start_loop_liveness_watchdog': ('gateway.shutdown_watchdog', 'start_loop_liveness_watchdog'),
    'gateway_shutdown_watchdog_u_process_hermes_home': ('gateway.shutdown_watchdog', '_process_hermes_home'),
    'gateway_shutdown_watchdog_get_loop_heartbeat_path': ('gateway.shutdown_watchdog', 'get_loop_heartbeat_path'),
    'gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path': ('gateway.shutdown_watchdog', 'get_shutdown_watchdog_dump_path'),
    'gateway_shutdown_watchdog_write_loop_heartbeat': ('gateway.shutdown_watchdog', 'write_loop_heartbeat'),
    'gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay': ('gateway.shutdown_watchdog', 'resolve_shutdown_watchdog_delay'),
    'gateway_shutdown_watchdog_u_write_watchdog_dump': ('gateway.shutdown_watchdog', '_write_watchdog_dump'),
    'gateway_shutdown_watchdog_arm_shutdown_watchdog': ('gateway.shutdown_watchdog', 'arm_shutdown_watchdog'),
    'gateway_shutdown_watchdog_loop_heartbeat_forever': ('gateway.shutdown_watchdog', 'loop_heartbeat_forever'),
    'gateway_status_phrases_u_clean_phrase_list': ('gateway.status_phrases', '_clean_phrase_list'),
    'gateway_status_phrases_u_merge_phrase_mapping': ('gateway.status_phrases', '_merge_phrase_mapping'),
    'gateway_status_phrases_u_merge_phrase_file': ('gateway.status_phrases', '_merge_phrase_file'),
    'gateway_status_phrases_u_relative_path_under': ('gateway.status_phrases', '_relative_path_under'),
    'gateway_status_phrases_u_iter_phrase_files': ('gateway.status_phrases', '_iter_phrase_files'),
    'gateway_status_phrases_u_merge_phrase_paths': ('gateway.status_phrases', '_merge_phrase_paths'),
    'gateway_status_phrases_u_load_builtin_catalog': ('gateway.status_phrases', '_load_builtin_catalog'),
    'gateway_status_phrases_u_copy_default_catalog': ('gateway.status_phrases', '_copy_default_catalog'),
    'gateway_status_phrases_u_merge_phrase_config': ('gateway.status_phrases', '_merge_phrase_config'),
    'gateway_status_phrases_resolve_status_phrase_catalog': ('gateway.status_phrases', 'resolve_status_phrase_catalog'),
    'gateway_status_phrases_classify_status_context': ('gateway.status_phrases', 'classify_status_context'),
    'gateway_status_phrases_choose_status_phrase': ('gateway.status_phrases', 'choose_status_phrase'),
    'gateway_platforms_qqbot_chunke_file_size_human': ('gateway.platforms.qqbot.chunked_upload', 'file_size_human'),
    'gateway_platforms_qqbot_chunke_file_size_human_2': ('gateway.platforms.qqbot.chunked_upload', 'file_size_human'),
    'gateway_platforms_qqbot_chunke_limit_human': ('gateway.platforms.qqbot.chunked_upload', 'limit_human'),
    'gateway_platforms_qqbot_chunke_u_parse_prepare_response': ('gateway.platforms.qqbot.chunked_upload', '_parse_prepare_response'),
    'gateway_platforms_qqbot_chunke_u_prepare': ('gateway.platforms.qqbot.chunked_upload', '_prepare'),
    'gateway_platforms_qqbot_chunke_u_upload_one_part': ('gateway.platforms.qqbot.chunked_upload', '_upload_one_part'),
    'gateway_platforms_qqbot_chunke_u_put_to_presigned_url': ('gateway.platforms.qqbot.chunked_upload', '_put_to_presigned_url'),
    'gateway_platforms_qqbot_chunke_u_part_finish_with_retry': ('gateway.platforms.qqbot.chunked_upload', '_part_finish_with_retry'),
    'gateway_platforms_qqbot_chunke_u_read_file_chunk': ('gateway.platforms.qqbot.chunked_upload', '_read_file_chunk'),
    'gateway_platforms_qqbot_chunke_u_compute_file_hashes': ('gateway.platforms.qqbot.chunked_upload', '_compute_file_hashes'),
    'gateway_platforms_qqbot_chunke_u_run_with_concurrency': ('gateway.platforms.qqbot.chunked_upload', '_run_with_concurrency'),
    'gateway_relay_ws_transport_u_render_relay_context': ('gateway.relay.ws_transport', '_render_relay_context'),
    'gateway_relay_ws_transport_u_normalize_slack_parent_command': ('gateway.relay.ws_transport', '_normalize_slack_parent_command'),
    'gateway_relay_ws_transport_u_passthrough_from_wire': ('gateway.relay.ws_transport', '_passthrough_from_wire'),
    'gateway_relay_ws_transport_u_dial_and_start': ('gateway.relay.ws_transport', '_dial_and_start'),
    'gateway_relay_ws_transport_auth_revoked': ('gateway.relay.ws_transport', 'auth_revoked'),
    'gateway_relay_ws_transport_u_bot_id_for': ('gateway.relay.ws_transport', '_bot_id_for'),
    'gateway_relay_ws_transport_go_dormant': ('gateway.relay.ws_transport', 'go_dormant'),
    'gateway_relay_ws_transport_u_send_inbound_ack': ('gateway.relay.ws_transport', '_send_inbound_ack'),
    'gateway_relay_ws_transport_u_close_code_of': ('gateway.relay.ws_transport', '_close_code_of'),
    'gateway_relay_ws_transport_u_reconnect_loop': ('gateway.relay.ws_transport', '_reconnect_loop'),
    'gateway_config_u_env_multiplex_profiles_override': ('gateway.config', '_env_multiplex_profiles_override'),
    'gateway_config_u_normalize_transport_token': ('gateway.config', '_normalize_transport_token'),
    'gateway_config_coerce_systemd_watchdog_seconds': ('gateway.config', 'coerce_systemd_watchdog_seconds'),
    'gateway_config_u_coerce_dict': ('gateway.config', '_coerce_dict'),
    'gateway_config_u_getenv_str': ('gateway.config', '_getenv_str'),
    'gateway_config_u_getenv_int': ('gateway.config', '_getenv_int'),
    'gateway_config_platform_binds_port': ('gateway.config', 'platform_binds_port'),
    'gateway_config_persist_home_channel': ('gateway.config', 'persist_home_channel'),
    'gateway_config_u_has_usable_api_server_key': ('gateway.config', '_has_usable_api_server_key'),
    'gateway_kanban_watchers_u_resolve_auto_decompose_settings': ('gateway.kanban_watchers', '_resolve_auto_decompose_settings'),
    'gateway_kanban_watchers_u_acquire_singleton_lock': ('gateway.kanban_watchers', '_acquire_singleton_lock'),
    'gateway_kanban_watchers_u_release_singleton_lock': ('gateway.kanban_watchers', '_release_singleton_lock'),
    'gateway_kanban_watchers_u_kanban_notifier_watcher': ('gateway.kanban_watchers', '_kanban_notifier_watcher'),
    'gateway_kanban_watchers_u_kanban_advance': ('gateway.kanban_watchers', '_kanban_advance'),
    'gateway_kanban_watchers_u_kanban_unsub': ('gateway.kanban_watchers', '_kanban_unsub'),
    'gateway_kanban_watchers_u_kanban_rewind': ('gateway.kanban_watchers', '_kanban_rewind'),
    'gateway_kanban_watchers_u_deliver_kanban_artifacts': ('gateway.kanban_watchers', '_deliver_kanban_artifacts'),
    'gateway_kanban_watchers_u_kanban_dispatcher_watcher': ('gateway.kanban_watchers', '_kanban_dispatcher_watcher'),
    'gateway_relay_adapter_u_start_revocation_monitor': ('gateway.relay.adapter', '_start_revocation_monitor'),
    'gateway_relay_adapter_u_watch_for_revocation': ('gateway.relay.adapter', '_watch_for_revocation'),
    'gateway_relay_adapter_fronts_platform': ('gateway.relay.adapter', 'fronts_platform'),
    'gateway_relay_adapter_u_platform_is_fronted': ('gateway.relay.adapter', '_platform_is_fronted'),
    'gateway_relay_adapter_u_on_passthrough': ('gateway.relay.adapter', '_on_passthrough'),
    'gateway_relay_adapter_u_discord_interaction_to_event': ('gateway.relay.adapter', '_discord_interaction_to_event'),
    'gateway_relay_adapter_u_render_interaction_options': ('gateway.relay.adapter', '_render_interaction_options'),
    'gateway_relay_adapter_go_dormant': ('gateway.relay.adapter', 'go_dormant'),
    'gateway_relay_adapter_send_for_platform': ('gateway.relay.adapter', 'send_for_platform'),
    'gateway_platforms_webhook_filt_u_stringify_filter_value': ('gateway.platforms.webhook_filters', '_stringify_filter_value'),
    'gateway_platforms_webhook_filt_u_resolve_profile_path': ('gateway.platforms.webhook_filters', '_resolve_profile_path'),
    'gateway_platforms_webhook_filt_u_resolve_script_path': ('gateway.platforms.webhook_filters', '_resolve_script_path'),
    'gateway_platforms_webhook_filt_u_load_filter_file_values': ('gateway.platforms.webhook_filters', '_load_filter_file_values'),
    'gateway_platforms_webhook_filt_resolve_filter_field': ('gateway.platforms.webhook_filters', 'resolve_filter_field'),
    'gateway_platforms_webhook_filt_filter_matches': ('gateway.platforms.webhook_filters', 'filter_matches'),
    'gateway_platforms_webhook_filt_route_filters_match': ('gateway.platforms.webhook_filters', 'route_filters_match'),
    'gateway_platforms_webhook_filt_run_route_script': ('gateway.platforms.webhook_filters', 'run_route_script'),
    'gateway_platform_registry_register_deferred': ('gateway.platform_registry', 'register_deferred'),
    'gateway_platform_registry_u_resolve_all': ('gateway.platform_registry', '_resolve_all'),
    'gateway_platform_registry_all_entries': ('gateway.platform_registry', 'all_entries'),
    'gateway_platform_registry_plugin_entries': ('gateway.platform_registry', 'plugin_entries'),
    'gateway_platform_registry_is_registered': ('gateway.platform_registry', 'is_registered'),
    'gateway_platform_registry_create_adapter': ('gateway.platform_registry', 'create_adapter'),
    'gateway_authz_mixin_u_auth_env': ('gateway.authz_mixin', '_auth_env'),
    'gateway_authz_mixin_u_coerce_allow_set': ('gateway.authz_mixin', '_coerce_allow_set'),
    'gateway_authz_mixin_u_registered_transport_adapter': ('gateway.authz_mixin', '_registered_transport_adapter'),
    'gateway_authz_mixin_u_adapter_profile_for_source': ('gateway.authz_mixin', '_adapter_profile_for_source'),
    'gateway_authz_mixin_u_pairing_store_for': ('gateway.authz_mixin', '_pairing_store_for'),
    'gateway_readiness_u_probe_state_db': ('gateway.readiness', '_probe_state_db'),
    'gateway_readiness_u_probe_config': ('gateway.readiness', '_probe_config'),
    'gateway_readiness_u_probe_disk': ('gateway.readiness', '_probe_disk'),
    'gateway_readiness_u_probe_gateway': ('gateway.readiness', '_probe_gateway'),
    'gateway_readiness_collect_runtime_readiness': ('gateway.readiness', 'collect_runtime_readiness'),
    'gateway_systemd_notify_u_notify_address': ('gateway.systemd_notify', '_notify_address'),
    'gateway_systemd_notify_watchdog_interval_seconds': ('gateway.systemd_notify', 'watchdog_interval_seconds'),
    'gateway_systemd_notify_unhealthy': ('gateway.systemd_notify', 'unhealthy'),
    'gateway_systemd_notify_u_lag_tolerance': ('gateway.systemd_notify', '_lag_tolerance'),
    'gateway_systemd_notify_record_tick': ('gateway.systemd_notify', 'record_tick'),
    'gateway_channel_directory_u_warn_slack_directory': ('gateway.channel_directory', '_warn_slack_directory'),
    'gateway_channel_directory_u_slack_api_error_code': ('gateway.channel_directory', '_slack_api_error_code'),
    'gateway_channel_directory_u_build_from_sessions_db': ('gateway.channel_directory', '_build_from_sessions_db'),
    'gateway_channel_directory_u_build_from_sessions_json': ('gateway.channel_directory', '_build_from_sessions_json'),
    'gateway_turn_lease_u__repr__': ('gateway.turn_lease', '__repr__'),
    'gateway_turn_lease_u__len__': ('gateway.turn_lease', '__len__'),
    'gateway_turn_lease_u_evict_idle': ('gateway.turn_lease', '_evict_idle'),
    'gateway_turn_lease_rebind': ('gateway.turn_lease', 'rebind'),
    'gateway_profile_routing_specificity': ('gateway.profile_routing', 'specificity'),
    'gateway_profile_routing_parse_profile_routes': ('gateway.profile_routing', 'parse_profile_routes'),
    'gateway_profile_routing_match_profile_route': ('gateway.profile_routing', 'match_profile_route'),
    'gateway_wake_adapter_supports_push': ('gateway.wake', 'adapter_supports_push'),
    'gateway_wake_deliver_wake': ('gateway.wake', 'deliver_wake'),
    'gateway_wake_u_self_post_chat_completion': ('gateway.wake', '_self_post_chat_completion'),
    'gateway_cwd_placeholder_u_truthy_env': ('gateway.cwd_placeholder', '_truthy_env'),
    'gateway_cwd_placeholder_resolve_placeholder_terminal_cwd': ('gateway.cwd_placeholder', 'resolve_placeholder_terminal_cwd'),
    'gateway_delivery_is_relay': ('gateway.delivery', 'is_relay'),
    'gateway_delivery_resolve_delivery_transport': ('gateway.delivery', 'resolve_delivery_transport'),
    'gateway_response_filters_u_strip_edge_silence_punctuation': ('gateway.response_filters', '_strip_edge_silence_punctuation'),
    'gateway_response_filters_u_canonical_silence_candidates': ('gateway.response_filters', '_canonical_silence_candidates'),
    'gateway_platforms_helpers_discard': ('gateway.platforms.helpers', 'discard'),
    'gateway_restart_is_gateway_supervisor_process': ('gateway.restart', 'is_gateway_supervisor_process'),
    'gateway_session_context_declare_stateless_channel': ('gateway.session_context', 'declare_stateless_channel'),
}

def main():
    if len(sys.argv) < 2: sys.stderr.write("usage: sta_oracle_port_gateway_remaining_wrappers.py <cases.json>\n"); return 2
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
