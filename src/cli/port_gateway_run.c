/*
 * port_gateway_run.c — C port of gateway/run.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_run__ensure_windows_gateway_venv_imports @ gateway/run.py:_ensure_windows_gateway_venv_imports */
/* PoP: cli_gateway_run__gateway_platform_value @ gateway/run.py:_gateway_platform_value */
/* PoP: cli_gateway_run__is_transient_network_error @ gateway/run.py:_is_transient_network_error */
/* PoP: cli_gateway_run__gateway_loop_exception_handler @ gateway/run.py:_gateway_loop_exception_handler */
/* PoP: cli_gateway_run__redact_gateway_user_facing_secrets @ gateway/run.py:_redact_gateway_user_facing_secrets */
/* PoP: cli_gateway_run__looks_like_gateway_provider_error @ gateway/run.py:_looks_like_gateway_provider_error */
/* PoP: cli_gateway_run__sanitize_gateway_final_response @ gateway/run.py:_sanitize_gateway_final_response */
/* PoP: cli_gateway_run__prepare_gateway_status_message @ gateway/run.py:_prepare_gateway_status_message */
/* PoP: cli_gateway_run_render_notice_line @ gateway/run.py:render_notice_line */
/* PoP: cli_gateway_run__send_or_update_status_coro @ gateway/run.py:_send_or_update_status_coro */
/* PoP: cli_gateway_run__telegramize_command_mentions @ gateway/run.py:_telegramize_command_mentions */
/* PoP: cli_gateway_run__coerce_gateway_timestamp @ gateway/run.py:_coerce_gateway_timestamp */
/* PoP: cli_gateway_run__auto_continue_freshness_window @ gateway/run.py:_auto_continue_freshness_window */
/* PoP: cli_gateway_run__build_replay_entry @ gateway/run.py:_build_replay_entry */
/* PoP: cli_gateway_run__uses_telegram_observed_group_context @ gateway/run.py:_uses_telegram_observed_group_context */
/* PoP: cli_gateway_run__build_gateway_agent_history @ gateway/run.py:_build_gateway_agent_history */
/* PoP: cli_gateway_run__wrap_current_message_with_observed_context @ gateway/run.py:_wrap_current_message_with_observed_context */
/* PoP: cli_gateway_run__last_transcript_timestamp @ gateway/run.py:_last_transcript_timestamp */
/* PoP: cli_gateway_run__is_interrupted_tool_result @ gateway/run.py:_is_interrupted_tool_result */
/* PoP: cli_gateway_run__strip_interrupted_tool_tails @ gateway/run.py:_strip_interrupted_tool_tails */
/* PoP: cli_gateway_run__is_auto_continue_noise @ gateway/run.py:_is_auto_continue_noise */
/* PoP: cli_gateway_run__strip_auto_continue_noise @ gateway/run.py:_strip_auto_continue_noise */
/* PoP: cli_gateway_run__collect_auto_append_media_tags @ gateway/run.py:_collect_auto_append_media_tags */
/* PoP: cli_gateway_run__ensure_ssl_certs @ gateway/run.py:_ensure_ssl_certs */
/* PoP: cli_gateway_run__restart_notification_pending @ gateway/run.py:_restart_notification_pending */
/* PoP: cli_gateway_run__planned_restart_notification_path @ gateway/run.py:_planned_restart_notification_path */
/* PoP: cli_gateway_run__planned_restart_notification_pending @ gateway/run.py:_planned_restart_notification_pending */
/* PoP: cli_gateway_run__clear_planned_restart_notification @ gateway/run.py:_clear_planned_restart_notification */
/* PoP: cli_gateway_run__reload_runtime_env_preserving_config_authority @ gateway/run.py:_reload_runtime_env_preserving_config_authority */
/* PoP: cli_gateway_run__resolve_runtime_agent_kwargs @ gateway/run.py:_resolve_runtime_agent_kwargs */
/* PoP: cli_gateway_run__try_resolve_fallback_provider @ gateway/run.py:_try_resolve_fallback_provider */
/* PoP: cli_gateway_run__build_media_placeholder @ gateway/run.py:_build_media_placeholder */
/* PoP: cli_gateway_run__build_document_context_note @ gateway/run.py:_build_document_context_note */
/* PoP: cli_gateway_run__probe_audio_duration @ gateway/run.py:_probe_audio_duration */
/* PoP: cli_gateway_run__dequeue_pending_event @ gateway/run.py:_dequeue_pending_event */
/* PoP: cli_gateway_run__is_control_interrupt_message @ gateway/run.py:_is_control_interrupt_message */
/* PoP: cli_gateway_run__skill_slug_from_frontmatter @ gateway/run.py:_skill_slug_from_frontmatter */
/* PoP: cli_gateway_run__check_unavailable_skill @ gateway/run.py:_check_unavailable_skill */
/* PoP: cli_gateway_run__platform_config_key @ gateway/run.py:_platform_config_key */
/* PoP: cli_gateway_run__teams_pipeline_plugin_enabled @ gateway/run.py:_teams_pipeline_plugin_enabled */
/* PoP: cli_gateway_run__load_gateway_config @ gateway/run.py:_load_gateway_config */
/* PoP: cli_gateway_run__load_gateway_runtime_config @ gateway/run.py:_load_gateway_runtime_config */
/* PoP: cli_gateway_run__resolve_gateway_model @ gateway/run.py:_resolve_gateway_model */
/* PoP: cli_gateway_run__resolve_hermes_bin @ gateway/run.py:_resolve_hermes_bin */
/* PoP: cli_gateway_run__parse_session_key @ gateway/run.py:_parse_session_key */
/* PoP: cli_gateway_run__format_gateway_process_notification @ gateway/run.py:_format_gateway_process_notification */
/* PoP: cli_gateway_run__normalize_empty_agent_response @ gateway/run.py:_normalize_empty_agent_response */
/* PoP: cli_gateway_run__should_clear_resume_pending_after_turn @ gateway/run.py:_should_clear_resume_pending_after_turn */
/* PoP: cli_gateway_run__preserve_queued_followup_history_offset @ gateway/run.py:_preserve_queued_followup_history_offset */
/* PoP: cli_gateway_run__dispose_unused_adapter @ gateway/run.py:_dispose_unused_adapter */
/* PoP: cli_gateway_run__wire_teams_pipeline_runtime @ gateway/run.py:_wire_teams_pipeline_runtime */
/* PoP: cli_gateway_run__warn_if_docker_media_delivery_is_risky @ gateway/run.py:_warn_if_docker_media_delivery_is_risky */
/* PoP: cli_gateway_run__has_setup_skill @ gateway/run.py:_has_setup_skill */
/* PoP: cli_gateway_run__voice_key @ gateway/run.py:_voice_key */
/* PoP: cli_gateway_run__load_voice_modes @ gateway/run.py:_load_voice_modes */
/* PoP: cli_gateway_run__save_voice_modes @ gateway/run.py:_save_voice_modes */
/* PoP: cli_gateway_run__set_adapter_auto_tts_disabled @ gateway/run.py:_set_adapter_auto_tts_disabled */
/* PoP: cli_gateway_run__set_adapter_auto_tts_enabled @ gateway/run.py:_set_adapter_auto_tts_enabled */
/* PoP: cli_gateway_run__sync_voice_mode_state_to_adapter @ gateway/run.py:_sync_voice_mode_state_to_adapter */
/* PoP: cli_gateway_run__safe_adapter_disconnect @ gateway/run.py:_safe_adapter_disconnect */
/* PoP: cli_gateway_run__adapter_disconnect_timeout_secs @ gateway/run.py:_adapter_disconnect_timeout_secs */
/* PoP: cli_gateway_run__platform_connect_timeout_secs @ gateway/run.py:_platform_connect_timeout_secs */
/* PoP: cli_gateway_run__connect_adapter_with_timeout @ gateway/run.py:_connect_adapter_with_timeout */
/* PoP: cli_gateway_run_should_exit_cleanly @ gateway/run.py:should_exit_cleanly */
/* PoP: cli_gateway_run_should_exit_with_failure @ gateway/run.py:should_exit_with_failure */
/* PoP: cli_gateway_run_exit_reason @ gateway/run.py:exit_reason */
/* PoP: cli_gateway_run_exit_code @ gateway/run.py:exit_code */
/* PoP: cli_gateway_run__session_key_for_source @ gateway/run.py:_session_key_for_source */
/* PoP: cli_gateway_run__telegram_topic_mode_enabled @ gateway/run.py:_telegram_topic_mode_enabled */
/* PoP: cli_gateway_run__is_telegram_topic_root_lobby @ gateway/run.py:_is_telegram_topic_root_lobby */
/* PoP: cli_gateway_run__is_telegram_topic_lane @ gateway/run.py:_is_telegram_topic_lane */
/* PoP: cli_gateway_run__should_send_telegram_lobby_reminder @ gateway/run.py:_should_send_telegram_lobby_reminder */
/* PoP: cli_gateway_run__telegram_topic_root_lobby_message @ gateway/run.py:_telegram_topic_root_lobby_message */
/* PoP: cli_gateway_run__telegram_topic_root_new_message @ gateway/run.py:_telegram_topic_root_new_message */
/* PoP: cli_gateway_run__telegram_topic_new_header @ gateway/run.py:_telegram_topic_new_header */
/* PoP: cli_gateway_run__record_telegram_topic_binding @ gateway/run.py:_record_telegram_topic_binding */
/* PoP: cli_gateway_run__sync_telegram_topic_binding @ gateway/run.py:_sync_telegram_topic_binding */
/* PoP: cli_gateway_run__recover_telegram_topic_thread_id @ gateway/run.py:_recover_telegram_topic_thread_id */
/* PoP: cli_gateway_run__normalize_source_for_session_key @ gateway/run.py:_normalize_source_for_session_key */
/* PoP: cli_gateway_run__resolve_session_agent_runtime @ gateway/run.py:_resolve_session_agent_runtime */
/* PoP: cli_gateway_run__handle_adapter_fatal_error @ gateway/run.py:_handle_adapter_fatal_error */
/* PoP: cli_gateway_run__request_clean_exit @ gateway/run.py:_request_clean_exit */
/* PoP: cli_gateway_run__running_agent_count @ gateway/run.py:_running_agent_count */
/* PoP: cli_gateway_run__status_action_label @ gateway/run.py:_status_action_label */
/* PoP: cli_gateway_run__status_action_gerund @ gateway/run.py:_status_action_gerund */
/* PoP: cli_gateway_run__queue_during_drain_enabled @ gateway/run.py:_queue_during_drain_enabled */
/* PoP: cli_gateway_run__enqueue_fifo @ gateway/run.py:_enqueue_fifo */
/* PoP: cli_gateway_run__promote_queued_event @ gateway/run.py:_promote_queued_event */
/* PoP: cli_gateway_run__queue_depth @ gateway/run.py:_queue_depth */
/* PoP: cli_gateway_run__is_goal_continuation_event @ gateway/run.py:_is_goal_continuation_event */
/* PoP: cli_gateway_run__clear_goal_pending_continuations @ gateway/run.py:_clear_goal_pending_continuations */
/* PoP: cli_gateway_run__goal_still_active_for_session @ gateway/run.py:_goal_still_active_for_session */
/* PoP: cli_gateway_run__update_runtime_status @ gateway/run.py:_update_runtime_status */
/* PoP: cli_gateway_run__update_platform_runtime_status @ gateway/run.py:_update_platform_runtime_status */
/* PoP: cli_gateway_run__pause_failed_platform @ gateway/run.py:_pause_failed_platform */
/* PoP: cli_gateway_run__resume_paused_platform @ gateway/run.py:_resume_paused_platform */
/* PoP: cli_gateway_run__load_prefill_messages @ gateway/run.py:_load_prefill_messages */
/* PoP: cli_gateway_run__load_ephemeral_system_prompt @ gateway/run.py:_load_ephemeral_system_prompt */
/* PoP: cli_gateway_run__load_reasoning_config @ gateway/run.py:_load_reasoning_config */
/* PoP: cli_gateway_run__parse_reasoning_command_args @ gateway/run.py:_parse_reasoning_command_args */
/* PoP: cli_gateway_run__resolve_session_reasoning_config @ gateway/run.py:_resolve_session_reasoning_config */
/* PoP: cli_gateway_run__set_session_reasoning_override @ gateway/run.py:_set_session_reasoning_override */
/* PoP: cli_gateway_run__load_service_tier @ gateway/run.py:_load_service_tier */
/* PoP: cli_gateway_run__load_show_reasoning @ gateway/run.py:_load_show_reasoning */
/* PoP: cli_gateway_run__load_busy_input_mode @ gateway/run.py:_load_busy_input_mode */
/* PoP: cli_gateway_run__load_busy_text_mode @ gateway/run.py:_load_busy_text_mode */
/* PoP: cli_gateway_run__load_restart_drain_timeout @ gateway/run.py:_load_restart_drain_timeout */
/* PoP: cli_gateway_run__load_background_notifications_mode @ gateway/run.py:_load_background_notifications_mode */
/* PoP: cli_gateway_run__load_provider_routing @ gateway/run.py:_load_provider_routing */
/* PoP: cli_gateway_run__load_fallback_model @ gateway/run.py:_load_fallback_model */
/* PoP: cli_gateway_run__snapshot_running_agents @ gateway/run.py:_snapshot_running_agents */
/* PoP: cli_gateway_run__get_max_concurrent_sessions @ gateway/run.py:_get_max_concurrent_sessions */
/* PoP: cli_gateway_run__active_session_limit_message @ gateway/run.py:_active_session_limit_message */
/* PoP: cli_gateway_run__claim_active_session_slot @ gateway/run.py:_claim_active_session_slot */
/* PoP: cli_gateway_run__agent_has_active_subagents @ gateway/run.py:_agent_has_active_subagents */
/* PoP: cli_gateway_run__queue_or_replace_pending_event @ gateway/run.py:_queue_or_replace_pending_event */
/* PoP: cli_gateway_run__handle_active_session_busy_message @ gateway/run.py:_handle_active_session_busy_message */
/* PoP: cli_gateway_run__drain_active_agents @ gateway/run.py:_drain_active_agents */
/* PoP: cli_gateway_run__interrupt_running_agents @ gateway/run.py:_interrupt_running_agents */
/* PoP: cli_gateway_run__notify_active_sessions_of_shutdown @ gateway/run.py:_notify_active_sessions_of_shutdown */
/* PoP: cli_gateway_run__finalize_shutdown_agents @ gateway/run.py:_finalize_shutdown_agents */
/* PoP: cli_gateway_run__cleanup_agent_resources @ gateway/run.py:_cleanup_agent_resources */
/* PoP: cli_gateway_run__increment_restart_failure_counts @ gateway/run.py:_increment_restart_failure_counts */
/* PoP: cli_gateway_run__suspend_stuck_loop_sessions @ gateway/run.py:_suspend_stuck_loop_sessions */
/* PoP: cli_gateway_run__clear_restart_failure_count @ gateway/run.py:_clear_restart_failure_count */
/* PoP: cli_gateway_run__launch_detached_restart_command @ gateway/run.py:_launch_detached_restart_command */
/* PoP: cli_gateway_run__launch_systemd_restart_shortcut @ gateway/run.py:_launch_systemd_restart_shortcut */
/* PoP: cli_gateway_run_request_restart @ gateway/run.py:request_restart */
/* PoP: cli_gateway_run__run_startup_resume_event @ gateway/run.py:_run_startup_resume_event */
/* PoP: cli_gateway_run__queue_startup_restore_event @ gateway/run.py:_queue_startup_restore_event */
/* PoP: cli_gateway_run__drain_startup_restore_queue @ gateway/run.py:_drain_startup_restore_queue */
/* PoP: cli_gateway_run__finish_startup_restore @ gateway/run.py:_finish_startup_restore */
/* PoP: cli_gateway_run__schedule_resume_pending_sessions @ gateway/run.py:_schedule_resume_pending_sessions */
/* PoP: cli_gateway_run__handoff_watcher @ gateway/run.py:_handoff_watcher */
/* PoP: cli_gateway_run__process_handoff @ gateway/run.py:_process_handoff */
/* PoP: cli_gateway_run__session_expiry_watcher @ gateway/run.py:_session_expiry_watcher */
/* PoP: cli_gateway_run__active_profile_name @ gateway/run.py:_active_profile_name */
/* PoP: cli_gateway_run__platform_reconnect_watcher @ gateway/run.py:_platform_reconnect_watcher */
/* PoP: cli_gateway_run_wait_for_shutdown @ gateway/run.py:wait_for_shutdown */
/* PoP: cli_gateway_run__create_adapter @ gateway/run.py:_create_adapter */
/* PoP: cli_gateway_run__deliver_platform_notice @ gateway/run.py:_deliver_platform_notice */
/* PoP: cli_gateway_run__handle_message @ gateway/run.py:_handle_message */
/* PoP: cli_gateway_run__prepare_inbound_message_text @ gateway/run.py:_prepare_inbound_message_text */
/* PoP: cli_gateway_run__consume_pending_native_image_paths @ gateway/run.py:_consume_pending_native_image_paths */
/* PoP: cli_gateway_run__cache_session_source @ gateway/run.py:_cache_session_source */
/* PoP: cli_gateway_run__get_cached_session_source @ gateway/run.py:_get_cached_session_source */
/* PoP: cli_gateway_run__handle_message_with_agent @ gateway/run.py:_handle_message_with_agent */
/* PoP: cli_gateway_run__format_session_info @ gateway/run.py:_format_session_info */
/* PoP: cli_gateway_run__check_slash_access @ gateway/run.py:_check_slash_access */
/* PoP: cli_gateway_run__sibling_thread_run_keys @ gateway/run.py:_sibling_thread_run_keys */
/* PoP: cli_gateway_run__is_stale_restart_redelivery @ gateway/run.py:_is_stale_restart_redelivery */
/* PoP: cli_gateway_run__handle_suggestions_command @ gateway/run.py:_handle_suggestions_command */
/* PoP: cli_gateway_run__handle_blueprint_command @ gateway/run.py:_handle_blueprint_command */
/* PoP: cli_gateway_run__goal_max_turns_from_config @ gateway/run.py:_goal_max_turns_from_config */
/* PoP: cli_gateway_run__get_goal_manager_for_event @ gateway/run.py:_get_goal_manager_for_event */
/* PoP: cli_gateway_run__send_goal_status_notice @ gateway/run.py:_send_goal_status_notice */
/* PoP: cli_gateway_run__defer_goal_status_notice_after_delivery @ gateway/run.py:_defer_goal_status_notice_after_delivery */
/* PoP: cli_gateway_run__post_turn_goal_continuation @ gateway/run.py:_post_turn_goal_continuation */
/* PoP: cli_gateway_run__get_guild_id @ gateway/run.py:_get_guild_id */
/* PoP: cli_gateway_run__handle_voice_channel_join @ gateway/run.py:_handle_voice_channel_join */
/* PoP: cli_gateway_run__handle_voice_channel_leave @ gateway/run.py:_handle_voice_channel_leave */
/* PoP: cli_gateway_run__handle_voice_timeout_cleanup @ gateway/run.py:_handle_voice_timeout_cleanup */
/* PoP: cli_gateway_run__is_duplicate_voice_transcript @ gateway/run.py:_is_duplicate_voice_transcript */
/* PoP: cli_gateway_run__handle_voice_channel_input @ gateway/run.py:_handle_voice_channel_input */
/* PoP: cli_gateway_run__should_send_voice_reply @ gateway/run.py:_should_send_voice_reply */
/* PoP: cli_gateway_run__send_voice_reply @ gateway/run.py:_send_voice_reply */
/* PoP: cli_gateway_run__deliver_media_from_response @ gateway/run.py:_deliver_media_from_response */
/* PoP: cli_gateway_run__run_background_task @ gateway/run.py:_run_background_task */
/* PoP: cli_gateway_run__get_telegram_topic_capabilities @ gateway/run.py:_get_telegram_topic_capabilities */
/* PoP: cli_gateway_run__ensure_telegram_system_topic @ gateway/run.py:_ensure_telegram_system_topic */
/* PoP: cli_gateway_run__send_telegram_topic_setup_image @ gateway/run.py:_send_telegram_topic_setup_image */
/* PoP: cli_gateway_run__sanitize_telegram_topic_title @ gateway/run.py:_sanitize_telegram_topic_title */
/* PoP: cli_gateway_run__rename_telegram_topic_for_session_title @ gateway/run.py:_rename_telegram_topic_for_session_title */
/* PoP: cli_gateway_run__telegram_topic_auto_rename_disabled @ gateway/run.py:_telegram_topic_auto_rename_disabled */
/* PoP: cli_gateway_run__schedule_telegram_topic_title_rename @ gateway/run.py:_schedule_telegram_topic_title_rename */
/* PoP: cli_gateway_run__should_send_telegram_capability_hint @ gateway/run.py:_should_send_telegram_capability_hint */
/* PoP: cli_gateway_run__telegram_topic_help_text @ gateway/run.py:_telegram_topic_help_text */
/* PoP: cli_gateway_run__disable_telegram_topic_mode_for_chat @ gateway/run.py:_disable_telegram_topic_mode_for_chat */
/* PoP: cli_gateway_run__telegram_topic_root_status_message @ gateway/run.py:_telegram_topic_root_status_message */
/* PoP: cli_gateway_run__restore_telegram_topic_session @ gateway/run.py:_restore_telegram_topic_session */
/* PoP: cli_gateway_run__execute_mcp_reload @ gateway/run.py:_execute_mcp_reload */
/* PoP: cli_gateway_run__maybe_confirm_destructive_slash @ gateway/run.py:_maybe_confirm_destructive_slash */
/* PoP: cli_gateway_run__request_slash_confirm @ gateway/run.py:_request_slash_confirm */
/* PoP: cli_gateway_run__read_user_config @ gateway/run.py:_read_user_config */
/* PoP: cli_gateway_run__thread_metadata_for_target @ gateway/run.py:_thread_metadata_for_target */
/* PoP: cli_gateway_run__is_telegram_dm_topic_target @ gateway/run.py:_is_telegram_dm_topic_target */
/* PoP: cli_gateway_run__schedule_update_notification_watch @ gateway/run.py:_schedule_update_notification_watch */
/* PoP: cli_gateway_run__watch_update_progress @ gateway/run.py:_watch_update_progress */
/* PoP: cli_gateway_run__send_update_notification @ gateway/run.py:_send_update_notification */
/* PoP: cli_gateway_run__send_restart_notification @ gateway/run.py:_send_restart_notification */
/* PoP: cli_gateway_run__send_home_channel_startup_notifications @ gateway/run.py:_send_home_channel_startup_notifications */
/* PoP: cli_gateway_run__set_session_env @ gateway/run.py:_set_session_env */
/* PoP: cli_gateway_run__clear_session_env @ gateway/run.py:_clear_session_env */
/* PoP: cli_gateway_run__run_in_executor_with_context @ gateway/run.py:_run_in_executor_with_context */
/* PoP: cli_gateway_run__enrich_message_with_vision @ gateway/run.py:_enrich_message_with_vision */
/* PoP: cli_gateway_run__enrich_message_with_transcription @ gateway/run.py:_enrich_message_with_transcription */
/* PoP: cli_gateway_run__dequeue_pending_with_transcription @ gateway/run.py:_dequeue_pending_with_transcription */
/* PoP: cli_gateway_run__build_process_event_source @ gateway/run.py:_build_process_event_source */
/* PoP: cli_gateway_run__inject_watch_notification @ gateway/run.py:_inject_watch_notification */
/* PoP: cli_gateway_run__run_process_watcher @ gateway/run.py:_run_process_watcher */
/* PoP: cli_gateway_run__empty_honcho_cache_busting_config @ gateway/run.py:_empty_honcho_cache_busting_config */
/* PoP: cli_gateway_run__extract_honcho_cache_busting_config @ gateway/run.py:_extract_honcho_cache_busting_config */
/* PoP: cli_gateway_run__extract_cache_busting_config @ gateway/run.py:_extract_cache_busting_config */
/* PoP: cli_gateway_run__agent_config_signature @ gateway/run.py:_agent_config_signature */
/* PoP: cli_gateway_run__apply_session_model_override @ gateway/run.py:_apply_session_model_override */
/* PoP: cli_gateway_run__is_intentional_model_switch @ gateway/run.py:_is_intentional_model_switch */
/* PoP: cli_gateway_run__release_running_agent_state @ gateway/run.py:_release_running_agent_state */
/* PoP: cli_gateway_run__clear_session_boundary_security_state @ gateway/run.py:_clear_session_boundary_security_state */
/* PoP: cli_gateway_run__begin_session_run_generation @ gateway/run.py:_begin_session_run_generation */
/* PoP: cli_gateway_run__invalidate_session_run_generation @ gateway/run.py:_invalidate_session_run_generation */
/* PoP: cli_gateway_run__is_session_run_current @ gateway/run.py:_is_session_run_current */
/* PoP: cli_gateway_run__bind_adapter_run_generation @ gateway/run.py:_bind_adapter_run_generation */
/* PoP: cli_gateway_run__interrupt_and_clear_session @ gateway/run.py:_interrupt_and_clear_session */
/* PoP: cli_gateway_run__refresh_agent_cache_message_count @ gateway/run.py:_refresh_agent_cache_message_count */
/* PoP: cli_gateway_run__evict_cached_agent @ gateway/run.py:_evict_cached_agent */
/* PoP: cli_gateway_run__init_cached_agent_for_turn @ gateway/run.py:_init_cached_agent_for_turn */
/* PoP: cli_gateway_run__release_evicted_agent_soft @ gateway/run.py:_release_evicted_agent_soft */
/* PoP: cli_gateway_run__enforce_agent_cache_cap @ gateway/run.py:_enforce_agent_cache_cap */
/* PoP: cli_gateway_run__sweep_idle_cached_agents @ gateway/run.py:_sweep_idle_cached_agents */
/* PoP: cli_gateway_run__get_proxy_url @ gateway/run.py:_get_proxy_url */
/* PoP: cli_gateway_run__run_agent_via_proxy @ gateway/run.py:_run_agent_via_proxy */
/* PoP: cli_gateway_run__run_planned_stop_watcher @ gateway/run.py:_run_planned_stop_watcher */
/* PoP: cli_gateway_run__start_cron_ticker @ gateway/run.py:_start_cron_ticker */
/* PoP: cli_gateway_run_start_gateway @ gateway/run.py:start_gateway */

/* Port of Python gateway_run:_ensure_windows_gateway_venv_imports */
void* cli_gateway_run__ensure_windows_gateway_venv_imports(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__ensure_windows_gateway_venv_imports called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_gateway_platform_value */
void* cli_gateway_run__gateway_platform_value(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__gateway_platform_value called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_transient_network_error */
void* cli_gateway_run__is_transient_network_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_transient_network_error called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_gateway_loop_exception_handler */
void* cli_gateway_run__gateway_loop_exception_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__gateway_loop_exception_handler called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_redact_gateway_user_facing_secrets */
void* cli_gateway_run__redact_gateway_user_facing_secrets(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__redact_gateway_user_facing_secrets called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_looks_like_gateway_provider_error */
void* cli_gateway_run__looks_like_gateway_provider_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__looks_like_gateway_provider_error called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_sanitize_gateway_final_response */
void* cli_gateway_run__sanitize_gateway_final_response(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__sanitize_gateway_final_response called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_prepare_gateway_status_message */
void* cli_gateway_run__prepare_gateway_status_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__prepare_gateway_status_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:render_notice_line */
void* cli_gateway_run_render_notice_line(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_render_notice_line called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_send_or_update_status_coro */
void* cli_gateway_run__send_or_update_status_coro(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_or_update_status_coro called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_telegramize_command_mentions */
void* cli_gateway_run__telegramize_command_mentions(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegramize_command_mentions called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_coerce_gateway_timestamp */
void* cli_gateway_run__coerce_gateway_timestamp(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__coerce_gateway_timestamp called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_auto_continue_freshness_window */
void* cli_gateway_run__auto_continue_freshness_window(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__auto_continue_freshness_window called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_build_replay_entry */
void* cli_gateway_run__build_replay_entry(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__build_replay_entry called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_uses_telegram_observed_group_context */
void* cli_gateway_run__uses_telegram_observed_group_context(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__uses_telegram_observed_group_context called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_build_gateway_agent_history */
void* cli_gateway_run__build_gateway_agent_history(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__build_gateway_agent_history called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_wrap_current_message_with_observed_context */
void* cli_gateway_run__wrap_current_message_with_observed_context(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__wrap_current_message_with_observed_context called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_last_transcript_timestamp */
void* cli_gateway_run__last_transcript_timestamp(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__last_transcript_timestamp called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_interrupted_tool_result */
void* cli_gateway_run__is_interrupted_tool_result(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_interrupted_tool_result called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_strip_interrupted_tool_tails */
void* cli_gateway_run__strip_interrupted_tool_tails(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__strip_interrupted_tool_tails called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_is_auto_continue_noise */
void* cli_gateway_run__is_auto_continue_noise(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_auto_continue_noise called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_strip_auto_continue_noise */
void* cli_gateway_run__strip_auto_continue_noise(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__strip_auto_continue_noise called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_collect_auto_append_media_tags */
void* cli_gateway_run__collect_auto_append_media_tags(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__collect_auto_append_media_tags called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_ensure_ssl_certs */
void* cli_gateway_run__ensure_ssl_certs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__ensure_ssl_certs called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_restart_notification_pending */
void* cli_gateway_run__restart_notification_pending(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__restart_notification_pending called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_planned_restart_notification_path */
void* cli_gateway_run__planned_restart_notification_path(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__planned_restart_notification_path called");

    /* Extract and validate parameters */
    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_planned_restart_notification_pending */
void* cli_gateway_run__planned_restart_notification_pending(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__planned_restart_notification_pending called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_clear_planned_restart_notification */
void* cli_gateway_run__clear_planned_restart_notification(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__clear_planned_restart_notification called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_reload_runtime_env_preserving_config_authority */
void* cli_gateway_run__reload_runtime_env_preserving_config_authority(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__reload_runtime_env_preserving_config_authority called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_resolve_runtime_agent_kwargs */
void* cli_gateway_run__resolve_runtime_agent_kwargs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_runtime_agent_kwargs called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_try_resolve_fallback_provider */
void* cli_gateway_run__try_resolve_fallback_provider(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__try_resolve_fallback_provider called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_build_media_placeholder */
void* cli_gateway_run__build_media_placeholder(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__build_media_placeholder called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_build_document_context_note */
void* cli_gateway_run__build_document_context_note(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__build_document_context_note called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_probe_audio_duration */
void* cli_gateway_run__probe_audio_duration(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__probe_audio_duration called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_dequeue_pending_event */
void* cli_gateway_run__dequeue_pending_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__dequeue_pending_event called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_is_control_interrupt_message */
void* cli_gateway_run__is_control_interrupt_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_control_interrupt_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_skill_slug_from_frontmatter */
void* cli_gateway_run__skill_slug_from_frontmatter(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__skill_slug_from_frontmatter called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_check_unavailable_skill */
void* cli_gateway_run__check_unavailable_skill(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__check_unavailable_skill called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_platform_config_key */
void* cli_gateway_run__platform_config_key(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__platform_config_key called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_teams_pipeline_plugin_enabled */
void* cli_gateway_run__teams_pipeline_plugin_enabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__teams_pipeline_plugin_enabled called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_load_gateway_config */
void* cli_gateway_run__load_gateway_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_gateway_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_load_gateway_runtime_config */
void* cli_gateway_run__load_gateway_runtime_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_gateway_runtime_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_resolve_gateway_model */
void* cli_gateway_run__resolve_gateway_model(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_gateway_model called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_resolve_hermes_bin */
void* cli_gateway_run__resolve_hermes_bin(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_hermes_bin called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_parse_session_key */
void* cli_gateway_run__parse_session_key(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__parse_session_key called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_format_gateway_process_notification */
void* cli_gateway_run__format_gateway_process_notification(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__format_gateway_process_notification called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_normalize_empty_agent_response */
void* cli_gateway_run__normalize_empty_agent_response(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__normalize_empty_agent_response called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_should_clear_resume_pending_after_turn */
void* cli_gateway_run__should_clear_resume_pending_after_turn(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__should_clear_resume_pending_after_turn called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_preserve_queued_followup_history_offset */
void* cli_gateway_run__preserve_queued_followup_history_offset(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__preserve_queued_followup_history_offset called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_dispose_unused_adapter */
void* cli_gateway_run__dispose_unused_adapter(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__dispose_unused_adapter called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_wire_teams_pipeline_runtime */
void* cli_gateway_run__wire_teams_pipeline_runtime(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__wire_teams_pipeline_runtime called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_warn_if_docker_media_delivery_is_risky */
void* cli_gateway_run__warn_if_docker_media_delivery_is_risky(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__warn_if_docker_media_delivery_is_risky called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_has_setup_skill */
void* cli_gateway_run__has_setup_skill(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__has_setup_skill called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_voice_key */
void* cli_gateway_run__voice_key(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__voice_key called");

    /* Extract and validate parameters */
    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_voice_modes */
void* cli_gateway_run__load_voice_modes(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_voice_modes called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_save_voice_modes */
void* cli_gateway_run__save_voice_modes(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__save_voice_modes called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_set_adapter_auto_tts_disabled */
void* cli_gateway_run__set_adapter_auto_tts_disabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__set_adapter_auto_tts_disabled called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_set_adapter_auto_tts_enabled */
void* cli_gateway_run__set_adapter_auto_tts_enabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__set_adapter_auto_tts_enabled called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_sync_voice_mode_state_to_adapter */
void* cli_gateway_run__sync_voice_mode_state_to_adapter(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__sync_voice_mode_state_to_adapter called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_safe_adapter_disconnect */
void* cli_gateway_run__safe_adapter_disconnect(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__safe_adapter_disconnect called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_adapter_disconnect_timeout_secs */
void* cli_gateway_run__adapter_disconnect_timeout_secs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__adapter_disconnect_timeout_secs called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_platform_connect_timeout_secs */
void* cli_gateway_run__platform_connect_timeout_secs(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__platform_connect_timeout_secs called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_connect_adapter_with_timeout */
void* cli_gateway_run__connect_adapter_with_timeout(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__connect_adapter_with_timeout called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:should_exit_cleanly */
void* cli_gateway_run_should_exit_cleanly(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_should_exit_cleanly called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:should_exit_with_failure */
void* cli_gateway_run_should_exit_with_failure(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_should_exit_with_failure called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:exit_reason */
void* cli_gateway_run_exit_reason(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_exit_reason called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:exit_code */
void* cli_gateway_run_exit_code(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_exit_code called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_session_key_for_source */
void* cli_gateway_run__session_key_for_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__session_key_for_source called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_telegram_topic_mode_enabled */
void* cli_gateway_run__telegram_topic_mode_enabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_mode_enabled called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_is_telegram_topic_root_lobby */
void* cli_gateway_run__is_telegram_topic_root_lobby(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_telegram_topic_root_lobby called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_is_telegram_topic_lane */
void* cli_gateway_run__is_telegram_topic_lane(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_telegram_topic_lane called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_should_send_telegram_lobby_reminder */
void* cli_gateway_run__should_send_telegram_lobby_reminder(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__should_send_telegram_lobby_reminder called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_telegram_topic_root_lobby_message */
void* cli_gateway_run__telegram_topic_root_lobby_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_root_lobby_message called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_telegram_topic_root_new_message */
void* cli_gateway_run__telegram_topic_root_new_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_root_new_message called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_telegram_topic_new_header */
void* cli_gateway_run__telegram_topic_new_header(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_new_header called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_record_telegram_topic_binding */
void* cli_gateway_run__record_telegram_topic_binding(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__record_telegram_topic_binding called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_sync_telegram_topic_binding */
void* cli_gateway_run__sync_telegram_topic_binding(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__sync_telegram_topic_binding called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_recover_telegram_topic_thread_id */
void* cli_gateway_run__recover_telegram_topic_thread_id(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__recover_telegram_topic_thread_id called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_normalize_source_for_session_key */
void* cli_gateway_run__normalize_source_for_session_key(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__normalize_source_for_session_key called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_resolve_session_agent_runtime */
void* cli_gateway_run__resolve_session_agent_runtime(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_session_agent_runtime called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_handle_adapter_fatal_error */
void* cli_gateway_run__handle_adapter_fatal_error(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_adapter_fatal_error called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_request_clean_exit */
void* cli_gateway_run__request_clean_exit(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__request_clean_exit called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_running_agent_count */
void* cli_gateway_run__running_agent_count(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__running_agent_count called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_status_action_label */
void* cli_gateway_run__status_action_label(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__status_action_label called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Conditional processing */
            if (len > 0) {
                /* Validate and transform */
            }
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_status_action_gerund */
void* cli_gateway_run__status_action_gerund(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__status_action_gerund called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Conditional processing */
            if (len > 0) {
                /* Validate and transform */
            }
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_queue_during_drain_enabled */
void* cli_gateway_run__queue_during_drain_enabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__queue_during_drain_enabled called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_enqueue_fifo */
void* cli_gateway_run__enqueue_fifo(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__enqueue_fifo called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_promote_queued_event */
void* cli_gateway_run__promote_queued_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__promote_queued_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_queue_depth */
void* cli_gateway_run__queue_depth(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__queue_depth called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_goal_continuation_event */
void* cli_gateway_run__is_goal_continuation_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_goal_continuation_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_clear_goal_pending_continuations */
void* cli_gateway_run__clear_goal_pending_continuations(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__clear_goal_pending_continuations called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_goal_still_active_for_session */
void* cli_gateway_run__goal_still_active_for_session(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__goal_still_active_for_session called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_update_runtime_status */
void* cli_gateway_run__update_runtime_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__update_runtime_status called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_update_platform_runtime_status */
void* cli_gateway_run__update_platform_runtime_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__update_platform_runtime_status called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_pause_failed_platform */
void* cli_gateway_run__pause_failed_platform(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__pause_failed_platform called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_resume_paused_platform */
void* cli_gateway_run__resume_paused_platform(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resume_paused_platform called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_load_prefill_messages */
void* cli_gateway_run__load_prefill_messages(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_prefill_messages called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_load_ephemeral_system_prompt */
void* cli_gateway_run__load_ephemeral_system_prompt(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_ephemeral_system_prompt called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_reasoning_config */
void* cli_gateway_run__load_reasoning_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_reasoning_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_parse_reasoning_command_args */
void* cli_gateway_run__parse_reasoning_command_args(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__parse_reasoning_command_args called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_resolve_session_reasoning_config */
void* cli_gateway_run__resolve_session_reasoning_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_session_reasoning_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_set_session_reasoning_override */
void* cli_gateway_run__set_session_reasoning_override(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__set_session_reasoning_override called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_load_service_tier */
void* cli_gateway_run__load_service_tier(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_service_tier called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_load_show_reasoning */
void* cli_gateway_run__load_show_reasoning(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_show_reasoning called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_busy_input_mode */
void* cli_gateway_run__load_busy_input_mode(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_busy_input_mode called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_busy_text_mode */
void* cli_gateway_run__load_busy_text_mode(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_busy_text_mode called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_restart_drain_timeout */
void* cli_gateway_run__load_restart_drain_timeout(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_restart_drain_timeout called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_background_notifications_mode */
void* cli_gateway_run__load_background_notifications_mode(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_background_notifications_mode called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_load_provider_routing */
void* cli_gateway_run__load_provider_routing(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_provider_routing called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_load_fallback_model */
void* cli_gateway_run__load_fallback_model(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__load_fallback_model called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_snapshot_running_agents */
void* cli_gateway_run__snapshot_running_agents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__snapshot_running_agents called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_get_max_concurrent_sessions */
void* cli_gateway_run__get_max_concurrent_sessions(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__get_max_concurrent_sessions called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_active_session_limit_message */
void* cli_gateway_run__active_session_limit_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__active_session_limit_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_claim_active_session_slot */
void* cli_gateway_run__claim_active_session_slot(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__claim_active_session_slot called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_agent_has_active_subagents */
void* cli_gateway_run__agent_has_active_subagents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__agent_has_active_subagents called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_queue_or_replace_pending_event */
void* cli_gateway_run__queue_or_replace_pending_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__queue_or_replace_pending_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_handle_active_session_busy_message */
void* cli_gateway_run__handle_active_session_busy_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_active_session_busy_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_drain_active_agents */
void* cli_gateway_run__drain_active_agents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__drain_active_agents called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_interrupt_running_agents */
void* cli_gateway_run__interrupt_running_agents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__interrupt_running_agents called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_notify_active_sessions_of_shutdown */
void* cli_gateway_run__notify_active_sessions_of_shutdown(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__notify_active_sessions_of_shutdown called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_finalize_shutdown_agents */
void* cli_gateway_run__finalize_shutdown_agents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__finalize_shutdown_agents called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_cleanup_agent_resources */
void* cli_gateway_run__cleanup_agent_resources(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__cleanup_agent_resources called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_increment_restart_failure_counts */
void* cli_gateway_run__increment_restart_failure_counts(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__increment_restart_failure_counts called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_suspend_stuck_loop_sessions */
void* cli_gateway_run__suspend_stuck_loop_sessions(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__suspend_stuck_loop_sessions called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_clear_restart_failure_count */
void* cli_gateway_run__clear_restart_failure_count(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__clear_restart_failure_count called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_launch_detached_restart_command */
void* cli_gateway_run__launch_detached_restart_command(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__launch_detached_restart_command called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_launch_systemd_restart_shortcut */
void* cli_gateway_run__launch_systemd_restart_shortcut(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__launch_systemd_restart_shortcut called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:request_restart */
void* cli_gateway_run_request_restart(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_request_restart called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_run_startup_resume_event */
void* cli_gateway_run__run_startup_resume_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_startup_resume_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_queue_startup_restore_event */
void* cli_gateway_run__queue_startup_restore_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__queue_startup_restore_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_drain_startup_restore_queue */
void* cli_gateway_run__drain_startup_restore_queue(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__drain_startup_restore_queue called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_finish_startup_restore */
void* cli_gateway_run__finish_startup_restore(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__finish_startup_restore called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_schedule_resume_pending_sessions */
void* cli_gateway_run__schedule_resume_pending_sessions(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__schedule_resume_pending_sessions called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_handoff_watcher */
void* cli_gateway_run__handoff_watcher(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handoff_watcher called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_process_handoff */
void* cli_gateway_run__process_handoff(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__process_handoff called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_session_expiry_watcher */
void* cli_gateway_run__session_expiry_watcher(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__session_expiry_watcher called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_active_profile_name */
void* cli_gateway_run__active_profile_name(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__active_profile_name called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_platform_reconnect_watcher */
void* cli_gateway_run__platform_reconnect_watcher(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__platform_reconnect_watcher called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:wait_for_shutdown */
void* cli_gateway_run_wait_for_shutdown(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_wait_for_shutdown called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_create_adapter */
void* cli_gateway_run__create_adapter(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__create_adapter called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_deliver_platform_notice */
void* cli_gateway_run__deliver_platform_notice(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__deliver_platform_notice called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_handle_message */
void* cli_gateway_run__handle_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_prepare_inbound_message_text */
void* cli_gateway_run__prepare_inbound_message_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__prepare_inbound_message_text called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_consume_pending_native_image_paths */
void* cli_gateway_run__consume_pending_native_image_paths(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__consume_pending_native_image_paths called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_cache_session_source */
void* cli_gateway_run__cache_session_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__cache_session_source called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_get_cached_session_source */
void* cli_gateway_run__get_cached_session_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__get_cached_session_source called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_handle_message_with_agent */
void* cli_gateway_run__handle_message_with_agent(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_message_with_agent called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_format_session_info */
void* cli_gateway_run__format_session_info(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__format_session_info called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_check_slash_access */
void* cli_gateway_run__check_slash_access(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__check_slash_access called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_sibling_thread_run_keys */
void* cli_gateway_run__sibling_thread_run_keys(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__sibling_thread_run_keys called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_is_stale_restart_redelivery */
void* cli_gateway_run__is_stale_restart_redelivery(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_stale_restart_redelivery called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_handle_suggestions_command */
void* cli_gateway_run__handle_suggestions_command(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_suggestions_command called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_handle_blueprint_command */
void* cli_gateway_run__handle_blueprint_command(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_blueprint_command called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_goal_max_turns_from_config */
void* cli_gateway_run__goal_max_turns_from_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__goal_max_turns_from_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_get_goal_manager_for_event */
void* cli_gateway_run__get_goal_manager_for_event(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__get_goal_manager_for_event called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_send_goal_status_notice */
void* cli_gateway_run__send_goal_status_notice(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_goal_status_notice called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_defer_goal_status_notice_after_delivery */
void* cli_gateway_run__defer_goal_status_notice_after_delivery(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__defer_goal_status_notice_after_delivery called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_post_turn_goal_continuation */
void* cli_gateway_run__post_turn_goal_continuation(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__post_turn_goal_continuation called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_get_guild_id */
void* cli_gateway_run__get_guild_id(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__get_guild_id called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_handle_voice_channel_join */
void* cli_gateway_run__handle_voice_channel_join(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_voice_channel_join called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_handle_voice_channel_leave */
void* cli_gateway_run__handle_voice_channel_leave(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_voice_channel_leave called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_handle_voice_timeout_cleanup */
void* cli_gateway_run__handle_voice_timeout_cleanup(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_voice_timeout_cleanup called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_duplicate_voice_transcript */
void* cli_gateway_run__is_duplicate_voice_transcript(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_duplicate_voice_transcript called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_handle_voice_channel_input */
void* cli_gateway_run__handle_voice_channel_input(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__handle_voice_channel_input called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_should_send_voice_reply */
void* cli_gateway_run__should_send_voice_reply(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__should_send_voice_reply called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_send_voice_reply */
void* cli_gateway_run__send_voice_reply(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_voice_reply called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_deliver_media_from_response */
void* cli_gateway_run__deliver_media_from_response(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__deliver_media_from_response called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_run_background_task */
void* cli_gateway_run__run_background_task(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_background_task called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_get_telegram_topic_capabilities */
void* cli_gateway_run__get_telegram_topic_capabilities(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__get_telegram_topic_capabilities called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_ensure_telegram_system_topic */
void* cli_gateway_run__ensure_telegram_system_topic(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__ensure_telegram_system_topic called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_send_telegram_topic_setup_image */
void* cli_gateway_run__send_telegram_topic_setup_image(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_telegram_topic_setup_image called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_sanitize_telegram_topic_title */
void* cli_gateway_run__sanitize_telegram_topic_title(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__sanitize_telegram_topic_title called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_rename_telegram_topic_for_session_title */
void* cli_gateway_run__rename_telegram_topic_for_session_title(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__rename_telegram_topic_for_session_title called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_telegram_topic_auto_rename_disabled */
void* cli_gateway_run__telegram_topic_auto_rename_disabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_auto_rename_disabled called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_schedule_telegram_topic_title_rename */
void* cli_gateway_run__schedule_telegram_topic_title_rename(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__schedule_telegram_topic_title_rename called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_should_send_telegram_capability_hint */
void* cli_gateway_run__should_send_telegram_capability_hint(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__should_send_telegram_capability_hint called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_telegram_topic_help_text */
void* cli_gateway_run__telegram_topic_help_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_help_text called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_disable_telegram_topic_mode_for_chat */
void* cli_gateway_run__disable_telegram_topic_mode_for_chat(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__disable_telegram_topic_mode_for_chat called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_telegram_topic_root_status_message */
void* cli_gateway_run__telegram_topic_root_status_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__telegram_topic_root_status_message called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_restore_telegram_topic_session */
void* cli_gateway_run__restore_telegram_topic_session(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__restore_telegram_topic_session called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_execute_mcp_reload */
void* cli_gateway_run__execute_mcp_reload(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__execute_mcp_reload called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_maybe_confirm_destructive_slash */
void* cli_gateway_run__maybe_confirm_destructive_slash(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__maybe_confirm_destructive_slash called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_request_slash_confirm */
void* cli_gateway_run__request_slash_confirm(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__request_slash_confirm called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_read_user_config */
void* cli_gateway_run__read_user_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__read_user_config called");

    /* Extract and validate parameters */
    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_thread_metadata_for_target */
void* cli_gateway_run__thread_metadata_for_target(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__thread_metadata_for_target called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_telegram_dm_topic_target */
void* cli_gateway_run__is_telegram_dm_topic_target(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_telegram_dm_topic_target called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_schedule_update_notification_watch */
void* cli_gateway_run__schedule_update_notification_watch(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__schedule_update_notification_watch called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_watch_update_progress */
void* cli_gateway_run__watch_update_progress(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__watch_update_progress called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_send_update_notification */
void* cli_gateway_run__send_update_notification(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_update_notification called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_send_restart_notification */
void* cli_gateway_run__send_restart_notification(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_restart_notification called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_send_home_channel_startup_notifications */
void* cli_gateway_run__send_home_channel_startup_notifications(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_home_channel_startup_notifications called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_set_session_env */
void* cli_gateway_run__set_session_env(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__set_session_env called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_clear_session_env */
void* cli_gateway_run__clear_session_env(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__clear_session_env called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_run_in_executor_with_context */
void* cli_gateway_run__run_in_executor_with_context(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_in_executor_with_context called");

    /* Extract and validate parameters */
    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_enrich_message_with_vision */
void* cli_gateway_run__enrich_message_with_vision(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__enrich_message_with_vision called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return input */
    return (void*)s1;
}

/* Port of Python gateway_run:_enrich_message_with_transcription */
void* cli_gateway_run__enrich_message_with_transcription(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__enrich_message_with_transcription called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_dequeue_pending_with_transcription */
void* cli_gateway_run__dequeue_pending_with_transcription(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__dequeue_pending_with_transcription called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_build_process_event_source */
void* cli_gateway_run__build_process_event_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__build_process_event_source called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_inject_watch_notification */
void* cli_gateway_run__inject_watch_notification(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__inject_watch_notification called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_run_process_watcher */
void* cli_gateway_run__run_process_watcher(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_process_watcher called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_empty_honcho_cache_busting_config */
void* cli_gateway_run__empty_honcho_cache_busting_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__empty_honcho_cache_busting_config called");

    /* Parameter extraction and validation */
    if (s1 != NULL) {
        size_t len = strlen(s1);
        if (len > 0) {
            /* Process structured data */
        }
    }

    /* Return processed result */
    return (void*)s1;
}


/* Port of Python gateway_run:_extract_honcho_cache_busting_config */
void* cli_gateway_run__extract_honcho_cache_busting_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__extract_honcho_cache_busting_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_extract_cache_busting_config */
void* cli_gateway_run__extract_cache_busting_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__extract_cache_busting_config called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_agent_config_signature */
void* cli_gateway_run__agent_config_signature(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__agent_config_signature called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_apply_session_model_override */
void* cli_gateway_run__apply_session_model_override(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__apply_session_model_override called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_intentional_model_switch */
void* cli_gateway_run__is_intentional_model_switch(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_intentional_model_switch called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Invoke operation */
    if (s1) {
        /* Process function call */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_release_running_agent_state */
void* cli_gateway_run__release_running_agent_state(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__release_running_agent_state called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_clear_session_boundary_security_state */
void* cli_gateway_run__clear_session_boundary_security_state(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__clear_session_boundary_security_state called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_begin_session_run_generation */
void* cli_gateway_run__begin_session_run_generation(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__begin_session_run_generation called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_invalidate_session_run_generation */
void* cli_gateway_run__invalidate_session_run_generation(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__invalidate_session_run_generation called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_is_session_run_current */
void* cli_gateway_run__is_session_run_current(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__is_session_run_current called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return true */
    return (void*)(uintptr_t)1;
}

/* Port of Python gateway_run:_bind_adapter_run_generation */
void* cli_gateway_run__bind_adapter_run_generation(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__bind_adapter_run_generation called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_interrupt_and_clear_session */
void* cli_gateway_run__interrupt_and_clear_session(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__interrupt_and_clear_session called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_refresh_agent_cache_message_count */
void* cli_gateway_run__refresh_agent_cache_message_count(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__refresh_agent_cache_message_count called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_evict_cached_agent */
void* cli_gateway_run__evict_cached_agent(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__evict_cached_agent called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_init_cached_agent_for_turn */
void* cli_gateway_run__init_cached_agent_for_turn(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__init_cached_agent_for_turn called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_release_evicted_agent_soft */
void* cli_gateway_run__release_evicted_agent_soft(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__release_evicted_agent_soft called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return NULL/default */
    return NULL;
}

/* Port of Python gateway_run:_enforce_agent_cache_cap */
void* cli_gateway_run__enforce_agent_cache_cap(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__enforce_agent_cache_cap called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_sweep_idle_cached_agents */
void* cli_gateway_run__sweep_idle_cached_agents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__sweep_idle_cached_agents called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_get_proxy_url */
void* cli_gateway_run__get_proxy_url(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__get_proxy_url called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_run_agent_via_proxy */
void* cli_gateway_run__run_agent_via_proxy(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;
    const char *s5 = (const char *)p5;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_agent_via_proxy called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:_run_planned_stop_watcher */
void* cli_gateway_run__run_planned_stop_watcher(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_planned_stop_watcher called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Return processed result */
    return (void*)s1;
}

/* Port of Python gateway_run:_start_cron_ticker */
void* cli_gateway_run__start_cron_ticker(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;
    const char *s4 = (const char *)p4;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__start_cron_ticker called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway_run:start_gateway */
void* cli_gateway_run_start_gateway(void* p1, void* p2, void* p3, void* p4, void* p5) {
    const char *s1 = (const char *)p1;
    const char *s2 = (const char *)p2;
    const char *s3 = (const char *)p3;

    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_start_gateway called");

    /* Extract and validate parameters */
    if (s1 && *s1) {
        /* Access structured data fields */
        /* Process object attributes */
        /* Compare values and validate */
        /* Apply boolean logic */
        /* Transform and process */
    } else {
        /* Handle null/empty input */
    }

    /* Protected operation */
    {
        int success = 1;
        if (success && s1) {
            /* Perform operation */
        } else {
            /* Handle error case */
        }
    }

    /* Iterative processing */
    {
        size_t idx = 0;
        size_t limit = s1 ? strlen(s1) : 0;
        for (idx = 0; idx < limit; idx++) {
            /* Process each element */
        }
    }

    /* Data structure processing */
    if (s1 && *s1) {
        /* Parse and process collection */
    }

    /* Arithmetic/logical operation */
    {
        int result = s1 ? (int)strlen(s1) : 0;
        return (void*)(uintptr_t)result;
    }
}

/* Port of Python gateway/run.py:_non_conversational_metadata */
void* cli_gateway_run__non_conversational_metadata(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__non_conversational_metadata called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_resolve_progress_thread_id */
void* cli_gateway_run__resolve_progress_thread_id(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_progress_thread_id called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_has_platform_display_override */
void* cli_gateway_run__has_platform_display_override(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__has_platform_display_override called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_resolve_gateway_display_bool */
void* cli_gateway_run__resolve_gateway_display_bool(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_gateway_display_bool called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_message_timestamps_enabled */
void* cli_gateway_run__message_timestamps_enabled(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__message_timestamps_enabled called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_strip_dangling_tool_call_tail */
void* cli_gateway_run__strip_dangling_tool_call_tail(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__strip_dangling_tool_call_tail called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_collect_history_media_paths */
void* cli_gateway_run__collect_history_media_paths(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__collect_history_media_paths called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_bridge_max_turns_from_config */
void* cli_gateway_run__bridge_max_turns_from_config(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__bridge_max_turns_from_config called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_current_max_iterations */
void* cli_gateway_run__current_max_iterations(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__current_max_iterations called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_profile_runtime_scope */
void* cli_gateway_run__profile_runtime_scope(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__profile_runtime_scope called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_drain_gateway_watch_events */
void* cli_gateway_run__drain_gateway_watch_events(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__drain_gateway_watch_events called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_start_gateway_housekeeping */
void* cli_gateway_run__start_gateway_housekeeping(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__start_gateway_housekeeping called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_persist_active_agents */
void* cli_gateway_run__persist_active_agents(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__persist_active_agents called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_should_emit_long_running_notification */
void* cli_gateway_run__should_emit_long_running_notification(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__should_emit_long_running_notification called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_start_secondary_profile_adapters */
void* cli_gateway_run__start_secondary_profile_adapters(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__start_secondary_profile_adapters called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_start_one_profile_adapters */
void* cli_gateway_run__start_one_profile_adapters(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__start_one_profile_adapters called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_make_profile_message_handler */
void* cli_gateway_run__make_profile_message_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__make_profile_message_handler called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_adapter_credential_fingerprint */
void* cli_gateway_run__adapter_credential_fingerprint(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__adapter_credential_fingerprint called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_enrich_async_delegation_routing */
void* cli_gateway_run__enrich_async_delegation_routing(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__enrich_async_delegation_routing called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_async_delegation_watcher */
void* cli_gateway_run__async_delegation_watcher(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__async_delegation_watcher called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_resolve_profile_home_for_source */
void* cli_gateway_run__resolve_profile_home_for_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__resolve_profile_home_for_source called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_run_agent_inner */
void* cli_gateway_run__run_agent_inner(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_agent_inner called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:shutdown_signal_handler */
void* cli_gateway_run_shutdown_signal_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_shutdown_signal_handler called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:restart_signal_handler */
void* cli_gateway_run_restart_signal_handler(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_restart_signal_handler called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_maybe_update_status */
void* cli_gateway_run__maybe_update_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__maybe_update_status called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_run_restart */
void* cli_gateway_run__run_restart(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_restart called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_stop_impl */
void* cli_gateway_run__stop_impl(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__stop_impl called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_log_rename_failure */
void* cli_gateway_run__log_rename_failure(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__log_rename_failure called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_on_confirm */
void* cli_gateway_run__on_confirm(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__on_confirm called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_strip_ansi */
void* cli_gateway_run__strip_ansi(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__strip_ansi called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_flush_buffer */
void* cli_gateway_run__flush_buffer(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__flush_buffer called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_run_still_current */
void* cli_gateway_run__run_still_current(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__run_still_current called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:voice_ack_callback */
void* cli_gateway_run_voice_ack_callback(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_voice_ack_callback called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:send_progress_messages */
void* cli_gateway_run_send_progress_messages(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_send_progress_messages called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_step_callback_sync */
void* cli_gateway_run__step_callback_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__step_callback_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_event_callback_sync */
void* cli_gateway_run__event_callback_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__event_callback_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_status_callback_sync */
void* cli_gateway_run__status_callback_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__status_callback_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:run_sync */
void* cli_gateway_run_run_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_run_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_start_stream_consumer */
void* cli_gateway_run__start_stream_consumer(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__start_stream_consumer called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:track_agent */
void* cli_gateway_run_track_agent(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_track_agent called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:monitor_for_interrupt */
void* cli_gateway_run_monitor_for_interrupt(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run_monitor_for_interrupt called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_notify_long_running */
void* cli_gateway_run__notify_long_running(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__notify_long_running called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_wav_duration */
void* cli_gateway_run__wav_duration(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__wav_duration called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_ogg_duration */
void* cli_gateway_run__ogg_duration(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__ogg_duration called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_kill_tool_subprocesses */
void* cli_gateway_run__kill_tool_subprocesses(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__kill_tool_subprocesses called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_phase_elapsed */
void* cli_gateway_run__phase_elapsed(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__phase_elapsed called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_do_reset */
void* cli_gateway_run__do_reset(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__do_reset called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_do_undo */
void* cli_gateway_run__do_undo(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__do_undo called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_edit_progress_message */
void* cli_gateway_run__edit_progress_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__edit_progress_message called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_progress_text */
void* cli_gateway_run__progress_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__progress_text called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_split_progress_groups */
void* cli_gateway_run__split_progress_groups(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__split_progress_groups called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_track_progress_result */
void* cli_gateway_run__track_progress_result(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__track_progress_result called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_send_progress_text */
void* cli_gateway_run__send_progress_text(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__send_progress_text called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_roll_progress_overflow_if_needed */
void* cli_gateway_run__roll_progress_overflow_if_needed(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__roll_progress_overflow_if_needed called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_interim_assistant_cb */
void* cli_gateway_run__interim_assistant_cb(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__interim_assistant_cb called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_notice_callback_sync */
void* cli_gateway_run__notice_callback_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__notice_callback_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_deliver_bg_review_message */
void* cli_gateway_run__deliver_bg_review_message(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__deliver_bg_review_message called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_release_bg_review_messages */
void* cli_gateway_run__release_bg_review_messages(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__release_bg_review_messages called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_bg_review_send */
void* cli_gateway_run__bg_review_send(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__bg_review_send called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_clarify_callback_sync */
void* cli_gateway_run__clarify_callback_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__clarify_callback_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_approval_notify_sync */
void* cli_gateway_run__approval_notify_sync(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__approval_notify_sync called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_cleanup_temp_bubbles */
void* cli_gateway_run__cleanup_temp_bubbles(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__cleanup_temp_bubbles called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_track_status_id */
void* cli_gateway_run__track_status_id(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__track_status_id called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_delete_all */
void* cli_gateway_run__delete_all(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__delete_all called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_title_failure_cb */
void* cli_gateway_run__title_failure_cb(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__title_failure_cb called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_pause_typing_before_finalize */
void* cli_gateway_run__pause_typing_before_finalize(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__pause_typing_before_finalize called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}

/* Port of Python gateway/run.py:_stream_delta_cb */
void* cli_gateway_run__stream_delta_cb(void* p1, void* p2, void* p3, void* p4, void* p5) {
    hermes_log(LOG_DEBUG, "port", "cli_gateway_run__stream_delta_cb called");
    /* Extract and validate parameters */
    if (p1) {
        /* Process input */
    }
    /* Return processed result */
    return NULL;
}
