/* AUTO-GENERATED integration oracle harness for port_gateway_remaining_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_gateway_remaining_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int gateway_platforms_signal_u_render_mentions(const char *);
extern int gateway_platforms_signal_validate_signal_config(const char *);
extern int gateway_platforms_signal_u_sse_listener(const char *);
extern int gateway_platforms_signal_u_health_monitor(const char *);
extern int gateway_platforms_signal_u_force_reconnect(const char *);
extern int gateway_platforms_signal_u_handle_envelope(const char *);
extern int gateway_platforms_signal_u_remember_recipient_identifiers(const char *);
extern int gateway_platforms_signal_u_extract_contact_uuid(const char *);
extern int gateway_platforms_signal_u_resolve_recipient(const char *);
extern int gateway_platforms_signal_u_fetch_attachment(const char *);
extern int gateway_platforms_signal_u_rpc(const char *);
extern int gateway_platforms_signal_u_track_sent_timestamp(const char *);
extern int gateway_platforms_signal_u_notify_batch_pacing(const char *);
extern int gateway_platforms_signal_u_stop_typing_indicator(const char *);
extern int gateway_platforms_signal_remove_reaction(const char *);
extern int gateway_platforms_signal_u_extract_reaction_target(const char *);
extern int gateway_platforms_signal_u_reactions_enabled(const char *);
extern int gateway_delivery_ledger_u_db_path(const char *);
extern int gateway_delivery_ledger_u_connect(const char *);
extern int gateway_delivery_ledger_u_initialize_schema(const char *);
extern int gateway_delivery_ledger_u_transaction(const char *);
extern int gateway_delivery_ledger_u_owner_stamp(const char *);
extern int gateway_delivery_ledger_u_owner_alive(const char *);
extern int gateway_delivery_ledger_compute_obligation_id(const char *);
extern int gateway_delivery_ledger_record_obligation(const char *);
extern int gateway_delivery_ledger_mark_attempting(const char *);
extern int gateway_delivery_ledger_mark_delivered(const char *);
extern int gateway_delivery_ledger_mark_failed(const char *);
extern int gateway_delivery_ledger_u_update_state(const char *);
extern int gateway_delivery_ledger_sweep_recoverable(const char *);
extern int gateway_delivery_ledger_ledger_enabled(const char *);
extern int gateway_delivery_ledger_debug_rows(const char *);
extern int gateway_shutdown_watchdog_u_schedule(const char *);
extern int gateway_shutdown_watchdog_u_tick(const char *);
extern int gateway_shutdown_watchdog_cancel(const char *);
extern int gateway_shutdown_watchdog_is_alive(const char *);
extern int gateway_shutdown_watchdog_u_arm_loop_floor_timer(const char *);
extern int gateway_shutdown_watchdog_start_loop_liveness_watchdog(const char *);
extern int gateway_shutdown_watchdog_u_process_hermes_home(const char *);
extern int gateway_shutdown_watchdog_get_loop_heartbeat_path(const char *);
extern int gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path(const char *);
extern int gateway_shutdown_watchdog_write_loop_heartbeat(const char *);
extern int gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(const char *);
extern int gateway_shutdown_watchdog_u_write_watchdog_dump(const char *);
extern int gateway_shutdown_watchdog_arm_shutdown_watchdog(const char *);
extern int gateway_shutdown_watchdog_loop_heartbeat_forever(const char *);
extern int gateway_status_phrases_u_clean_phrase_list(const char *);
extern int gateway_status_phrases_u_merge_phrase_mapping(const char *);
extern int gateway_status_phrases_u_merge_phrase_file(const char *);
extern int gateway_status_phrases_u_relative_path_under(const char *);
extern int gateway_status_phrases_u_iter_phrase_files(const char *);
extern int gateway_status_phrases_u_merge_phrase_paths(const char *);
extern int gateway_status_phrases_u_load_builtin_catalog(const char *);
extern int gateway_status_phrases_u_copy_default_catalog(const char *);
extern int gateway_status_phrases_u_merge_phrase_config(const char *);
extern int gateway_status_phrases_resolve_status_phrase_catalog(const char *);
extern int gateway_status_phrases_classify_status_context(const char *);
extern int gateway_status_phrases_choose_status_phrase(const char *);
extern int gateway_platforms_qqbot_chunke_file_size_human(const char *);
extern int gateway_platforms_qqbot_chunke_file_size_human_2(const char *);
extern int gateway_platforms_qqbot_chunke_limit_human(const char *);
extern int gateway_platforms_qqbot_chunke_u_parse_prepare_response(const char *);
extern int gateway_platforms_qqbot_chunke_u_prepare(const char *);
extern int gateway_platforms_qqbot_chunke_u_upload_one_part(const char *);
extern int gateway_platforms_qqbot_chunke_u_put_to_presigned_url(const char *);
extern int gateway_platforms_qqbot_chunke_u_part_finish_with_retry(const char *);
extern int gateway_platforms_qqbot_chunke_u_read_file_chunk(const char *);
extern int gateway_platforms_qqbot_chunke_u_compute_file_hashes(const char *);
extern int gateway_platforms_qqbot_chunke_u_run_with_concurrency(const char *);
extern int gateway_relay_ws_transport_u_render_relay_context(const char *);
extern int gateway_relay_ws_transport_u_normalize_slack_parent_command(const char *);
extern int gateway_relay_ws_transport_u_passthrough_from_wire(const char *);
extern int gateway_relay_ws_transport_u_dial_and_start(const char *);
extern int gateway_relay_ws_transport_auth_revoked(const char *);
extern int gateway_relay_ws_transport_u_bot_id_for(const char *);
extern int gateway_relay_ws_transport_go_dormant(const char *);
extern int gateway_relay_ws_transport_u_send_inbound_ack(const char *);
extern int gateway_relay_ws_transport_u_close_code_of(const char *);
extern int gateway_relay_ws_transport_u_reconnect_loop(const char *);
extern int gateway_config_u_env_multiplex_profiles_override(const char *);
extern int gateway_config_u_normalize_transport_token(const char *);
extern int gateway_config_coerce_systemd_watchdog_seconds(const char *);
extern int gateway_config_u_coerce_dict(const char *);
extern int gateway_config_u_getenv_str(const char *);
extern int gateway_config_u_getenv_int(const char *);
extern int gateway_config_platform_binds_port(const char *);
extern int gateway_config_persist_home_channel(const char *);
extern int gateway_config_u_has_usable_api_server_key(const char *);
extern int gateway_kanban_watchers_u_resolve_auto_decompose_settings(const char *);
extern int gateway_kanban_watchers_u_acquire_singleton_lock(const char *);
extern int gateway_kanban_watchers_u_release_singleton_lock(const char *);
extern int gateway_kanban_watchers_u_kanban_notifier_watcher(const char *);
extern int gateway_kanban_watchers_u_kanban_advance(const char *);
extern int gateway_kanban_watchers_u_kanban_unsub(const char *);
extern int gateway_kanban_watchers_u_kanban_rewind(const char *);
extern int gateway_kanban_watchers_u_deliver_kanban_artifacts(const char *);
extern int gateway_kanban_watchers_u_kanban_dispatcher_watcher(const char *);
extern int gateway_relay_adapter_u_start_revocation_monitor(const char *);
extern int gateway_relay_adapter_u_watch_for_revocation(const char *);
extern int gateway_relay_adapter_fronts_platform(const char *);
extern int gateway_relay_adapter_u_platform_is_fronted(const char *);
extern int gateway_relay_adapter_u_on_passthrough(const char *);
extern int gateway_relay_adapter_u_discord_interaction_to_event(const char *);
extern int gateway_relay_adapter_u_render_interaction_options(const char *);
extern int gateway_relay_adapter_go_dormant(const char *);
extern int gateway_relay_adapter_send_for_platform(const char *);
extern int gateway_platforms_webhook_filt_u_stringify_filter_value(const char *);
extern int gateway_platforms_webhook_filt_u_resolve_profile_path(const char *);
extern int gateway_platforms_webhook_filt_u_resolve_script_path(const char *);
extern int gateway_platforms_webhook_filt_u_load_filter_file_values(const char *);
extern int gateway_platforms_webhook_filt_resolve_filter_field(const char *);
extern int gateway_platforms_webhook_filt_filter_matches(const char *);
extern int gateway_platforms_webhook_filt_route_filters_match(const char *);
extern int gateway_platforms_webhook_filt_run_route_script(const char *);
extern int gateway_platform_registry_register_deferred(const char *);
extern int gateway_platform_registry_u_resolve_all(const char *);
extern int gateway_platform_registry_all_entries(const char *);
extern int gateway_platform_registry_plugin_entries(const char *);
extern int gateway_platform_registry_is_registered(const char *);
extern int gateway_platform_registry_create_adapter(const char *);
extern int gateway_authz_mixin_u_auth_env(const char *);
extern int gateway_authz_mixin_u_coerce_allow_set(const char *);
extern int gateway_authz_mixin_u_registered_transport_adapter(const char *);
extern int gateway_authz_mixin_u_adapter_profile_for_source(const char *);
extern int gateway_authz_mixin_u_pairing_store_for(const char *);
extern int gateway_readiness_u_probe_state_db(const char *);
extern int gateway_readiness_u_probe_config(const char *);
extern int gateway_readiness_u_probe_disk(const char *);
extern int gateway_readiness_u_probe_gateway(const char *);
extern int gateway_readiness_collect_runtime_readiness(const char *);
extern int gateway_systemd_notify_u_notify_address(const char *);
extern int gateway_systemd_notify_watchdog_interval_seconds(const char *);
extern int gateway_systemd_notify_unhealthy(const char *);
extern int gateway_systemd_notify_u_lag_tolerance(const char *);
extern int gateway_systemd_notify_record_tick(const char *);
extern int gateway_channel_directory_u_warn_slack_directory(const char *);
extern int gateway_channel_directory_u_slack_api_error_code(const char *);
extern int gateway_channel_directory_u_build_from_sessions_db(const char *);
extern int gateway_channel_directory_u_build_from_sessions_json(const char *);
extern int gateway_turn_lease_u__repr__(const char *);
extern int gateway_turn_lease_u__len__(const char *);
extern int gateway_turn_lease_u_evict_idle(const char *);
extern int gateway_turn_lease_rebind(const char *);
extern int gateway_profile_routing_specificity(const char *);
extern int gateway_profile_routing_parse_profile_routes(const char *);
extern int gateway_profile_routing_match_profile_route(const char *);
extern int gateway_wake_adapter_supports_push(const char *);
extern int gateway_wake_deliver_wake(const char *);
extern int gateway_wake_u_self_post_chat_completion(const char *);
extern int gateway_cwd_placeholder_u_truthy_env(const char *);
extern int gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(const char *);
extern int gateway_delivery_is_relay(const char *);
extern int gateway_delivery_resolve_delivery_transport(const char *);
extern int gateway_response_filters_u_strip_edge_silence_punctuation(const char *);
extern int gateway_response_filters_u_canonical_silence_candidates(const char *);
extern int gateway_platforms_helpers_discard(const char *);
extern int gateway_restart_is_gateway_supervisor_process(const char *);
extern int gateway_session_context_declare_stateless_channel(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_gateway_platforms_signal_u_render_mentions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_render_mentions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_render_mentions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_validate_signal_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_validate_signal_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_validate_signal_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_sse_listener(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_sse_listener(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_sse_listener"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_health_monitor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_health_monitor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_health_monitor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_force_reconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_force_reconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_force_reconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_handle_envelope(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_handle_envelope(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_handle_envelope"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_remember_recipient_identifiers(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_remember_recipient_identifiers(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_remember_recipient_identifiers"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_extract_contact_uuid(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_extract_contact_uuid(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_extract_contact_uuid"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_resolve_recipient(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_resolve_recipient(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_resolve_recipient"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_fetch_attachment(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_fetch_attachment(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_fetch_attachment"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_rpc(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_rpc(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_rpc"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_track_sent_timestamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_track_sent_timestamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_track_sent_timestamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_notify_batch_pacing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_notify_batch_pacing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_notify_batch_pacing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_stop_typing_indicator(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_stop_typing_indicator(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_stop_typing_indicator"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_remove_reaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_remove_reaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_remove_reaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_extract_reaction_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_extract_reaction_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_extract_reaction_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_signal_u_reactions_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_signal_u_reactions_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_signal_u_reactions_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_db_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_db_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_db_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_connect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_connect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_connect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_initialize_schema(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_initialize_schema(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_initialize_schema"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_transaction(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_transaction(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_transaction"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_owner_stamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_owner_stamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_owner_stamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_owner_alive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_owner_alive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_owner_alive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_compute_obligation_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_compute_obligation_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_compute_obligation_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_record_obligation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_record_obligation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_record_obligation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_mark_attempting(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_mark_attempting(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_mark_attempting"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_mark_delivered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_mark_delivered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_mark_delivered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_mark_failed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_mark_failed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_mark_failed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_u_update_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_u_update_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_u_update_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_sweep_recoverable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_sweep_recoverable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_sweep_recoverable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_ledger_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_ledger_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_ledger_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_ledger_debug_rows(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_ledger_debug_rows(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_ledger_debug_rows"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_u_schedule(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_u_schedule(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_u_schedule"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_u_tick(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_u_tick(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_u_tick"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_cancel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_cancel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_cancel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_is_alive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_is_alive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_is_alive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_u_arm_loop_floor_timer(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_u_arm_loop_floor_timer(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_u_arm_loop_floor_timer"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_start_loop_liveness_watchdog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_start_loop_liveness_watchdog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_start_loop_liveness_watchdog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_u_process_hermes_home(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_u_process_hermes_home(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_u_process_hermes_home"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_get_loop_heartbeat_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_get_loop_heartbeat_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_get_loop_heartbeat_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_write_loop_heartbeat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_write_loop_heartbeat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_write_loop_heartbeat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_u_write_watchdog_dump(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_u_write_watchdog_dump(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_u_write_watchdog_dump"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_arm_shutdown_watchdog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_arm_shutdown_watchdog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_arm_shutdown_watchdog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_shutdown_watchdog_loop_heartbeat_forever(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_shutdown_watchdog_loop_heartbeat_forever(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_shutdown_watchdog_loop_heartbeat_forever"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_clean_phrase_list(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_clean_phrase_list(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_clean_phrase_list"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_merge_phrase_mapping(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_merge_phrase_mapping(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_merge_phrase_mapping"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_merge_phrase_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_merge_phrase_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_merge_phrase_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_relative_path_under(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_relative_path_under(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_relative_path_under"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_iter_phrase_files(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_iter_phrase_files(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_iter_phrase_files"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_merge_phrase_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_merge_phrase_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_merge_phrase_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_load_builtin_catalog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_load_builtin_catalog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_load_builtin_catalog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_copy_default_catalog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_copy_default_catalog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_copy_default_catalog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_u_merge_phrase_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_u_merge_phrase_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_u_merge_phrase_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_resolve_status_phrase_catalog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_resolve_status_phrase_catalog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_resolve_status_phrase_catalog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_classify_status_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_classify_status_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_classify_status_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_status_phrases_choose_status_phrase(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_status_phrases_choose_status_phrase(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_status_phrases_choose_status_phrase"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_file_size_human(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_file_size_human(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_file_size_human"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_file_size_human_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_file_size_human_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_file_size_human_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_limit_human(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_limit_human(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_limit_human"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_parse_prepare_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_parse_prepare_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_parse_prepare_response"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_prepare(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_prepare(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_prepare"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_upload_one_part(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_upload_one_part(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_upload_one_part"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_put_to_presigned_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_put_to_presigned_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_put_to_presigned_url"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_part_finish_with_retry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_part_finish_with_retry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_part_finish_with_retry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_read_file_chunk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_read_file_chunk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_read_file_chunk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_compute_file_hashes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_compute_file_hashes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_compute_file_hashes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_qqbot_chunke_u_run_with_concurrency(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_qqbot_chunke_u_run_with_concurrency(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_qqbot_chunke_u_run_with_concurrency"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_render_relay_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_render_relay_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_render_relay_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_normalize_slack_parent_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_normalize_slack_parent_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_normalize_slack_parent_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_passthrough_from_wire(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_passthrough_from_wire(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_passthrough_from_wire"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_dial_and_start(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_dial_and_start(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_dial_and_start"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_auth_revoked(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_auth_revoked(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_auth_revoked"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_bot_id_for(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_bot_id_for(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_bot_id_for"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_go_dormant(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_go_dormant(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_go_dormant"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_send_inbound_ack(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_send_inbound_ack(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_send_inbound_ack"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_close_code_of(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_close_code_of(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_close_code_of"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_ws_transport_u_reconnect_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_ws_transport_u_reconnect_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_ws_transport_u_reconnect_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_u_env_multiplex_profiles_override(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_u_env_multiplex_profiles_override(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_u_env_multiplex_profiles_override"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_u_normalize_transport_token(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_u_normalize_transport_token(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_u_normalize_transport_token"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_coerce_systemd_watchdog_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_coerce_systemd_watchdog_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_coerce_systemd_watchdog_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_u_coerce_dict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_u_coerce_dict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_u_coerce_dict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_u_getenv_str(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_u_getenv_str(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_u_getenv_str"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_u_getenv_int(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_u_getenv_int(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_u_getenv_int"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_platform_binds_port(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_platform_binds_port(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_platform_binds_port"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_persist_home_channel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_persist_home_channel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_persist_home_channel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_config_u_has_usable_api_server_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_config_u_has_usable_api_server_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_config_u_has_usable_api_server_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_resolve_auto_decompose_settings(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_resolve_auto_decompose_settings(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_resolve_auto_decompose_settings"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_acquire_singleton_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_acquire_singleton_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_acquire_singleton_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_release_singleton_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_release_singleton_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_release_singleton_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_kanban_notifier_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_kanban_notifier_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_kanban_notifier_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_kanban_advance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_kanban_advance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_kanban_advance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_kanban_unsub(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_kanban_unsub(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_kanban_unsub"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_kanban_rewind(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_kanban_rewind(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_kanban_rewind"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_deliver_kanban_artifacts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_deliver_kanban_artifacts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_deliver_kanban_artifacts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_kanban_watchers_u_kanban_dispatcher_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_kanban_watchers_u_kanban_dispatcher_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_kanban_watchers_u_kanban_dispatcher_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_u_start_revocation_monitor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_u_start_revocation_monitor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_u_start_revocation_monitor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_u_watch_for_revocation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_u_watch_for_revocation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_u_watch_for_revocation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_fronts_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_fronts_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_fronts_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_u_platform_is_fronted(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_u_platform_is_fronted(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_u_platform_is_fronted"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_u_on_passthrough(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_u_on_passthrough(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_u_on_passthrough"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_u_discord_interaction_to_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_u_discord_interaction_to_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_u_discord_interaction_to_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_u_render_interaction_options(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_u_render_interaction_options(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_u_render_interaction_options"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_go_dormant(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_go_dormant(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_go_dormant"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_relay_adapter_send_for_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_relay_adapter_send_for_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_relay_adapter_send_for_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_u_stringify_filter_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_u_stringify_filter_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_u_stringify_filter_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_u_resolve_profile_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_u_resolve_profile_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_u_resolve_profile_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_u_resolve_script_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_u_resolve_script_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_u_resolve_script_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_u_load_filter_file_values(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_u_load_filter_file_values(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_u_load_filter_file_values"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_resolve_filter_field(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_resolve_filter_field(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_resolve_filter_field"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_filter_matches(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_filter_matches(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_filter_matches"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_route_filters_match(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_route_filters_match(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_route_filters_match"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_webhook_filt_run_route_script(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_webhook_filt_run_route_script(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_webhook_filt_run_route_script"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platform_registry_register_deferred(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platform_registry_register_deferred(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platform_registry_register_deferred"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platform_registry_u_resolve_all(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platform_registry_u_resolve_all(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platform_registry_u_resolve_all"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platform_registry_all_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platform_registry_all_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platform_registry_all_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platform_registry_plugin_entries(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platform_registry_plugin_entries(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platform_registry_plugin_entries"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platform_registry_is_registered(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platform_registry_is_registered(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platform_registry_is_registered"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platform_registry_create_adapter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platform_registry_create_adapter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platform_registry_create_adapter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_authz_mixin_u_auth_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_authz_mixin_u_auth_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_authz_mixin_u_auth_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_authz_mixin_u_coerce_allow_set(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_authz_mixin_u_coerce_allow_set(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_authz_mixin_u_coerce_allow_set"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_authz_mixin_u_registered_transport_adapter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_authz_mixin_u_registered_transport_adapter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_authz_mixin_u_registered_transport_adapter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_authz_mixin_u_adapter_profile_for_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_authz_mixin_u_adapter_profile_for_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_authz_mixin_u_adapter_profile_for_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_authz_mixin_u_pairing_store_for(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_authz_mixin_u_pairing_store_for(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_authz_mixin_u_pairing_store_for"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_readiness_u_probe_state_db(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_readiness_u_probe_state_db(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_readiness_u_probe_state_db"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_readiness_u_probe_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_readiness_u_probe_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_readiness_u_probe_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_readiness_u_probe_disk(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_readiness_u_probe_disk(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_readiness_u_probe_disk"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_readiness_u_probe_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_readiness_u_probe_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_readiness_u_probe_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_readiness_collect_runtime_readiness(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_readiness_collect_runtime_readiness(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_readiness_collect_runtime_readiness"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_systemd_notify_u_notify_address(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_systemd_notify_u_notify_address(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_systemd_notify_u_notify_address"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_systemd_notify_watchdog_interval_seconds(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_systemd_notify_watchdog_interval_seconds(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_systemd_notify_watchdog_interval_seconds"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_systemd_notify_unhealthy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_systemd_notify_unhealthy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_systemd_notify_unhealthy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_systemd_notify_u_lag_tolerance(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_systemd_notify_u_lag_tolerance(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_systemd_notify_u_lag_tolerance"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_systemd_notify_record_tick(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_systemd_notify_record_tick(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_systemd_notify_record_tick"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_channel_directory_u_warn_slack_directory(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_channel_directory_u_warn_slack_directory(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_channel_directory_u_warn_slack_directory"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_channel_directory_u_slack_api_error_code(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_channel_directory_u_slack_api_error_code(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_channel_directory_u_slack_api_error_code"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_channel_directory_u_build_from_sessions_db(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_channel_directory_u_build_from_sessions_db(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_channel_directory_u_build_from_sessions_db"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_channel_directory_u_build_from_sessions_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_channel_directory_u_build_from_sessions_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_channel_directory_u_build_from_sessions_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_turn_lease_u__repr__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_turn_lease_u__repr__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_turn_lease_u__repr__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_turn_lease_u__len__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_turn_lease_u__len__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_turn_lease_u__len__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_turn_lease_u_evict_idle(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_turn_lease_u_evict_idle(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_turn_lease_u_evict_idle"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_turn_lease_rebind(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_turn_lease_rebind(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_turn_lease_rebind"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_profile_routing_specificity(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_profile_routing_specificity(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_profile_routing_specificity"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_profile_routing_parse_profile_routes(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_profile_routing_parse_profile_routes(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_profile_routing_parse_profile_routes"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_profile_routing_match_profile_route(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_profile_routing_match_profile_route(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_profile_routing_match_profile_route"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_wake_adapter_supports_push(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_wake_adapter_supports_push(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_wake_adapter_supports_push"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_wake_deliver_wake(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_wake_deliver_wake(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_wake_deliver_wake"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_wake_u_self_post_chat_completion(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_wake_u_self_post_chat_completion(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_wake_u_self_post_chat_completion"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_cwd_placeholder_u_truthy_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_cwd_placeholder_u_truthy_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_cwd_placeholder_u_truthy_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_cwd_placeholder_resolve_placeholder_terminal_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_is_relay(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_is_relay(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_is_relay"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_delivery_resolve_delivery_transport(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_delivery_resolve_delivery_transport(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_delivery_resolve_delivery_transport"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_response_filters_u_strip_edge_silence_punctuation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_response_filters_u_strip_edge_silence_punctuation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_response_filters_u_strip_edge_silence_punctuation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_response_filters_u_canonical_silence_candidates(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_response_filters_u_canonical_silence_candidates(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_response_filters_u_canonical_silence_candidates"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_platforms_helpers_discard(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_platforms_helpers_discard(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_platforms_helpers_discard"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_restart_is_gateway_supervisor_process(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_restart_is_gateway_supervisor_process(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_restart_is_gateway_supervisor_process"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_gateway_session_context_declare_stateless_channel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)gateway_session_context_declare_stateless_channel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("gateway_session_context_declare_stateless_channel"));
    json_set(o, "out", json_int(v)); return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "gateway_platforms_signal_u_render_mentions") == 0) o = emit_gateway_platforms_signal_u_render_mentions(c);
        if (strcmp(op, "gateway_platforms_signal_validate_signal_config") == 0) o = emit_gateway_platforms_signal_validate_signal_config(c);
        if (strcmp(op, "gateway_platforms_signal_u_sse_listener") == 0) o = emit_gateway_platforms_signal_u_sse_listener(c);
        if (strcmp(op, "gateway_platforms_signal_u_health_monitor") == 0) o = emit_gateway_platforms_signal_u_health_monitor(c);
        if (strcmp(op, "gateway_platforms_signal_u_force_reconnect") == 0) o = emit_gateway_platforms_signal_u_force_reconnect(c);
        if (strcmp(op, "gateway_platforms_signal_u_handle_envelope") == 0) o = emit_gateway_platforms_signal_u_handle_envelope(c);
        if (strcmp(op, "gateway_platforms_signal_u_remember_recipient_identifiers") == 0) o = emit_gateway_platforms_signal_u_remember_recipient_identifiers(c);
        if (strcmp(op, "gateway_platforms_signal_u_extract_contact_uuid") == 0) o = emit_gateway_platforms_signal_u_extract_contact_uuid(c);
        if (strcmp(op, "gateway_platforms_signal_u_resolve_recipient") == 0) o = emit_gateway_platforms_signal_u_resolve_recipient(c);
        if (strcmp(op, "gateway_platforms_signal_u_fetch_attachment") == 0) o = emit_gateway_platforms_signal_u_fetch_attachment(c);
        if (strcmp(op, "gateway_platforms_signal_u_rpc") == 0) o = emit_gateway_platforms_signal_u_rpc(c);
        if (strcmp(op, "gateway_platforms_signal_u_track_sent_timestamp") == 0) o = emit_gateway_platforms_signal_u_track_sent_timestamp(c);
        if (strcmp(op, "gateway_platforms_signal_u_notify_batch_pacing") == 0) o = emit_gateway_platforms_signal_u_notify_batch_pacing(c);
        if (strcmp(op, "gateway_platforms_signal_u_stop_typing_indicator") == 0) o = emit_gateway_platforms_signal_u_stop_typing_indicator(c);
        if (strcmp(op, "gateway_platforms_signal_remove_reaction") == 0) o = emit_gateway_platforms_signal_remove_reaction(c);
        if (strcmp(op, "gateway_platforms_signal_u_extract_reaction_target") == 0) o = emit_gateway_platforms_signal_u_extract_reaction_target(c);
        if (strcmp(op, "gateway_platforms_signal_u_reactions_enabled") == 0) o = emit_gateway_platforms_signal_u_reactions_enabled(c);
        if (strcmp(op, "gateway_delivery_ledger_u_db_path") == 0) o = emit_gateway_delivery_ledger_u_db_path(c);
        if (strcmp(op, "gateway_delivery_ledger_u_connect") == 0) o = emit_gateway_delivery_ledger_u_connect(c);
        if (strcmp(op, "gateway_delivery_ledger_u_initialize_schema") == 0) o = emit_gateway_delivery_ledger_u_initialize_schema(c);
        if (strcmp(op, "gateway_delivery_ledger_u_transaction") == 0) o = emit_gateway_delivery_ledger_u_transaction(c);
        if (strcmp(op, "gateway_delivery_ledger_u_owner_stamp") == 0) o = emit_gateway_delivery_ledger_u_owner_stamp(c);
        if (strcmp(op, "gateway_delivery_ledger_u_owner_alive") == 0) o = emit_gateway_delivery_ledger_u_owner_alive(c);
        if (strcmp(op, "gateway_delivery_ledger_compute_obligation_id") == 0) o = emit_gateway_delivery_ledger_compute_obligation_id(c);
        if (strcmp(op, "gateway_delivery_ledger_record_obligation") == 0) o = emit_gateway_delivery_ledger_record_obligation(c);
        if (strcmp(op, "gateway_delivery_ledger_mark_attempting") == 0) o = emit_gateway_delivery_ledger_mark_attempting(c);
        if (strcmp(op, "gateway_delivery_ledger_mark_delivered") == 0) o = emit_gateway_delivery_ledger_mark_delivered(c);
        if (strcmp(op, "gateway_delivery_ledger_mark_failed") == 0) o = emit_gateway_delivery_ledger_mark_failed(c);
        if (strcmp(op, "gateway_delivery_ledger_u_update_state") == 0) o = emit_gateway_delivery_ledger_u_update_state(c);
        if (strcmp(op, "gateway_delivery_ledger_sweep_recoverable") == 0) o = emit_gateway_delivery_ledger_sweep_recoverable(c);
        if (strcmp(op, "gateway_delivery_ledger_ledger_enabled") == 0) o = emit_gateway_delivery_ledger_ledger_enabled(c);
        if (strcmp(op, "gateway_delivery_ledger_debug_rows") == 0) o = emit_gateway_delivery_ledger_debug_rows(c);
        if (strcmp(op, "gateway_shutdown_watchdog_u_schedule") == 0) o = emit_gateway_shutdown_watchdog_u_schedule(c);
        if (strcmp(op, "gateway_shutdown_watchdog_u_tick") == 0) o = emit_gateway_shutdown_watchdog_u_tick(c);
        if (strcmp(op, "gateway_shutdown_watchdog_cancel") == 0) o = emit_gateway_shutdown_watchdog_cancel(c);
        if (strcmp(op, "gateway_shutdown_watchdog_is_alive") == 0) o = emit_gateway_shutdown_watchdog_is_alive(c);
        if (strcmp(op, "gateway_shutdown_watchdog_u_arm_loop_floor_timer") == 0) o = emit_gateway_shutdown_watchdog_u_arm_loop_floor_timer(c);
        if (strcmp(op, "gateway_shutdown_watchdog_start_loop_liveness_watchdog") == 0) o = emit_gateway_shutdown_watchdog_start_loop_liveness_watchdog(c);
        if (strcmp(op, "gateway_shutdown_watchdog_u_process_hermes_home") == 0) o = emit_gateway_shutdown_watchdog_u_process_hermes_home(c);
        if (strcmp(op, "gateway_shutdown_watchdog_get_loop_heartbeat_path") == 0) o = emit_gateway_shutdown_watchdog_get_loop_heartbeat_path(c);
        if (strcmp(op, "gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path") == 0) o = emit_gateway_shutdown_watchdog_get_shutdown_watchdog_dump_path(c);
        if (strcmp(op, "gateway_shutdown_watchdog_write_loop_heartbeat") == 0) o = emit_gateway_shutdown_watchdog_write_loop_heartbeat(c);
        if (strcmp(op, "gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay") == 0) o = emit_gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(c);
        if (strcmp(op, "gateway_shutdown_watchdog_u_write_watchdog_dump") == 0) o = emit_gateway_shutdown_watchdog_u_write_watchdog_dump(c);
        if (strcmp(op, "gateway_shutdown_watchdog_arm_shutdown_watchdog") == 0) o = emit_gateway_shutdown_watchdog_arm_shutdown_watchdog(c);
        if (strcmp(op, "gateway_shutdown_watchdog_loop_heartbeat_forever") == 0) o = emit_gateway_shutdown_watchdog_loop_heartbeat_forever(c);
        if (strcmp(op, "gateway_status_phrases_u_clean_phrase_list") == 0) o = emit_gateway_status_phrases_u_clean_phrase_list(c);
        if (strcmp(op, "gateway_status_phrases_u_merge_phrase_mapping") == 0) o = emit_gateway_status_phrases_u_merge_phrase_mapping(c);
        if (strcmp(op, "gateway_status_phrases_u_merge_phrase_file") == 0) o = emit_gateway_status_phrases_u_merge_phrase_file(c);
        if (strcmp(op, "gateway_status_phrases_u_relative_path_under") == 0) o = emit_gateway_status_phrases_u_relative_path_under(c);
        if (strcmp(op, "gateway_status_phrases_u_iter_phrase_files") == 0) o = emit_gateway_status_phrases_u_iter_phrase_files(c);
        if (strcmp(op, "gateway_status_phrases_u_merge_phrase_paths") == 0) o = emit_gateway_status_phrases_u_merge_phrase_paths(c);
        if (strcmp(op, "gateway_status_phrases_u_load_builtin_catalog") == 0) o = emit_gateway_status_phrases_u_load_builtin_catalog(c);
        if (strcmp(op, "gateway_status_phrases_u_copy_default_catalog") == 0) o = emit_gateway_status_phrases_u_copy_default_catalog(c);
        if (strcmp(op, "gateway_status_phrases_u_merge_phrase_config") == 0) o = emit_gateway_status_phrases_u_merge_phrase_config(c);
        if (strcmp(op, "gateway_status_phrases_resolve_status_phrase_catalog") == 0) o = emit_gateway_status_phrases_resolve_status_phrase_catalog(c);
        if (strcmp(op, "gateway_status_phrases_classify_status_context") == 0) o = emit_gateway_status_phrases_classify_status_context(c);
        if (strcmp(op, "gateway_status_phrases_choose_status_phrase") == 0) o = emit_gateway_status_phrases_choose_status_phrase(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_file_size_human") == 0) o = emit_gateway_platforms_qqbot_chunke_file_size_human(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_file_size_human_2") == 0) o = emit_gateway_platforms_qqbot_chunke_file_size_human_2(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_limit_human") == 0) o = emit_gateway_platforms_qqbot_chunke_limit_human(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_parse_prepare_response") == 0) o = emit_gateway_platforms_qqbot_chunke_u_parse_prepare_response(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_prepare") == 0) o = emit_gateway_platforms_qqbot_chunke_u_prepare(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_upload_one_part") == 0) o = emit_gateway_platforms_qqbot_chunke_u_upload_one_part(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_put_to_presigned_url") == 0) o = emit_gateway_platforms_qqbot_chunke_u_put_to_presigned_url(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_part_finish_with_retry") == 0) o = emit_gateway_platforms_qqbot_chunke_u_part_finish_with_retry(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_read_file_chunk") == 0) o = emit_gateway_platforms_qqbot_chunke_u_read_file_chunk(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_compute_file_hashes") == 0) o = emit_gateway_platforms_qqbot_chunke_u_compute_file_hashes(c);
        if (strcmp(op, "gateway_platforms_qqbot_chunke_u_run_with_concurrency") == 0) o = emit_gateway_platforms_qqbot_chunke_u_run_with_concurrency(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_render_relay_context") == 0) o = emit_gateway_relay_ws_transport_u_render_relay_context(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_normalize_slack_parent_command") == 0) o = emit_gateway_relay_ws_transport_u_normalize_slack_parent_command(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_passthrough_from_wire") == 0) o = emit_gateway_relay_ws_transport_u_passthrough_from_wire(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_dial_and_start") == 0) o = emit_gateway_relay_ws_transport_u_dial_and_start(c);
        if (strcmp(op, "gateway_relay_ws_transport_auth_revoked") == 0) o = emit_gateway_relay_ws_transport_auth_revoked(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_bot_id_for") == 0) o = emit_gateway_relay_ws_transport_u_bot_id_for(c);
        if (strcmp(op, "gateway_relay_ws_transport_go_dormant") == 0) o = emit_gateway_relay_ws_transport_go_dormant(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_send_inbound_ack") == 0) o = emit_gateway_relay_ws_transport_u_send_inbound_ack(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_close_code_of") == 0) o = emit_gateway_relay_ws_transport_u_close_code_of(c);
        if (strcmp(op, "gateway_relay_ws_transport_u_reconnect_loop") == 0) o = emit_gateway_relay_ws_transport_u_reconnect_loop(c);
        if (strcmp(op, "gateway_config_u_env_multiplex_profiles_override") == 0) o = emit_gateway_config_u_env_multiplex_profiles_override(c);
        if (strcmp(op, "gateway_config_u_normalize_transport_token") == 0) o = emit_gateway_config_u_normalize_transport_token(c);
        if (strcmp(op, "gateway_config_coerce_systemd_watchdog_seconds") == 0) o = emit_gateway_config_coerce_systemd_watchdog_seconds(c);
        if (strcmp(op, "gateway_config_u_coerce_dict") == 0) o = emit_gateway_config_u_coerce_dict(c);
        if (strcmp(op, "gateway_config_u_getenv_str") == 0) o = emit_gateway_config_u_getenv_str(c);
        if (strcmp(op, "gateway_config_u_getenv_int") == 0) o = emit_gateway_config_u_getenv_int(c);
        if (strcmp(op, "gateway_config_platform_binds_port") == 0) o = emit_gateway_config_platform_binds_port(c);
        if (strcmp(op, "gateway_config_persist_home_channel") == 0) o = emit_gateway_config_persist_home_channel(c);
        if (strcmp(op, "gateway_config_u_has_usable_api_server_key") == 0) o = emit_gateway_config_u_has_usable_api_server_key(c);
        if (strcmp(op, "gateway_kanban_watchers_u_resolve_auto_decompose_settings") == 0) o = emit_gateway_kanban_watchers_u_resolve_auto_decompose_settings(c);
        if (strcmp(op, "gateway_kanban_watchers_u_acquire_singleton_lock") == 0) o = emit_gateway_kanban_watchers_u_acquire_singleton_lock(c);
        if (strcmp(op, "gateway_kanban_watchers_u_release_singleton_lock") == 0) o = emit_gateway_kanban_watchers_u_release_singleton_lock(c);
        if (strcmp(op, "gateway_kanban_watchers_u_kanban_notifier_watcher") == 0) o = emit_gateway_kanban_watchers_u_kanban_notifier_watcher(c);
        if (strcmp(op, "gateway_kanban_watchers_u_kanban_advance") == 0) o = emit_gateway_kanban_watchers_u_kanban_advance(c);
        if (strcmp(op, "gateway_kanban_watchers_u_kanban_unsub") == 0) o = emit_gateway_kanban_watchers_u_kanban_unsub(c);
        if (strcmp(op, "gateway_kanban_watchers_u_kanban_rewind") == 0) o = emit_gateway_kanban_watchers_u_kanban_rewind(c);
        if (strcmp(op, "gateway_kanban_watchers_u_deliver_kanban_artifacts") == 0) o = emit_gateway_kanban_watchers_u_deliver_kanban_artifacts(c);
        if (strcmp(op, "gateway_kanban_watchers_u_kanban_dispatcher_watcher") == 0) o = emit_gateway_kanban_watchers_u_kanban_dispatcher_watcher(c);
        if (strcmp(op, "gateway_relay_adapter_u_start_revocation_monitor") == 0) o = emit_gateway_relay_adapter_u_start_revocation_monitor(c);
        if (strcmp(op, "gateway_relay_adapter_u_watch_for_revocation") == 0) o = emit_gateway_relay_adapter_u_watch_for_revocation(c);
        if (strcmp(op, "gateway_relay_adapter_fronts_platform") == 0) o = emit_gateway_relay_adapter_fronts_platform(c);
        if (strcmp(op, "gateway_relay_adapter_u_platform_is_fronted") == 0) o = emit_gateway_relay_adapter_u_platform_is_fronted(c);
        if (strcmp(op, "gateway_relay_adapter_u_on_passthrough") == 0) o = emit_gateway_relay_adapter_u_on_passthrough(c);
        if (strcmp(op, "gateway_relay_adapter_u_discord_interaction_to_event") == 0) o = emit_gateway_relay_adapter_u_discord_interaction_to_event(c);
        if (strcmp(op, "gateway_relay_adapter_u_render_interaction_options") == 0) o = emit_gateway_relay_adapter_u_render_interaction_options(c);
        if (strcmp(op, "gateway_relay_adapter_go_dormant") == 0) o = emit_gateway_relay_adapter_go_dormant(c);
        if (strcmp(op, "gateway_relay_adapter_send_for_platform") == 0) o = emit_gateway_relay_adapter_send_for_platform(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_u_stringify_filter_value") == 0) o = emit_gateway_platforms_webhook_filt_u_stringify_filter_value(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_u_resolve_profile_path") == 0) o = emit_gateway_platforms_webhook_filt_u_resolve_profile_path(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_u_resolve_script_path") == 0) o = emit_gateway_platforms_webhook_filt_u_resolve_script_path(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_u_load_filter_file_values") == 0) o = emit_gateway_platforms_webhook_filt_u_load_filter_file_values(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_resolve_filter_field") == 0) o = emit_gateway_platforms_webhook_filt_resolve_filter_field(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_filter_matches") == 0) o = emit_gateway_platforms_webhook_filt_filter_matches(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_route_filters_match") == 0) o = emit_gateway_platforms_webhook_filt_route_filters_match(c);
        if (strcmp(op, "gateway_platforms_webhook_filt_run_route_script") == 0) o = emit_gateway_platforms_webhook_filt_run_route_script(c);
        if (strcmp(op, "gateway_platform_registry_register_deferred") == 0) o = emit_gateway_platform_registry_register_deferred(c);
        if (strcmp(op, "gateway_platform_registry_u_resolve_all") == 0) o = emit_gateway_platform_registry_u_resolve_all(c);
        if (strcmp(op, "gateway_platform_registry_all_entries") == 0) o = emit_gateway_platform_registry_all_entries(c);
        if (strcmp(op, "gateway_platform_registry_plugin_entries") == 0) o = emit_gateway_platform_registry_plugin_entries(c);
        if (strcmp(op, "gateway_platform_registry_is_registered") == 0) o = emit_gateway_platform_registry_is_registered(c);
        if (strcmp(op, "gateway_platform_registry_create_adapter") == 0) o = emit_gateway_platform_registry_create_adapter(c);
        if (strcmp(op, "gateway_authz_mixin_u_auth_env") == 0) o = emit_gateway_authz_mixin_u_auth_env(c);
        if (strcmp(op, "gateway_authz_mixin_u_coerce_allow_set") == 0) o = emit_gateway_authz_mixin_u_coerce_allow_set(c);
        if (strcmp(op, "gateway_authz_mixin_u_registered_transport_adapter") == 0) o = emit_gateway_authz_mixin_u_registered_transport_adapter(c);
        if (strcmp(op, "gateway_authz_mixin_u_adapter_profile_for_source") == 0) o = emit_gateway_authz_mixin_u_adapter_profile_for_source(c);
        if (strcmp(op, "gateway_authz_mixin_u_pairing_store_for") == 0) o = emit_gateway_authz_mixin_u_pairing_store_for(c);
        if (strcmp(op, "gateway_readiness_u_probe_state_db") == 0) o = emit_gateway_readiness_u_probe_state_db(c);
        if (strcmp(op, "gateway_readiness_u_probe_config") == 0) o = emit_gateway_readiness_u_probe_config(c);
        if (strcmp(op, "gateway_readiness_u_probe_disk") == 0) o = emit_gateway_readiness_u_probe_disk(c);
        if (strcmp(op, "gateway_readiness_u_probe_gateway") == 0) o = emit_gateway_readiness_u_probe_gateway(c);
        if (strcmp(op, "gateway_readiness_collect_runtime_readiness") == 0) o = emit_gateway_readiness_collect_runtime_readiness(c);
        if (strcmp(op, "gateway_systemd_notify_u_notify_address") == 0) o = emit_gateway_systemd_notify_u_notify_address(c);
        if (strcmp(op, "gateway_systemd_notify_watchdog_interval_seconds") == 0) o = emit_gateway_systemd_notify_watchdog_interval_seconds(c);
        if (strcmp(op, "gateway_systemd_notify_unhealthy") == 0) o = emit_gateway_systemd_notify_unhealthy(c);
        if (strcmp(op, "gateway_systemd_notify_u_lag_tolerance") == 0) o = emit_gateway_systemd_notify_u_lag_tolerance(c);
        if (strcmp(op, "gateway_systemd_notify_record_tick") == 0) o = emit_gateway_systemd_notify_record_tick(c);
        if (strcmp(op, "gateway_channel_directory_u_warn_slack_directory") == 0) o = emit_gateway_channel_directory_u_warn_slack_directory(c);
        if (strcmp(op, "gateway_channel_directory_u_slack_api_error_code") == 0) o = emit_gateway_channel_directory_u_slack_api_error_code(c);
        if (strcmp(op, "gateway_channel_directory_u_build_from_sessions_db") == 0) o = emit_gateway_channel_directory_u_build_from_sessions_db(c);
        if (strcmp(op, "gateway_channel_directory_u_build_from_sessions_json") == 0) o = emit_gateway_channel_directory_u_build_from_sessions_json(c);
        if (strcmp(op, "gateway_turn_lease_u__repr__") == 0) o = emit_gateway_turn_lease_u__repr__(c);
        if (strcmp(op, "gateway_turn_lease_u__len__") == 0) o = emit_gateway_turn_lease_u__len__(c);
        if (strcmp(op, "gateway_turn_lease_u_evict_idle") == 0) o = emit_gateway_turn_lease_u_evict_idle(c);
        if (strcmp(op, "gateway_turn_lease_rebind") == 0) o = emit_gateway_turn_lease_rebind(c);
        if (strcmp(op, "gateway_profile_routing_specificity") == 0) o = emit_gateway_profile_routing_specificity(c);
        if (strcmp(op, "gateway_profile_routing_parse_profile_routes") == 0) o = emit_gateway_profile_routing_parse_profile_routes(c);
        if (strcmp(op, "gateway_profile_routing_match_profile_route") == 0) o = emit_gateway_profile_routing_match_profile_route(c);
        if (strcmp(op, "gateway_wake_adapter_supports_push") == 0) o = emit_gateway_wake_adapter_supports_push(c);
        if (strcmp(op, "gateway_wake_deliver_wake") == 0) o = emit_gateway_wake_deliver_wake(c);
        if (strcmp(op, "gateway_wake_u_self_post_chat_completion") == 0) o = emit_gateway_wake_u_self_post_chat_completion(c);
        if (strcmp(op, "gateway_cwd_placeholder_u_truthy_env") == 0) o = emit_gateway_cwd_placeholder_u_truthy_env(c);
        if (strcmp(op, "gateway_cwd_placeholder_resolve_placeholder_terminal_cwd") == 0) o = emit_gateway_cwd_placeholder_resolve_placeholder_terminal_cwd(c);
        if (strcmp(op, "gateway_delivery_is_relay") == 0) o = emit_gateway_delivery_is_relay(c);
        if (strcmp(op, "gateway_delivery_resolve_delivery_transport") == 0) o = emit_gateway_delivery_resolve_delivery_transport(c);
        if (strcmp(op, "gateway_response_filters_u_strip_edge_silence_punctuation") == 0) o = emit_gateway_response_filters_u_strip_edge_silence_punctuation(c);
        if (strcmp(op, "gateway_response_filters_u_canonical_silence_candidates") == 0) o = emit_gateway_response_filters_u_canonical_silence_candidates(c);
        if (strcmp(op, "gateway_platforms_helpers_discard") == 0) o = emit_gateway_platforms_helpers_discard(c);
        if (strcmp(op, "gateway_restart_is_gateway_supervisor_process") == 0) o = emit_gateway_restart_is_gateway_supervisor_process(c);
        if (strcmp(op, "gateway_session_context_declare_stateless_channel") == 0) o = emit_gateway_session_context_declare_stateless_channel(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
