/* AUTO-GENERATED integration oracle harness for port_gateway_run_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_gateway_run_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int grun_u_send_or_update_status_coro(const char *);
extern int grun_u_resolve_runtime_agent_kwargs(const char *);
extern int grun_u_resolve_runtime_agent_kwargs_for_provider(const char *);
extern int grun_u_try_resolve_fallback_provider(const char *);
extern int grun_u_probe_audio_duration(const char *);
extern int grun_u_dequeue_pending_event(const char *);
extern int grun_u_get_channel_override(const char *);
extern int grun_u_drain_gateway_watch_events(const char *);
extern int grun_u_dispose_unused_adapter(const char *);
extern int grun_u_wire_teams_pipeline_runtime(const char *);
extern int grun_u_warn_if_docker_media_delivery_is_risky(const char *);
extern int grun_u_set_adapter_auto_tts_disabled(const char *);
extern int grun_u_set_adapter_auto_tts_enabled(const char *);
extern int grun_u_sync_voice_mode_state_to_adapter(const char *);
extern int grun_u_await_adapter_cleanup_with_timeout(const char *);
extern int grun_u_safe_adapter_disconnect(const char *);
extern int grun_u_bounded_adapter_teardown(const char *);
extern int grun_u_connect_initial_adapter_with_timeout(const char *);
extern int grun_u_telegram_topic_mode_enabled(const char *);
extern int grun_u_should_send_telegram_lobby_reminder(const char *);
extern int grun_u_record_telegram_topic_binding(const char *);
extern int grun_u_sync_telegram_topic_binding(const char *);
extern int grun_u_recover_telegram_topic_thread_id(const char *);
extern int grun_u_normalize_source_for_session_key(const char *);
extern int grun_u_resolve_session_agent_runtime(const char *);
extern int grun_u_resolve_turn_agent_config(const char *);
extern int grun_u_sync_session_model_from_agent(const char *);
extern int grun_u_handle_reaction_event(const char *);
extern int grun_u_handle_adapter_fatal_error(const char *);
extern int grun_u_handle_adapter_fatal_error_detached(const char *);
extern int grun_u_handle_adapter_fatal_error_impl(const char *);
extern int grun_u_restart_loop_guard_config(const char *);
extern int grun_u_log_scale_to_zero_not_armed_reason(const char *);
extern int grun_u_scale_to_zero_note_real_inbound(const char *);
extern int grun_u_relay_adapter_for_dormancy(const char *);
extern int grun_u_scale_to_zero_watcher(const char *);
extern int grun_u_enqueue_fifo(const char *);
extern int grun_u_promote_queued_event(const char *);
extern int grun_u_clear_goal_pending_continuations(const char *);
extern int grun_u_persist_active_agents(const char *);
extern int grun_u_enter_external_drain(const char *);
extern int grun_u_exit_external_drain(const char *);
extern int grun_u_drain_control_watcher(const char *);
extern int grun_u_pause_failed_platform(const char *);
extern int grun_u_resume_paused_platform(const char *);
extern int grun_u_resolve_model_for_channel(const char *);
extern int grun_u_get_system_prompt_for_channel(const char *);
extern int grun_u_refresh_fallback_model(const char *);
extern int grun_u_apply_fallback_chain_to_agent(const char *);
extern int grun_u_snapshot_running_agents(const char *);
extern int grun_u_claim_active_session_slot(const char *);
extern int grun_u_agent_has_active_subagents(const char *);
extern int grun_u_session_has_compression_in_flight(const char *);
extern int grun_u_lookup_session_id_under_store_lock(const char *);
extern int grun_u_queue_or_replace_pending_event(const char *);
extern int grun_u_handle_active_session_busy_message(const char *);
extern int grun_u_notify_active_sessions_of_shutdown(const char *);
extern int grun_u_finalize_shutdown_agents(const char *);
extern int grun_u_should_emit_long_running_notification(const char *);
extern int grun_u_defer_agent_cleanup_until_future_done(const char *);
extern int grun_u_cleanup_agent_resources_off_loop(const char *);
extern int grun_u_cleanup_agent_resources(const char *);
extern int grun_u_increment_restart_failure_counts(const char *);
extern int grun_u_suspend_stuck_loop_sessions(const char *);
extern int grun_u_launch_detached_restart_command(const char *);
extern int grun_u_run_startup_resume_event(const char *);
extern int grun_u_queue_startup_restore_event(const char *);
extern int grun_u_drain_startup_restore_queue(const char *);
extern int grun_u_finish_startup_restore(const char *);
extern int grun_u_redeliver_pending_obligations(const char *);
extern int grun_u_schedule_resume_pending_sessions(const char *);
extern int grun_u_abort_startup_if_shutdown_requested(const char *);
extern int grun_u_start_loop_liveness_guards(const char *);
extern int grun_u_stop_loop_liveness_guards(const char *);
extern int grun_u_spawn_supervised(const char *);
extern int grun_u_handoff_watcher(const char *);
extern int grun_u_process_handoff(const char *);
extern int grun_u_session_expiry_watcher(const char *);
extern int grun_u_ensure_reconnect_watcher_running(const char *);
extern int grun_u_platform_reconnect_watcher(const char *);
extern int grun_u_cancel_secondary_profile_reconnect_tasks(const char *);
extern int grun_u_start_systemd_watchdog(const char *);
extern int grun_u_stop_systemd_watchdog(const char *);
extern int grun_u_start_secondary_profile_adapters(const char *);
extern int grun_u_start_one_profile_adapters(const char *);
extern int grun_u_configure_profile_adapter(const char *);
extern int grun_u_run_secondary_profile_reconnect(const char *);
extern int grun_u_schedule_secondary_profile_reconnect(const char *);
extern int grun_u_make_profile_fatal_error_handler(const char *);
extern int grun_u_handle_profile_adapter_fatal_error(const char *);
extern int grun_u_make_profile_message_handler(const char *);
extern int grun_u_create_adapter(const char *);
extern int grun_u_make_adapter_auth_check(const char *);
extern int grun_u_deliver_platform_notice(const char *);
extern int grun_u_resolve_async_delegation_session(const char *);
extern int grun_u_prepare_inbound_message_text(const char *);
extern int grun_u_prepare_profile_scoped_inbound_message_text(const char *);
extern int grun_async_session_store(const char *);
extern int grun_u_handle_message_with_agent(const char *);
extern int grun_u_reset_notice_session_info(const char *);
extern int grun_u_format_session_info(const char *);
extern int grun_u_sibling_thread_run_keys(const char *);
extern int grun_u_is_stale_restart_redelivery(const char *);
extern int grun_u_handle_suggestions_command(const char *);
extern int grun_u_handle_blueprint_command(const char *);
extern int grun_u_get_goal_manager_for_event(const char *);
extern int grun_u_send_goal_status_notice(const char *);
extern int grun_u_defer_goal_status_notice_after_delivery(const char *);
extern int grun_u_post_turn_goal_continuation(const char *);
extern int grun_u_handle_voice_channel_join(const char *);
extern int grun_u_handle_voice_channel_leave(const char *);
extern int grun_u_handle_voice_timeout_cleanup(const char *);
extern int grun_u_handle_voice_channel_input(const char *);
extern int grun_u_should_send_voice_reply(const char *);
extern int grun_u_send_voice_reply(const char *);
extern int grun_u_deliver_media_from_response(const char *);
extern int grun_u_run_background_task(const char *);
extern int grun_u_run_background_task_inner(const char *);
extern int grun_u_get_telegram_topic_capabilities(const char *);
extern int grun_u_ensure_telegram_system_topic(const char *);
extern int grun_u_send_telegram_topic_setup_image(const char *);
extern int grun_u_rename_discord_auto_thread_for_session_title(const char *);
extern int grun_u_schedule_discord_semantic_thread_rename(const char *);
extern int grun_u_rename_telegram_topic_for_session_title(const char *);
extern int grun_u_schedule_telegram_topic_title_rename(const char *);
extern int grun_u_disable_telegram_topic_mode_for_chat(const char *);
extern int grun_u_telegram_topic_root_status_message(const char *);
extern int grun_u_restore_telegram_topic_session(const char *);
extern int grun_u_execute_mcp_reload(const char *);
extern int grun_u_maybe_confirm_destructive_slash(const char *);
extern int grun_u_request_slash_confirm(const char *);
extern int grun_u_read_user_config(const char *);
extern int grun_u_schedule_update_notification_watch(const char *);
extern int grun_u_watch_update_progress(const char *);
extern int grun_u_send_update_notification(const char *);
extern int grun_u_send_restart_notification(const char *);
extern int grun_u_send_home_channel_startup_notifications(const char *);
extern int grun_u_set_session_env(const char *);
extern int grun_u_clear_session_env(const char *);
extern int grun_u_run_in_executor_with_context(const char *);
extern int grun_u_get_executor(const char *);
extern int grun_u_shutdown_executor(const char *);
extern int grun_u_enrich_message_with_vision(const char *);
extern int grun_u_enrich_message_with_transcription(const char *);
extern int grun_u_pending_event_audio_paths(const char *);
extern int grun_u_transcribe_pending_audio_event_once(const char *);
extern int grun_u_echo_pending_stt_transcripts_once(const char *);
extern int grun_u_transcribe_and_echo_pending_voice(const char *);
extern int grun_u_build_process_event_source(const char *);
extern int grun_u_inject_watch_notification(const char *);
extern int grun_u_classify_completion_target(const char *);
extern int grun_u_deliver_completion_notification(const char *);
extern int grun_u_enrich_async_delegation_routing(const char *);
extern int grun_u_async_delegation_watcher(const char *);
extern int grun_u_run_process_watcher(const char *);
extern int grun_u_extract_honcho_cache_busting_config(const char *);
extern int grun_u_extract_cache_busting_config(const char *);
extern int grun_u_agent_config_signature(const char *);
extern int grun_u_rehydrate_session_model_override(const char *);
extern int grun_u_release_running_agent_state(const char *);
extern int grun_u_release_turn_lease(const char *);
extern int grun_u_rebind_turn_lease(const char *);
extern int grun_u_clear_conversation_scope(const char *);
extern int grun_u_clear_session_boundary_security_state(const char *);
extern int grun_u_bind_adapter_run_generation(const char *);
extern int grun_u_interrupt_and_clear_session(const char *);
extern int grun_u_refresh_agent_cache_message_count(const char *);
extern int grun_u_voice_channel_sidecar_note(const char *);
extern int grun_u_pinned_session_context_prompt(const char *);
extern int grun_u_init_cached_agent_for_turn(const char *);
extern int grun_u_commit_memory_before_soft_evict(const char *);
extern int grun_u_commit_then_release_soft(const char *);
extern int grun_u_release_evicted_agent_soft(const char *);
extern int grun_u_enforce_agent_cache_cap(const char *);
extern int grun_u_sweep_idle_cached_agents(const char *);
extern int grun_u_run_agent_via_proxy(const char *);
extern int grun_u_profile_name_for_source(const char *);
extern int grun_u_resolve_profile_home_for_source(const char *);
extern int grun_u_run_planned_stop_watcher(const char *);
extern int grun_u_start_gateway_housekeeping(const char *);
extern int grun_u_start_cron_ticker(const char *);
extern int grun_u_await_thread_exit(const char *);
extern int grun_start_gateway(const char *);
extern int grun_u_exit_after_graceful_shutdown(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_grun_u_send_or_update_status_coro(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_or_update_status_coro(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_or_update_status_coro"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_runtime_agent_kwargs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_runtime_agent_kwargs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_runtime_agent_kwargs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_runtime_agent_kwargs_for_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_runtime_agent_kwargs_for_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_runtime_agent_kwargs_for_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_try_resolve_fallback_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_try_resolve_fallback_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_try_resolve_fallback_provider"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_probe_audio_duration(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_probe_audio_duration(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_probe_audio_duration"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_dequeue_pending_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_dequeue_pending_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_dequeue_pending_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_get_channel_override(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_get_channel_override(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_get_channel_override"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_drain_gateway_watch_events(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_drain_gateway_watch_events(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_drain_gateway_watch_events"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_dispose_unused_adapter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_dispose_unused_adapter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_dispose_unused_adapter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_wire_teams_pipeline_runtime(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_wire_teams_pipeline_runtime(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_wire_teams_pipeline_runtime"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_warn_if_docker_media_delivery_is_risky(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_warn_if_docker_media_delivery_is_risky(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_warn_if_docker_media_delivery_is_risky"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_set_adapter_auto_tts_disabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_set_adapter_auto_tts_disabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_set_adapter_auto_tts_disabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_set_adapter_auto_tts_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_set_adapter_auto_tts_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_set_adapter_auto_tts_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_sync_voice_mode_state_to_adapter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_sync_voice_mode_state_to_adapter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_sync_voice_mode_state_to_adapter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_await_adapter_cleanup_with_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_await_adapter_cleanup_with_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_await_adapter_cleanup_with_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_safe_adapter_disconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_safe_adapter_disconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_safe_adapter_disconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_bounded_adapter_teardown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_bounded_adapter_teardown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_bounded_adapter_teardown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_connect_initial_adapter_with_timeout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_connect_initial_adapter_with_timeout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_connect_initial_adapter_with_timeout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_telegram_topic_mode_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_telegram_topic_mode_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_telegram_topic_mode_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_should_send_telegram_lobby_reminder(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_should_send_telegram_lobby_reminder(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_should_send_telegram_lobby_reminder"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_record_telegram_topic_binding(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_record_telegram_topic_binding(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_record_telegram_topic_binding"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_sync_telegram_topic_binding(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_sync_telegram_topic_binding(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_sync_telegram_topic_binding"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_recover_telegram_topic_thread_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_recover_telegram_topic_thread_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_recover_telegram_topic_thread_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_normalize_source_for_session_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_normalize_source_for_session_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_normalize_source_for_session_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_session_agent_runtime(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_session_agent_runtime(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_session_agent_runtime"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_turn_agent_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_turn_agent_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_turn_agent_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_sync_session_model_from_agent(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_sync_session_model_from_agent(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_sync_session_model_from_agent"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_reaction_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_reaction_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_reaction_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_adapter_fatal_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_adapter_fatal_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_adapter_fatal_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_adapter_fatal_error_detached(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_adapter_fatal_error_detached(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_adapter_fatal_error_detached"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_adapter_fatal_error_impl(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_adapter_fatal_error_impl(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_adapter_fatal_error_impl"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_restart_loop_guard_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_restart_loop_guard_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_restart_loop_guard_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_log_scale_to_zero_not_armed_reason(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_log_scale_to_zero_not_armed_reason(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_log_scale_to_zero_not_armed_reason"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_scale_to_zero_note_real_inbound(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_scale_to_zero_note_real_inbound(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_scale_to_zero_note_real_inbound"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_relay_adapter_for_dormancy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_relay_adapter_for_dormancy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_relay_adapter_for_dormancy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_scale_to_zero_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_scale_to_zero_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_scale_to_zero_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_enqueue_fifo(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_enqueue_fifo(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_enqueue_fifo"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_promote_queued_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_promote_queued_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_promote_queued_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_clear_goal_pending_continuations(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_clear_goal_pending_continuations(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_clear_goal_pending_continuations"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_persist_active_agents(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_persist_active_agents(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_persist_active_agents"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_enter_external_drain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_enter_external_drain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_enter_external_drain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_exit_external_drain(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_exit_external_drain(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_exit_external_drain"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_drain_control_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_drain_control_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_drain_control_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_pause_failed_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_pause_failed_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_pause_failed_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resume_paused_platform(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resume_paused_platform(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resume_paused_platform"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_model_for_channel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_model_for_channel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_model_for_channel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_get_system_prompt_for_channel(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_get_system_prompt_for_channel(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_get_system_prompt_for_channel"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_refresh_fallback_model(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_refresh_fallback_model(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_refresh_fallback_model"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_apply_fallback_chain_to_agent(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_apply_fallback_chain_to_agent(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_apply_fallback_chain_to_agent"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_snapshot_running_agents(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_snapshot_running_agents(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_snapshot_running_agents"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_claim_active_session_slot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_claim_active_session_slot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_claim_active_session_slot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_agent_has_active_subagents(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_agent_has_active_subagents(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_agent_has_active_subagents"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_session_has_compression_in_flight(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_session_has_compression_in_flight(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_session_has_compression_in_flight"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_lookup_session_id_under_store_lock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_lookup_session_id_under_store_lock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_lookup_session_id_under_store_lock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_queue_or_replace_pending_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_queue_or_replace_pending_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_queue_or_replace_pending_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_active_session_busy_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_active_session_busy_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_active_session_busy_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_notify_active_sessions_of_shutdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_notify_active_sessions_of_shutdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_notify_active_sessions_of_shutdown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_finalize_shutdown_agents(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_finalize_shutdown_agents(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_finalize_shutdown_agents"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_should_emit_long_running_notification(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_should_emit_long_running_notification(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_should_emit_long_running_notification"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_defer_agent_cleanup_until_future_done(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_defer_agent_cleanup_until_future_done(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_defer_agent_cleanup_until_future_done"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_cleanup_agent_resources_off_loop(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_cleanup_agent_resources_off_loop(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_cleanup_agent_resources_off_loop"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_cleanup_agent_resources(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_cleanup_agent_resources(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_cleanup_agent_resources"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_increment_restart_failure_counts(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_increment_restart_failure_counts(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_increment_restart_failure_counts"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_suspend_stuck_loop_sessions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_suspend_stuck_loop_sessions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_suspend_stuck_loop_sessions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_launch_detached_restart_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_launch_detached_restart_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_launch_detached_restart_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_startup_resume_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_startup_resume_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_startup_resume_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_queue_startup_restore_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_queue_startup_restore_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_queue_startup_restore_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_drain_startup_restore_queue(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_drain_startup_restore_queue(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_drain_startup_restore_queue"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_finish_startup_restore(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_finish_startup_restore(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_finish_startup_restore"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_redeliver_pending_obligations(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_redeliver_pending_obligations(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_redeliver_pending_obligations"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_schedule_resume_pending_sessions(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_schedule_resume_pending_sessions(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_schedule_resume_pending_sessions"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_abort_startup_if_shutdown_requested(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_abort_startup_if_shutdown_requested(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_abort_startup_if_shutdown_requested"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_start_loop_liveness_guards(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_start_loop_liveness_guards(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_start_loop_liveness_guards"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_stop_loop_liveness_guards(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_stop_loop_liveness_guards(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_stop_loop_liveness_guards"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_spawn_supervised(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_spawn_supervised(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_spawn_supervised"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handoff_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handoff_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handoff_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_process_handoff(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_process_handoff(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_process_handoff"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_session_expiry_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_session_expiry_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_session_expiry_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_ensure_reconnect_watcher_running(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_ensure_reconnect_watcher_running(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_ensure_reconnect_watcher_running"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_platform_reconnect_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_platform_reconnect_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_platform_reconnect_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_cancel_secondary_profile_reconnect_tasks(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_cancel_secondary_profile_reconnect_tasks(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_cancel_secondary_profile_reconnect_tasks"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_start_systemd_watchdog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_start_systemd_watchdog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_start_systemd_watchdog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_stop_systemd_watchdog(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_stop_systemd_watchdog(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_stop_systemd_watchdog"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_start_secondary_profile_adapters(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_start_secondary_profile_adapters(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_start_secondary_profile_adapters"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_start_one_profile_adapters(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_start_one_profile_adapters(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_start_one_profile_adapters"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_configure_profile_adapter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_configure_profile_adapter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_configure_profile_adapter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_secondary_profile_reconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_secondary_profile_reconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_secondary_profile_reconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_schedule_secondary_profile_reconnect(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_schedule_secondary_profile_reconnect(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_schedule_secondary_profile_reconnect"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_make_profile_fatal_error_handler(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_make_profile_fatal_error_handler(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_make_profile_fatal_error_handler"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_profile_adapter_fatal_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_profile_adapter_fatal_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_profile_adapter_fatal_error"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_make_profile_message_handler(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_make_profile_message_handler(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_make_profile_message_handler"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_create_adapter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_create_adapter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_create_adapter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_make_adapter_auth_check(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_make_adapter_auth_check(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_make_adapter_auth_check"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_deliver_platform_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_deliver_platform_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_deliver_platform_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_async_delegation_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_async_delegation_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_async_delegation_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_prepare_inbound_message_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_prepare_inbound_message_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_prepare_inbound_message_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_prepare_profile_scoped_inbound_message_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_prepare_profile_scoped_inbound_message_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_prepare_profile_scoped_inbound_message_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_async_session_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_async_session_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_async_session_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_message_with_agent(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_message_with_agent(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_message_with_agent"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_reset_notice_session_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_reset_notice_session_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_reset_notice_session_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_format_session_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_format_session_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_format_session_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_sibling_thread_run_keys(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_sibling_thread_run_keys(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_sibling_thread_run_keys"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_is_stale_restart_redelivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_is_stale_restart_redelivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_is_stale_restart_redelivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_suggestions_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_suggestions_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_suggestions_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_blueprint_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_blueprint_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_blueprint_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_get_goal_manager_for_event(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_get_goal_manager_for_event(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_get_goal_manager_for_event"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_send_goal_status_notice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_goal_status_notice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_goal_status_notice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_defer_goal_status_notice_after_delivery(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_defer_goal_status_notice_after_delivery(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_defer_goal_status_notice_after_delivery"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_post_turn_goal_continuation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_post_turn_goal_continuation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_post_turn_goal_continuation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_voice_channel_join(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_voice_channel_join(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_voice_channel_join"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_voice_channel_leave(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_voice_channel_leave(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_voice_channel_leave"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_voice_timeout_cleanup(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_voice_timeout_cleanup(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_voice_timeout_cleanup"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_handle_voice_channel_input(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_handle_voice_channel_input(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_handle_voice_channel_input"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_should_send_voice_reply(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_should_send_voice_reply(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_should_send_voice_reply"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_send_voice_reply(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_voice_reply(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_voice_reply"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_deliver_media_from_response(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_deliver_media_from_response(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_deliver_media_from_response"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_background_task(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_background_task(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_background_task"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_background_task_inner(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_background_task_inner(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_background_task_inner"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_get_telegram_topic_capabilities(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_get_telegram_topic_capabilities(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_get_telegram_topic_capabilities"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_ensure_telegram_system_topic(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_ensure_telegram_system_topic(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_ensure_telegram_system_topic"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_send_telegram_topic_setup_image(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_telegram_topic_setup_image(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_telegram_topic_setup_image"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_rename_discord_auto_thread_for_session_title(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_rename_discord_auto_thread_for_session_title(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_rename_discord_auto_thread_for_session_title"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_schedule_discord_semantic_thread_rename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_schedule_discord_semantic_thread_rename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_schedule_discord_semantic_thread_rename"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_rename_telegram_topic_for_session_title(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_rename_telegram_topic_for_session_title(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_rename_telegram_topic_for_session_title"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_schedule_telegram_topic_title_rename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_schedule_telegram_topic_title_rename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_schedule_telegram_topic_title_rename"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_disable_telegram_topic_mode_for_chat(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_disable_telegram_topic_mode_for_chat(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_disable_telegram_topic_mode_for_chat"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_telegram_topic_root_status_message(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_telegram_topic_root_status_message(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_telegram_topic_root_status_message"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_restore_telegram_topic_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_restore_telegram_topic_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_restore_telegram_topic_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_execute_mcp_reload(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_execute_mcp_reload(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_execute_mcp_reload"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_maybe_confirm_destructive_slash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_maybe_confirm_destructive_slash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_maybe_confirm_destructive_slash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_request_slash_confirm(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_request_slash_confirm(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_request_slash_confirm"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_read_user_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_read_user_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_read_user_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_schedule_update_notification_watch(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_schedule_update_notification_watch(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_schedule_update_notification_watch"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_watch_update_progress(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_watch_update_progress(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_watch_update_progress"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_send_update_notification(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_update_notification(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_update_notification"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_send_restart_notification(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_restart_notification(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_restart_notification"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_send_home_channel_startup_notifications(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_send_home_channel_startup_notifications(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_send_home_channel_startup_notifications"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_set_session_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_set_session_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_set_session_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_clear_session_env(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_clear_session_env(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_clear_session_env"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_in_executor_with_context(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_in_executor_with_context(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_in_executor_with_context"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_get_executor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_get_executor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_get_executor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_shutdown_executor(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_shutdown_executor(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_shutdown_executor"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_enrich_message_with_vision(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_enrich_message_with_vision(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_enrich_message_with_vision"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_enrich_message_with_transcription(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_enrich_message_with_transcription(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_enrich_message_with_transcription"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_pending_event_audio_paths(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_pending_event_audio_paths(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_pending_event_audio_paths"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_transcribe_pending_audio_event_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_transcribe_pending_audio_event_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_transcribe_pending_audio_event_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_echo_pending_stt_transcripts_once(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_echo_pending_stt_transcripts_once(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_echo_pending_stt_transcripts_once"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_transcribe_and_echo_pending_voice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_transcribe_and_echo_pending_voice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_transcribe_and_echo_pending_voice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_build_process_event_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_build_process_event_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_build_process_event_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_inject_watch_notification(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_inject_watch_notification(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_inject_watch_notification"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_classify_completion_target(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_classify_completion_target(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_classify_completion_target"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_deliver_completion_notification(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_deliver_completion_notification(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_deliver_completion_notification"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_enrich_async_delegation_routing(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_enrich_async_delegation_routing(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_enrich_async_delegation_routing"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_async_delegation_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_async_delegation_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_async_delegation_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_process_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_process_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_process_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_extract_honcho_cache_busting_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_extract_honcho_cache_busting_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_extract_honcho_cache_busting_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_extract_cache_busting_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_extract_cache_busting_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_extract_cache_busting_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_agent_config_signature(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_agent_config_signature(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_agent_config_signature"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_rehydrate_session_model_override(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_rehydrate_session_model_override(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_rehydrate_session_model_override"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_release_running_agent_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_release_running_agent_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_release_running_agent_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_release_turn_lease(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_release_turn_lease(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_release_turn_lease"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_rebind_turn_lease(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_rebind_turn_lease(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_rebind_turn_lease"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_clear_conversation_scope(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_clear_conversation_scope(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_clear_conversation_scope"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_clear_session_boundary_security_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_clear_session_boundary_security_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_clear_session_boundary_security_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_bind_adapter_run_generation(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_bind_adapter_run_generation(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_bind_adapter_run_generation"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_interrupt_and_clear_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_interrupt_and_clear_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_interrupt_and_clear_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_refresh_agent_cache_message_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_refresh_agent_cache_message_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_refresh_agent_cache_message_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_voice_channel_sidecar_note(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_voice_channel_sidecar_note(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_voice_channel_sidecar_note"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_pinned_session_context_prompt(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_pinned_session_context_prompt(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_pinned_session_context_prompt"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_init_cached_agent_for_turn(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_init_cached_agent_for_turn(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_init_cached_agent_for_turn"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_commit_memory_before_soft_evict(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_commit_memory_before_soft_evict(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_commit_memory_before_soft_evict"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_commit_then_release_soft(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_commit_then_release_soft(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_commit_then_release_soft"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_release_evicted_agent_soft(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_release_evicted_agent_soft(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_release_evicted_agent_soft"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_enforce_agent_cache_cap(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_enforce_agent_cache_cap(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_enforce_agent_cache_cap"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_sweep_idle_cached_agents(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_sweep_idle_cached_agents(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_sweep_idle_cached_agents"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_agent_via_proxy(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_agent_via_proxy(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_agent_via_proxy"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_profile_name_for_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_profile_name_for_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_profile_name_for_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_resolve_profile_home_for_source(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_resolve_profile_home_for_source(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_resolve_profile_home_for_source"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_run_planned_stop_watcher(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_run_planned_stop_watcher(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_run_planned_stop_watcher"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_start_gateway_housekeeping(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_start_gateway_housekeeping(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_start_gateway_housekeeping"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_start_cron_ticker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_start_cron_ticker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_start_cron_ticker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_await_thread_exit(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_await_thread_exit(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_await_thread_exit"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_start_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_start_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_start_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_grun_u_exit_after_graceful_shutdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)grun_u_exit_after_graceful_shutdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("grun_u_exit_after_graceful_shutdown"));
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
        if (strcmp(op, "grun_u_send_or_update_status_coro") == 0) o = emit_grun_u_send_or_update_status_coro(c);
        if (strcmp(op, "grun_u_resolve_runtime_agent_kwargs") == 0) o = emit_grun_u_resolve_runtime_agent_kwargs(c);
        if (strcmp(op, "grun_u_resolve_runtime_agent_kwargs_for_provider") == 0) o = emit_grun_u_resolve_runtime_agent_kwargs_for_provider(c);
        if (strcmp(op, "grun_u_try_resolve_fallback_provider") == 0) o = emit_grun_u_try_resolve_fallback_provider(c);
        if (strcmp(op, "grun_u_probe_audio_duration") == 0) o = emit_grun_u_probe_audio_duration(c);
        if (strcmp(op, "grun_u_dequeue_pending_event") == 0) o = emit_grun_u_dequeue_pending_event(c);
        if (strcmp(op, "grun_u_get_channel_override") == 0) o = emit_grun_u_get_channel_override(c);
        if (strcmp(op, "grun_u_drain_gateway_watch_events") == 0) o = emit_grun_u_drain_gateway_watch_events(c);
        if (strcmp(op, "grun_u_dispose_unused_adapter") == 0) o = emit_grun_u_dispose_unused_adapter(c);
        if (strcmp(op, "grun_u_wire_teams_pipeline_runtime") == 0) o = emit_grun_u_wire_teams_pipeline_runtime(c);
        if (strcmp(op, "grun_u_warn_if_docker_media_delivery_is_risky") == 0) o = emit_grun_u_warn_if_docker_media_delivery_is_risky(c);
        if (strcmp(op, "grun_u_set_adapter_auto_tts_disabled") == 0) o = emit_grun_u_set_adapter_auto_tts_disabled(c);
        if (strcmp(op, "grun_u_set_adapter_auto_tts_enabled") == 0) o = emit_grun_u_set_adapter_auto_tts_enabled(c);
        if (strcmp(op, "grun_u_sync_voice_mode_state_to_adapter") == 0) o = emit_grun_u_sync_voice_mode_state_to_adapter(c);
        if (strcmp(op, "grun_u_await_adapter_cleanup_with_timeout") == 0) o = emit_grun_u_await_adapter_cleanup_with_timeout(c);
        if (strcmp(op, "grun_u_safe_adapter_disconnect") == 0) o = emit_grun_u_safe_adapter_disconnect(c);
        if (strcmp(op, "grun_u_bounded_adapter_teardown") == 0) o = emit_grun_u_bounded_adapter_teardown(c);
        if (strcmp(op, "grun_u_connect_initial_adapter_with_timeout") == 0) o = emit_grun_u_connect_initial_adapter_with_timeout(c);
        if (strcmp(op, "grun_u_telegram_topic_mode_enabled") == 0) o = emit_grun_u_telegram_topic_mode_enabled(c);
        if (strcmp(op, "grun_u_should_send_telegram_lobby_reminder") == 0) o = emit_grun_u_should_send_telegram_lobby_reminder(c);
        if (strcmp(op, "grun_u_record_telegram_topic_binding") == 0) o = emit_grun_u_record_telegram_topic_binding(c);
        if (strcmp(op, "grun_u_sync_telegram_topic_binding") == 0) o = emit_grun_u_sync_telegram_topic_binding(c);
        if (strcmp(op, "grun_u_recover_telegram_topic_thread_id") == 0) o = emit_grun_u_recover_telegram_topic_thread_id(c);
        if (strcmp(op, "grun_u_normalize_source_for_session_key") == 0) o = emit_grun_u_normalize_source_for_session_key(c);
        if (strcmp(op, "grun_u_resolve_session_agent_runtime") == 0) o = emit_grun_u_resolve_session_agent_runtime(c);
        if (strcmp(op, "grun_u_resolve_turn_agent_config") == 0) o = emit_grun_u_resolve_turn_agent_config(c);
        if (strcmp(op, "grun_u_sync_session_model_from_agent") == 0) o = emit_grun_u_sync_session_model_from_agent(c);
        if (strcmp(op, "grun_u_handle_reaction_event") == 0) o = emit_grun_u_handle_reaction_event(c);
        if (strcmp(op, "grun_u_handle_adapter_fatal_error") == 0) o = emit_grun_u_handle_adapter_fatal_error(c);
        if (strcmp(op, "grun_u_handle_adapter_fatal_error_detached") == 0) o = emit_grun_u_handle_adapter_fatal_error_detached(c);
        if (strcmp(op, "grun_u_handle_adapter_fatal_error_impl") == 0) o = emit_grun_u_handle_adapter_fatal_error_impl(c);
        if (strcmp(op, "grun_u_restart_loop_guard_config") == 0) o = emit_grun_u_restart_loop_guard_config(c);
        if (strcmp(op, "grun_u_log_scale_to_zero_not_armed_reason") == 0) o = emit_grun_u_log_scale_to_zero_not_armed_reason(c);
        if (strcmp(op, "grun_u_scale_to_zero_note_real_inbound") == 0) o = emit_grun_u_scale_to_zero_note_real_inbound(c);
        if (strcmp(op, "grun_u_relay_adapter_for_dormancy") == 0) o = emit_grun_u_relay_adapter_for_dormancy(c);
        if (strcmp(op, "grun_u_scale_to_zero_watcher") == 0) o = emit_grun_u_scale_to_zero_watcher(c);
        if (strcmp(op, "grun_u_enqueue_fifo") == 0) o = emit_grun_u_enqueue_fifo(c);
        if (strcmp(op, "grun_u_promote_queued_event") == 0) o = emit_grun_u_promote_queued_event(c);
        if (strcmp(op, "grun_u_clear_goal_pending_continuations") == 0) o = emit_grun_u_clear_goal_pending_continuations(c);
        if (strcmp(op, "grun_u_persist_active_agents") == 0) o = emit_grun_u_persist_active_agents(c);
        if (strcmp(op, "grun_u_enter_external_drain") == 0) o = emit_grun_u_enter_external_drain(c);
        if (strcmp(op, "grun_u_exit_external_drain") == 0) o = emit_grun_u_exit_external_drain(c);
        if (strcmp(op, "grun_u_drain_control_watcher") == 0) o = emit_grun_u_drain_control_watcher(c);
        if (strcmp(op, "grun_u_pause_failed_platform") == 0) o = emit_grun_u_pause_failed_platform(c);
        if (strcmp(op, "grun_u_resume_paused_platform") == 0) o = emit_grun_u_resume_paused_platform(c);
        if (strcmp(op, "grun_u_resolve_model_for_channel") == 0) o = emit_grun_u_resolve_model_for_channel(c);
        if (strcmp(op, "grun_u_get_system_prompt_for_channel") == 0) o = emit_grun_u_get_system_prompt_for_channel(c);
        if (strcmp(op, "grun_u_refresh_fallback_model") == 0) o = emit_grun_u_refresh_fallback_model(c);
        if (strcmp(op, "grun_u_apply_fallback_chain_to_agent") == 0) o = emit_grun_u_apply_fallback_chain_to_agent(c);
        if (strcmp(op, "grun_u_snapshot_running_agents") == 0) o = emit_grun_u_snapshot_running_agents(c);
        if (strcmp(op, "grun_u_claim_active_session_slot") == 0) o = emit_grun_u_claim_active_session_slot(c);
        if (strcmp(op, "grun_u_agent_has_active_subagents") == 0) o = emit_grun_u_agent_has_active_subagents(c);
        if (strcmp(op, "grun_u_session_has_compression_in_flight") == 0) o = emit_grun_u_session_has_compression_in_flight(c);
        if (strcmp(op, "grun_u_lookup_session_id_under_store_lock") == 0) o = emit_grun_u_lookup_session_id_under_store_lock(c);
        if (strcmp(op, "grun_u_queue_or_replace_pending_event") == 0) o = emit_grun_u_queue_or_replace_pending_event(c);
        if (strcmp(op, "grun_u_handle_active_session_busy_message") == 0) o = emit_grun_u_handle_active_session_busy_message(c);
        if (strcmp(op, "grun_u_notify_active_sessions_of_shutdown") == 0) o = emit_grun_u_notify_active_sessions_of_shutdown(c);
        if (strcmp(op, "grun_u_finalize_shutdown_agents") == 0) o = emit_grun_u_finalize_shutdown_agents(c);
        if (strcmp(op, "grun_u_should_emit_long_running_notification") == 0) o = emit_grun_u_should_emit_long_running_notification(c);
        if (strcmp(op, "grun_u_defer_agent_cleanup_until_future_done") == 0) o = emit_grun_u_defer_agent_cleanup_until_future_done(c);
        if (strcmp(op, "grun_u_cleanup_agent_resources_off_loop") == 0) o = emit_grun_u_cleanup_agent_resources_off_loop(c);
        if (strcmp(op, "grun_u_cleanup_agent_resources") == 0) o = emit_grun_u_cleanup_agent_resources(c);
        if (strcmp(op, "grun_u_increment_restart_failure_counts") == 0) o = emit_grun_u_increment_restart_failure_counts(c);
        if (strcmp(op, "grun_u_suspend_stuck_loop_sessions") == 0) o = emit_grun_u_suspend_stuck_loop_sessions(c);
        if (strcmp(op, "grun_u_launch_detached_restart_command") == 0) o = emit_grun_u_launch_detached_restart_command(c);
        if (strcmp(op, "grun_u_run_startup_resume_event") == 0) o = emit_grun_u_run_startup_resume_event(c);
        if (strcmp(op, "grun_u_queue_startup_restore_event") == 0) o = emit_grun_u_queue_startup_restore_event(c);
        if (strcmp(op, "grun_u_drain_startup_restore_queue") == 0) o = emit_grun_u_drain_startup_restore_queue(c);
        if (strcmp(op, "grun_u_finish_startup_restore") == 0) o = emit_grun_u_finish_startup_restore(c);
        if (strcmp(op, "grun_u_redeliver_pending_obligations") == 0) o = emit_grun_u_redeliver_pending_obligations(c);
        if (strcmp(op, "grun_u_schedule_resume_pending_sessions") == 0) o = emit_grun_u_schedule_resume_pending_sessions(c);
        if (strcmp(op, "grun_u_abort_startup_if_shutdown_requested") == 0) o = emit_grun_u_abort_startup_if_shutdown_requested(c);
        if (strcmp(op, "grun_u_start_loop_liveness_guards") == 0) o = emit_grun_u_start_loop_liveness_guards(c);
        if (strcmp(op, "grun_u_stop_loop_liveness_guards") == 0) o = emit_grun_u_stop_loop_liveness_guards(c);
        if (strcmp(op, "grun_u_spawn_supervised") == 0) o = emit_grun_u_spawn_supervised(c);
        if (strcmp(op, "grun_u_handoff_watcher") == 0) o = emit_grun_u_handoff_watcher(c);
        if (strcmp(op, "grun_u_process_handoff") == 0) o = emit_grun_u_process_handoff(c);
        if (strcmp(op, "grun_u_session_expiry_watcher") == 0) o = emit_grun_u_session_expiry_watcher(c);
        if (strcmp(op, "grun_u_ensure_reconnect_watcher_running") == 0) o = emit_grun_u_ensure_reconnect_watcher_running(c);
        if (strcmp(op, "grun_u_platform_reconnect_watcher") == 0) o = emit_grun_u_platform_reconnect_watcher(c);
        if (strcmp(op, "grun_u_cancel_secondary_profile_reconnect_tasks") == 0) o = emit_grun_u_cancel_secondary_profile_reconnect_tasks(c);
        if (strcmp(op, "grun_u_start_systemd_watchdog") == 0) o = emit_grun_u_start_systemd_watchdog(c);
        if (strcmp(op, "grun_u_stop_systemd_watchdog") == 0) o = emit_grun_u_stop_systemd_watchdog(c);
        if (strcmp(op, "grun_u_start_secondary_profile_adapters") == 0) o = emit_grun_u_start_secondary_profile_adapters(c);
        if (strcmp(op, "grun_u_start_one_profile_adapters") == 0) o = emit_grun_u_start_one_profile_adapters(c);
        if (strcmp(op, "grun_u_configure_profile_adapter") == 0) o = emit_grun_u_configure_profile_adapter(c);
        if (strcmp(op, "grun_u_run_secondary_profile_reconnect") == 0) o = emit_grun_u_run_secondary_profile_reconnect(c);
        if (strcmp(op, "grun_u_schedule_secondary_profile_reconnect") == 0) o = emit_grun_u_schedule_secondary_profile_reconnect(c);
        if (strcmp(op, "grun_u_make_profile_fatal_error_handler") == 0) o = emit_grun_u_make_profile_fatal_error_handler(c);
        if (strcmp(op, "grun_u_handle_profile_adapter_fatal_error") == 0) o = emit_grun_u_handle_profile_adapter_fatal_error(c);
        if (strcmp(op, "grun_u_make_profile_message_handler") == 0) o = emit_grun_u_make_profile_message_handler(c);
        if (strcmp(op, "grun_u_create_adapter") == 0) o = emit_grun_u_create_adapter(c);
        if (strcmp(op, "grun_u_make_adapter_auth_check") == 0) o = emit_grun_u_make_adapter_auth_check(c);
        if (strcmp(op, "grun_u_deliver_platform_notice") == 0) o = emit_grun_u_deliver_platform_notice(c);
        if (strcmp(op, "grun_u_resolve_async_delegation_session") == 0) o = emit_grun_u_resolve_async_delegation_session(c);
        if (strcmp(op, "grun_u_prepare_inbound_message_text") == 0) o = emit_grun_u_prepare_inbound_message_text(c);
        if (strcmp(op, "grun_u_prepare_profile_scoped_inbound_message_text") == 0) o = emit_grun_u_prepare_profile_scoped_inbound_message_text(c);
        if (strcmp(op, "grun_async_session_store") == 0) o = emit_grun_async_session_store(c);
        if (strcmp(op, "grun_u_handle_message_with_agent") == 0) o = emit_grun_u_handle_message_with_agent(c);
        if (strcmp(op, "grun_u_reset_notice_session_info") == 0) o = emit_grun_u_reset_notice_session_info(c);
        if (strcmp(op, "grun_u_format_session_info") == 0) o = emit_grun_u_format_session_info(c);
        if (strcmp(op, "grun_u_sibling_thread_run_keys") == 0) o = emit_grun_u_sibling_thread_run_keys(c);
        if (strcmp(op, "grun_u_is_stale_restart_redelivery") == 0) o = emit_grun_u_is_stale_restart_redelivery(c);
        if (strcmp(op, "grun_u_handle_suggestions_command") == 0) o = emit_grun_u_handle_suggestions_command(c);
        if (strcmp(op, "grun_u_handle_blueprint_command") == 0) o = emit_grun_u_handle_blueprint_command(c);
        if (strcmp(op, "grun_u_get_goal_manager_for_event") == 0) o = emit_grun_u_get_goal_manager_for_event(c);
        if (strcmp(op, "grun_u_send_goal_status_notice") == 0) o = emit_grun_u_send_goal_status_notice(c);
        if (strcmp(op, "grun_u_defer_goal_status_notice_after_delivery") == 0) o = emit_grun_u_defer_goal_status_notice_after_delivery(c);
        if (strcmp(op, "grun_u_post_turn_goal_continuation") == 0) o = emit_grun_u_post_turn_goal_continuation(c);
        if (strcmp(op, "grun_u_handle_voice_channel_join") == 0) o = emit_grun_u_handle_voice_channel_join(c);
        if (strcmp(op, "grun_u_handle_voice_channel_leave") == 0) o = emit_grun_u_handle_voice_channel_leave(c);
        if (strcmp(op, "grun_u_handle_voice_timeout_cleanup") == 0) o = emit_grun_u_handle_voice_timeout_cleanup(c);
        if (strcmp(op, "grun_u_handle_voice_channel_input") == 0) o = emit_grun_u_handle_voice_channel_input(c);
        if (strcmp(op, "grun_u_should_send_voice_reply") == 0) o = emit_grun_u_should_send_voice_reply(c);
        if (strcmp(op, "grun_u_send_voice_reply") == 0) o = emit_grun_u_send_voice_reply(c);
        if (strcmp(op, "grun_u_deliver_media_from_response") == 0) o = emit_grun_u_deliver_media_from_response(c);
        if (strcmp(op, "grun_u_run_background_task") == 0) o = emit_grun_u_run_background_task(c);
        if (strcmp(op, "grun_u_run_background_task_inner") == 0) o = emit_grun_u_run_background_task_inner(c);
        if (strcmp(op, "grun_u_get_telegram_topic_capabilities") == 0) o = emit_grun_u_get_telegram_topic_capabilities(c);
        if (strcmp(op, "grun_u_ensure_telegram_system_topic") == 0) o = emit_grun_u_ensure_telegram_system_topic(c);
        if (strcmp(op, "grun_u_send_telegram_topic_setup_image") == 0) o = emit_grun_u_send_telegram_topic_setup_image(c);
        if (strcmp(op, "grun_u_rename_discord_auto_thread_for_session_title") == 0) o = emit_grun_u_rename_discord_auto_thread_for_session_title(c);
        if (strcmp(op, "grun_u_schedule_discord_semantic_thread_rename") == 0) o = emit_grun_u_schedule_discord_semantic_thread_rename(c);
        if (strcmp(op, "grun_u_rename_telegram_topic_for_session_title") == 0) o = emit_grun_u_rename_telegram_topic_for_session_title(c);
        if (strcmp(op, "grun_u_schedule_telegram_topic_title_rename") == 0) o = emit_grun_u_schedule_telegram_topic_title_rename(c);
        if (strcmp(op, "grun_u_disable_telegram_topic_mode_for_chat") == 0) o = emit_grun_u_disable_telegram_topic_mode_for_chat(c);
        if (strcmp(op, "grun_u_telegram_topic_root_status_message") == 0) o = emit_grun_u_telegram_topic_root_status_message(c);
        if (strcmp(op, "grun_u_restore_telegram_topic_session") == 0) o = emit_grun_u_restore_telegram_topic_session(c);
        if (strcmp(op, "grun_u_execute_mcp_reload") == 0) o = emit_grun_u_execute_mcp_reload(c);
        if (strcmp(op, "grun_u_maybe_confirm_destructive_slash") == 0) o = emit_grun_u_maybe_confirm_destructive_slash(c);
        if (strcmp(op, "grun_u_request_slash_confirm") == 0) o = emit_grun_u_request_slash_confirm(c);
        if (strcmp(op, "grun_u_read_user_config") == 0) o = emit_grun_u_read_user_config(c);
        if (strcmp(op, "grun_u_schedule_update_notification_watch") == 0) o = emit_grun_u_schedule_update_notification_watch(c);
        if (strcmp(op, "grun_u_watch_update_progress") == 0) o = emit_grun_u_watch_update_progress(c);
        if (strcmp(op, "grun_u_send_update_notification") == 0) o = emit_grun_u_send_update_notification(c);
        if (strcmp(op, "grun_u_send_restart_notification") == 0) o = emit_grun_u_send_restart_notification(c);
        if (strcmp(op, "grun_u_send_home_channel_startup_notifications") == 0) o = emit_grun_u_send_home_channel_startup_notifications(c);
        if (strcmp(op, "grun_u_set_session_env") == 0) o = emit_grun_u_set_session_env(c);
        if (strcmp(op, "grun_u_clear_session_env") == 0) o = emit_grun_u_clear_session_env(c);
        if (strcmp(op, "grun_u_run_in_executor_with_context") == 0) o = emit_grun_u_run_in_executor_with_context(c);
        if (strcmp(op, "grun_u_get_executor") == 0) o = emit_grun_u_get_executor(c);
        if (strcmp(op, "grun_u_shutdown_executor") == 0) o = emit_grun_u_shutdown_executor(c);
        if (strcmp(op, "grun_u_enrich_message_with_vision") == 0) o = emit_grun_u_enrich_message_with_vision(c);
        if (strcmp(op, "grun_u_enrich_message_with_transcription") == 0) o = emit_grun_u_enrich_message_with_transcription(c);
        if (strcmp(op, "grun_u_pending_event_audio_paths") == 0) o = emit_grun_u_pending_event_audio_paths(c);
        if (strcmp(op, "grun_u_transcribe_pending_audio_event_once") == 0) o = emit_grun_u_transcribe_pending_audio_event_once(c);
        if (strcmp(op, "grun_u_echo_pending_stt_transcripts_once") == 0) o = emit_grun_u_echo_pending_stt_transcripts_once(c);
        if (strcmp(op, "grun_u_transcribe_and_echo_pending_voice") == 0) o = emit_grun_u_transcribe_and_echo_pending_voice(c);
        if (strcmp(op, "grun_u_build_process_event_source") == 0) o = emit_grun_u_build_process_event_source(c);
        if (strcmp(op, "grun_u_inject_watch_notification") == 0) o = emit_grun_u_inject_watch_notification(c);
        if (strcmp(op, "grun_u_classify_completion_target") == 0) o = emit_grun_u_classify_completion_target(c);
        if (strcmp(op, "grun_u_deliver_completion_notification") == 0) o = emit_grun_u_deliver_completion_notification(c);
        if (strcmp(op, "grun_u_enrich_async_delegation_routing") == 0) o = emit_grun_u_enrich_async_delegation_routing(c);
        if (strcmp(op, "grun_u_async_delegation_watcher") == 0) o = emit_grun_u_async_delegation_watcher(c);
        if (strcmp(op, "grun_u_run_process_watcher") == 0) o = emit_grun_u_run_process_watcher(c);
        if (strcmp(op, "grun_u_extract_honcho_cache_busting_config") == 0) o = emit_grun_u_extract_honcho_cache_busting_config(c);
        if (strcmp(op, "grun_u_extract_cache_busting_config") == 0) o = emit_grun_u_extract_cache_busting_config(c);
        if (strcmp(op, "grun_u_agent_config_signature") == 0) o = emit_grun_u_agent_config_signature(c);
        if (strcmp(op, "grun_u_rehydrate_session_model_override") == 0) o = emit_grun_u_rehydrate_session_model_override(c);
        if (strcmp(op, "grun_u_release_running_agent_state") == 0) o = emit_grun_u_release_running_agent_state(c);
        if (strcmp(op, "grun_u_release_turn_lease") == 0) o = emit_grun_u_release_turn_lease(c);
        if (strcmp(op, "grun_u_rebind_turn_lease") == 0) o = emit_grun_u_rebind_turn_lease(c);
        if (strcmp(op, "grun_u_clear_conversation_scope") == 0) o = emit_grun_u_clear_conversation_scope(c);
        if (strcmp(op, "grun_u_clear_session_boundary_security_state") == 0) o = emit_grun_u_clear_session_boundary_security_state(c);
        if (strcmp(op, "grun_u_bind_adapter_run_generation") == 0) o = emit_grun_u_bind_adapter_run_generation(c);
        if (strcmp(op, "grun_u_interrupt_and_clear_session") == 0) o = emit_grun_u_interrupt_and_clear_session(c);
        if (strcmp(op, "grun_u_refresh_agent_cache_message_count") == 0) o = emit_grun_u_refresh_agent_cache_message_count(c);
        if (strcmp(op, "grun_u_voice_channel_sidecar_note") == 0) o = emit_grun_u_voice_channel_sidecar_note(c);
        if (strcmp(op, "grun_u_pinned_session_context_prompt") == 0) o = emit_grun_u_pinned_session_context_prompt(c);
        if (strcmp(op, "grun_u_init_cached_agent_for_turn") == 0) o = emit_grun_u_init_cached_agent_for_turn(c);
        if (strcmp(op, "grun_u_commit_memory_before_soft_evict") == 0) o = emit_grun_u_commit_memory_before_soft_evict(c);
        if (strcmp(op, "grun_u_commit_then_release_soft") == 0) o = emit_grun_u_commit_then_release_soft(c);
        if (strcmp(op, "grun_u_release_evicted_agent_soft") == 0) o = emit_grun_u_release_evicted_agent_soft(c);
        if (strcmp(op, "grun_u_enforce_agent_cache_cap") == 0) o = emit_grun_u_enforce_agent_cache_cap(c);
        if (strcmp(op, "grun_u_sweep_idle_cached_agents") == 0) o = emit_grun_u_sweep_idle_cached_agents(c);
        if (strcmp(op, "grun_u_run_agent_via_proxy") == 0) o = emit_grun_u_run_agent_via_proxy(c);
        if (strcmp(op, "grun_u_profile_name_for_source") == 0) o = emit_grun_u_profile_name_for_source(c);
        if (strcmp(op, "grun_u_resolve_profile_home_for_source") == 0) o = emit_grun_u_resolve_profile_home_for_source(c);
        if (strcmp(op, "grun_u_run_planned_stop_watcher") == 0) o = emit_grun_u_run_planned_stop_watcher(c);
        if (strcmp(op, "grun_u_start_gateway_housekeeping") == 0) o = emit_grun_u_start_gateway_housekeeping(c);
        if (strcmp(op, "grun_u_start_cron_ticker") == 0) o = emit_grun_u_start_cron_ticker(c);
        if (strcmp(op, "grun_u_await_thread_exit") == 0) o = emit_grun_u_await_thread_exit(c);
        if (strcmp(op, "grun_start_gateway") == 0) o = emit_grun_start_gateway(c);
        if (strcmp(op, "grun_u_exit_after_graceful_shutdown") == 0) o = emit_grun_u_exit_after_graceful_shutdown(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
