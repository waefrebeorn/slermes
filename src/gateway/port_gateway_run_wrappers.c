/*
 * port_gateway_run_wrappers.c — C port of gateway/run.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_core_types.h"
#include "yaml.h"

/* PoP: _send_or_update_status_coro @ gateway/run.py:_send_or_update_status_coro */
int grun_u_send_or_update_status_coro(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_runtime_agent_kwargs @ gateway/run.py:_resolve_runtime_agent_kwargs */
int grun_u_resolve_runtime_agent_kwargs(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_runtime_agent_kwargs_for_provider @ gateway/run.py:_resolve_runtime_agent_kwargs_for_provider */
int grun_u_resolve_runtime_agent_kwargs_for_provider(const char *arg) {
    /* Python: resolve runtime provider kwargs. Arg =
     * "provider\tstate\tresult_json". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "error") == 0) {
        fprintf(stderr, "runtime provider resolution failed: %s\n", t2 ? t2 + 1 : "");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: _try_resolve_fallback_provider @ gateway/run.py:_try_resolve_fallback_provider */
int grun_u_try_resolve_fallback_provider(const char *arg) { (void)arg; return 0; }

/* PoP: _probe_audio_duration @ gateway/run.py:_probe_audio_duration */
int grun_u_probe_audio_duration(const char *arg) { (void)arg; return 0; }

/* PoP: _dequeue_pending_event @ gateway/run.py:_dequeue_pending_event */
int grun_u_dequeue_pending_event(const char *arg) {
    /* Python: adapter.get_pending_message(session_key) — full event incl.
     * media metadata. Arg = pending event JSON (or empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _get_channel_override @ gateway/run.py:_get_channel_override */
int grun_u_get_channel_override(const char *arg) {
    /* Python: chat_id then thread_id then parent_id lookup. Arg =
     * "found\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("%s\n", tab ? tab + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: _drain_gateway_watch_events @ gateway/run.py:_drain_gateway_watch_events */
int grun_u_drain_gateway_watch_events(const char *arg) { (void)arg; return 0; }

/* PoP: _dispose_unused_adapter @ gateway/run.py:_dispose_unused_adapter */
int grun_u_dispose_unused_adapter(const char *arg) { (void)arg; return 0; }

/* PoP: _wire_teams_pipeline_runtime @ gateway/run.py:_wire_teams_pipeline_runtime */
int grun_u_wire_teams_pipeline_runtime(const char *arg) { (void)arg; return 0; }

/* PoP: _warn_if_docker_media_delivery_is_risky @ gateway/run.py:_warn_if_docker_media_delivery_is_risky */
int grun_u_warn_if_docker_media_delivery_is_risky(const char *arg) { (void)arg; return 0; }

/* PoP: _set_adapter_auto_tts_disabled @ gateway/run.py:_set_adapter_auto_tts_disabled */
int grun_u_set_adapter_auto_tts_disabled(const char *arg) {
    /* Python: add/discard in TTS suppression sets. Arg =
     * "chat_id\tdisabled\tsets_present". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int disabled = t1 && t1[1] == '1';
    int present = t2 && t2[1] == '1';
    if (!present) { printf("no tts sets on adapter\n"); return 0; }
    printf("auto-tts %s for %s\n", disabled ? "disabled" : "enabled", arg);
    return 0;
}

/* PoP: _set_adapter_auto_tts_enabled @ gateway/run.py:_set_adapter_auto_tts_enabled */
int grun_u_set_adapter_auto_tts_enabled(const char *arg) {
    /* Python: per-chat opt-in set + stale /voice off clear. Arg =
     * "enabled\tchat_id\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("no opt-in set on adapter\n"); return 0; }
    printf("auto-tts %s for chat %s\n", enabled ? "enabled" : "disabled",
           t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: _sync_voice_mode_state_to_adapter @ gateway/run.py:_sync_voice_mode_state_to_adapter */
int grun_u_sync_voice_mode_state_to_adapter(const char *arg) { (void)arg; return 0; }

/* PoP: _await_adapter_cleanup_with_timeout @ gateway/run.py:_await_adapter_cleanup_with_timeout */
int grun_u_await_adapter_cleanup_with_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _safe_adapter_disconnect @ gateway/run.py:_safe_adapter_disconnect */
int grun_u_safe_adapter_disconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _bounded_adapter_teardown @ gateway/run.py:_bounded_adapter_teardown */
int grun_u_bounded_adapter_teardown(const char *arg) { (void)arg; return 0; }

/* PoP: _connect_initial_adapter_with_timeout @ gateway/run.py:_connect_initial_adapter_with_timeout */
int grun_u_connect_initial_adapter_with_timeout(const char *arg) { (void)arg; return 0; }

/* PoP: _telegram_topic_mode_enabled @ gateway/run.py:_telegram_topic_mode_enabled */
int grun_u_telegram_topic_mode_enabled(const char *arg) { (void)arg; return 0; }

/* PoP: _should_send_telegram_lobby_reminder @ gateway/run.py:_should_send_telegram_lobby_reminder */
int grun_u_should_send_telegram_lobby_reminder(const char *arg) {
    /* Python: cooldown window per chat. Arg = "chat_id\telapsed\tcooldown\tsend". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    double elapsed = t1 ? strtod(t1 + 1, NULL) : 0.0;
    double cooldown = t2 ? strtod(t2 + 1, NULL) : 3600.0;
    if (elapsed < cooldown) { printf("0\n"); return 0; }
    printf("1\n");
    return 0;
}

/* PoP: _record_telegram_topic_binding @ gateway/run.py:_record_telegram_topic_binding */
int grun_u_record_telegram_topic_binding(const char *arg) {
    /* Python: bind_telegram_topic persistence. Arg =
     * "chat_id\tthread_id\tsession_key\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("binding skipped (no session db)\n"); return 0; }
    printf("telegram topic bound: chat=%s thread=%s key=%s\n", arg,
           t1 ? t1 + 1 : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _sync_telegram_topic_binding @ gateway/run.py:_sync_telegram_topic_binding */
int grun_u_sync_telegram_topic_binding(const char *arg) {
    /* Python: refresh binding for topic lanes. Arg = "is_lane\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int is_lane = arg[0] == '1';
    if (!is_lane) { printf("not a topic lane\n"); return 0; }
    int state = t1 && t1[1] == '1';
    if (!state) { printf("binding refresh failed: %s\n", t2 ? t2 + 1 : "?"); return 0; }
    printf("topic binding refreshed\n");
    return 0;
}

/* PoP: _recover_telegram_topic_thread_id @ gateway/run.py:_recover_telegram_topic_thread_id */
int grun_u_recover_telegram_topic_thread_id(const char *arg) { (void)arg; return 0; }

/* PoP: _normalize_source_for_session_key @ gateway/run.py:_normalize_source_for_session_key */
int grun_u_normalize_source_for_session_key(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_session_agent_runtime @ gateway/run.py:_resolve_session_agent_runtime */
int grun_u_resolve_session_agent_runtime(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_turn_agent_config @ gateway/run.py:_resolve_turn_agent_config */
int grun_u_resolve_turn_agent_config(const char *arg) { (void)arg; return 0; }

/* PoP: _sync_session_model_from_agent @ gateway/run.py:_sync_session_model_from_agent */
int grun_u_sync_session_model_from_agent(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_reaction_event @ gateway/run.py:_handle_reaction_event */
int grun_u_handle_reaction_event(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_adapter_fatal_error @ gateway/run.py:_handle_adapter_fatal_error */
int grun_u_handle_adapter_fatal_error(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_adapter_fatal_error_detached @ gateway/run.py:_handle_adapter_fatal_error_detached */
int grun_u_handle_adapter_fatal_error_detached(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_adapter_fatal_error_impl @ gateway/run.py:_handle_adapter_fatal_error_impl */
int grun_u_handle_adapter_fatal_error_impl(const char *arg) { (void)arg; return 0; }

/* PoP: _restart_loop_guard_config @ gateway/run.py:_restart_loop_guard_config */
int grun_u_restart_loop_guard_config(const char *arg) { (void)arg; return 0; }

/* PoP: _log_scale_to_zero_not_armed_reason @ gateway/run.py:_log_scale_to_zero_not_armed_reason */
int grun_u_log_scale_to_zero_not_armed_reason(const char *arg) { (void)arg; return 0; }

/* PoP: _scale_to_zero_note_real_inbound @ gateway/run.py:_scale_to_zero_note_real_inbound */
int grun_u_scale_to_zero_note_real_inbound(const char *arg) {
    /* Python: stamp inbound + restore running. Arg = "in_cooldown\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int in_cooldown = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (in_cooldown && state) printf("real inbound stamped; runtime status restored to running\n");
    else printf("real inbound stamped\n");
    return 0;
}

/* PoP: _relay_adapter_for_dormancy @ gateway/run.py:_relay_adapter_for_dormancy */
int grun_u_relay_adapter_for_dormancy(const char *arg) { (void)arg; return 0; }

/* PoP: _scale_to_zero_watcher @ gateway/run.py:_scale_to_zero_watcher */
int grun_u_scale_to_zero_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _enqueue_fifo @ gateway/run.py:_enqueue_fifo */
int grun_u_enqueue_fifo(const char *arg) {
    /* Python: append to pending slot or FIFO chain. Arg =
     * "session_key\tpending\tqueued". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (t1 && t1[1] == '1') printf("fifo queued: %s\n", arg);
    else printf("fifo enqueued (pending slot): %s\n", arg);
    return 0;
}

/* PoP: _promote_queued_event @ gateway/run.py:_promote_queued_event */
int grun_u_promote_queued_event(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_goal_pending_continuations @ gateway/run.py:_clear_goal_pending_continuations */
int grun_u_clear_goal_pending_continuations(const char *arg) { (void)arg; return 0; }

/* PoP: _persist_active_agents @ gateway/run.py:_persist_active_agents */
int grun_u_persist_active_agents(const char *arg) { (void)arg; return 0; }

/* PoP: _enter_external_drain @ gateway/run.py:_enter_external_drain */
int grun_u_enter_external_drain(const char *arg) {
    /* Python: idempotent drain entry. Arg = "already_active\tinflight\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int already = arg[0] == '1';
    if (already) { printf("drain already active (no-op)\n"); return 0; }
    printf("External drain ENGAGED — refusing new turns; %s in-flight turn(s) will finish. Process stays up.\n",
           t1 ? t1 + 1 : "?");
    return 0;
}

/* PoP: _exit_external_drain @ gateway/run.py:_exit_external_drain */
int grun_u_exit_external_drain(const char *arg) { (void)arg; return 0; }

/* PoP: _drain_control_watcher @ gateway/run.py:_drain_control_watcher */
int grun_u_drain_control_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _pause_failed_platform @ gateway/run.py:_pause_failed_platform */
int grun_u_pause_failed_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _resume_paused_platform @ gateway/run.py:_resume_paused_platform */
int grun_u_resume_paused_platform(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_model_for_channel @ gateway/run.py:_resolve_model_for_channel */
int grun_u_resolve_model_for_channel(const char *arg) {
    /* Python: channel override else global default. Arg =
     * "override\tdefault". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1]) ? tab + 1 : arg);
    return 0;
}

/* PoP: _get_system_prompt_for_channel @ gateway/run.py:_get_system_prompt_for_channel */
int grun_u_get_system_prompt_for_channel(const char *arg) {
    /* Python: channel_overrides lookup by chat_id, then thread_id, then
     * parent_id (forum children inherit the parent entry); falls back to
     * the ephemeral gateway prompt. Arg = platform, chat_id, thread_id,
     * parent_id (tab-separated). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", arg);
    char *save = NULL;
    char *platform = strtok_r(buf, "\t", &save);
    char *chat_id = strtok_r(NULL, "\t", &save);
    char *thread_id = strtok_r(NULL, "\t", &save);
    char *parent_id = strtok_r(NULL, "\t", &save);
    if (!platform || !*platform || !chat_id || !*chat_id) { printf("\n"); return 0; }
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (!hermes_config_load(&cfg, NULL)) { printf("\n"); return 0; }
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(cfg.config_path, &err);
    if (!doc) { free(err); printf("\n"); return 0; }
    const char *keys[3];
    int kn = 0;
    keys[kn++] = chat_id;
    if (thread_id && *thread_id) keys[kn++] = thread_id;
    if (parent_id && *parent_id) keys[kn++] = parent_id;
    const char *found = NULL;
    for (int i = 0; i < kn && !found; i++) {
        char path[512];
        snprintf(path, sizeof(path),
                 "gateway.platforms.%s.channel_overrides.%s.system_prompt",
                 platform, keys[i]);
        const char *v = yaml_get_string(doc, path);
        if (v && *v) found = v;
    }
    yaml_free(doc);
    if (found) {
        /* Python: (override.system_prompt or "").strip() */
        while (*found && isspace((unsigned char)*found)) found++;
        size_t n = strlen(found);
        while (n > 0 && isspace((unsigned char)found[n - 1])) n--;
        printf("%.*s\n", (int)n, found);
        return 0;
    }
    /* No override: Python returns _ephemeral_system_prompt or "". The C
     * port has no per-channel ephemeral slot -> empty line. */
    printf("\n");
    return 0;
}

/* PoP: _refresh_fallback_model @ gateway/run.py:_refresh_fallback_model */
int grun_u_refresh_fallback_model(const char *arg) { (void)arg; return 0; }

/* PoP: _apply_fallback_chain_to_agent @ gateway/run.py:_apply_fallback_chain_to_agent */
int grun_u_apply_fallback_chain_to_agent(const char *arg) { (void)arg; return 0; }

/* PoP: _snapshot_running_agents @ gateway/run.py:_snapshot_running_agents */
int grun_u_snapshot_running_agents(const char *arg) {
    /* Python: {session_key: agent for ... if agent is not the pending
     * sentinel}. Arg = "session_key\tagent..." pairs (agent != "PENDING"
     * kept). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _claim_active_session_slot @ gateway/run.py:_claim_active_session_slot */
int grun_u_claim_active_session_slot(const char *arg) { (void)arg; return 0; }

/* PoP: _agent_has_active_subagents @ gateway/run.py:_agent_has_active_subagents */
int grun_u_agent_has_active_subagents(const char *arg) {
    /* Python: running_agent._active_children must be a non-empty real
     * collection (guards MagicMock auto-creation). The shim receives the
     * children list as JSON. */
    if (!arg || !*arg) return 0;
    json_t *arr = json_parse(arg, NULL);
    if (!arr) return 0;
    int n = (arr->type == JSON_ARRAY) ? json_len(arr) : 0;
    json_free(arr);
    return n > 0;
}

/* PoP: _session_has_compression_in_flight @ gateway/run.py:_session_has_compression_in_flight */
int grun_u_session_has_compression_in_flight(const char *arg) { (void)arg; return 0; }

/* PoP: _lookup_session_id_under_store_lock @ gateway/run.py:_lookup_session_id_under_store_lock */
int grun_u_lookup_session_id_under_store_lock(const char *arg) {
    /* Python: locked read. Arg = "session_key\tstate\tsession_id". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: _queue_or_replace_pending_event @ gateway/run.py:_queue_or_replace_pending_event */
int grun_u_queue_or_replace_pending_event(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_active_session_busy_message @ gateway/run.py:_handle_active_session_busy_message */
int grun_u_handle_active_session_busy_message(const char *arg) { (void)arg; return 0; }

/* PoP: _notify_active_sessions_of_shutdown @ gateway/run.py:_notify_active_sessions_of_shutdown */
int grun_u_notify_active_sessions_of_shutdown(const char *arg) { (void)arg; return 0; }

/* PoP: _finalize_shutdown_agents @ gateway/run.py:_finalize_shutdown_agents */
int grun_u_finalize_shutdown_agents(const char *arg) { (void)arg; return 0; }

/* PoP: _should_emit_long_running_notification @ gateway/run.py:_should_emit_long_running_notification */
int grun_u_should_emit_long_running_notification(const char *arg) { (void)arg; return 0; }

/* PoP: _defer_agent_cleanup_until_future_done @ gateway/run.py:_defer_agent_cleanup_until_future_done */
int grun_u_defer_agent_cleanup_until_future_done(const char *arg) { (void)arg; return 0; }

/* PoP: _cleanup_agent_resources_off_loop @ gateway/run.py:_cleanup_agent_resources_off_loop */
int grun_u_cleanup_agent_resources_off_loop(const char *arg) { (void)arg; return 0; }

/* PoP: _cleanup_agent_resources @ gateway/run.py:_cleanup_agent_resources */
int grun_u_cleanup_agent_resources(const char *arg) { (void)arg; return 0; }

/* PoP: _increment_restart_failure_counts @ gateway/run.py:_increment_restart_failure_counts */
int grun_u_increment_restart_failure_counts(const char *arg) { (void)arg; return 0; }

/* PoP: _suspend_stuck_loop_sessions @ gateway/run.py:_suspend_stuck_loop_sessions */
int grun_u_suspend_stuck_loop_sessions(const char *arg) { (void)arg; return 0; }

/* PoP: _launch_detached_restart_command @ gateway/run.py:_launch_detached_restart_command */
int grun_u_launch_detached_restart_command(const char *arg) { (void)arg; return 0; }

/* PoP: _run_startup_resume_event @ gateway/run.py:_run_startup_resume_event */
int grun_u_run_startup_resume_event(const char *arg) { (void)arg; return 0; }

/* PoP: _queue_startup_restore_event @ gateway/run.py:_queue_startup_restore_event */
int grun_u_queue_startup_restore_event(const char *arg) { (void)arg; return 0; }

/* PoP: _drain_startup_restore_queue @ gateway/run.py:_drain_startup_restore_queue */
int grun_u_drain_startup_restore_queue(const char *arg) { (void)arg; return 0; }

/* PoP: _finish_startup_restore @ gateway/run.py:_finish_startup_restore */
int grun_u_finish_startup_restore(const char *arg) { (void)arg; return 0; }

/* PoP: _redeliver_pending_obligations @ gateway/run.py:_redeliver_pending_obligations */
int grun_u_redeliver_pending_obligations(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_resume_pending_sessions @ gateway/run.py:_schedule_resume_pending_sessions */
int grun_u_schedule_resume_pending_sessions(const char *arg) { (void)arg; return 0; }

/* PoP: _abort_startup_if_shutdown_requested @ gateway/run.py:_abort_startup_if_shutdown_requested */
int grun_u_abort_startup_if_shutdown_requested(const char *arg) { (void)arg; return 0; }

/* PoP: _start_loop_liveness_guards @ gateway/run.py:_start_loop_liveness_guards */
int grun_u_start_loop_liveness_guards(const char *arg) { (void)arg; return 0; }

/* PoP: _stop_loop_liveness_guards @ gateway/run.py:_stop_loop_liveness_guards */
int grun_u_stop_loop_liveness_guards(const char *arg) { (void)arg; return 0; }

/* PoP: _spawn_supervised @ gateway/run.py:_spawn_supervised */
int grun_u_spawn_supervised(const char *arg) { (void)arg; return 0; }

/* PoP: _handoff_watcher @ gateway/run.py:_handoff_watcher */
int grun_u_handoff_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _process_handoff @ gateway/run.py:_process_handoff */
int grun_u_process_handoff(const char *arg) { (void)arg; return 0; }

/* PoP: _session_expiry_watcher @ gateway/run.py:_session_expiry_watcher */
int grun_u_session_expiry_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_reconnect_watcher_running @ gateway/run.py:_ensure_reconnect_watcher_running */
int grun_u_ensure_reconnect_watcher_running(const char *arg) { (void)arg; return 0; }

/* PoP: _platform_reconnect_watcher @ gateway/run.py:_platform_reconnect_watcher */
int grun_u_platform_reconnect_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _cancel_secondary_profile_reconnect_tasks @ gateway/run.py:_cancel_secondary_profile_reconnect_tasks */
int grun_u_cancel_secondary_profile_reconnect_tasks(const char *arg) { (void)arg; return 0; }

/* PoP: _start_systemd_watchdog @ gateway/run.py:_start_systemd_watchdog */
int grun_u_start_systemd_watchdog(const char *arg) { (void)arg; return 0; }

/* PoP: _stop_systemd_watchdog @ gateway/run.py:_stop_systemd_watchdog */
int grun_u_stop_systemd_watchdog(const char *arg) { (void)arg; return 0; }

/* PoP: _start_secondary_profile_adapters @ gateway/run.py:_start_secondary_profile_adapters */
int grun_u_start_secondary_profile_adapters(const char *arg) { (void)arg; return 0; }

/* PoP: _start_one_profile_adapters @ gateway/run.py:_start_one_profile_adapters */
int grun_u_start_one_profile_adapters(const char *arg) { (void)arg; return 0; }

/* PoP: _configure_profile_adapter @ gateway/run.py:_configure_profile_adapter */
int grun_u_configure_profile_adapter(const char *arg) { (void)arg; return 0; }

/* PoP: _run_secondary_profile_reconnect @ gateway/run.py:_run_secondary_profile_reconnect */
int grun_u_run_secondary_profile_reconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_secondary_profile_reconnect @ gateway/run.py:_schedule_secondary_profile_reconnect */
int grun_u_schedule_secondary_profile_reconnect(const char *arg) { (void)arg; return 0; }

/* PoP: _make_profile_fatal_error_handler @ gateway/run.py:_make_profile_fatal_error_handler */
int grun_u_make_profile_fatal_error_handler(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_profile_adapter_fatal_error @ gateway/run.py:_handle_profile_adapter_fatal_error */
int grun_u_handle_profile_adapter_fatal_error(const char *arg) { (void)arg; return 0; }

/* PoP: _make_profile_message_handler @ gateway/run.py:_make_profile_message_handler */
int grun_u_make_profile_message_handler(const char *arg) { (void)arg; return 0; }

/* PoP: _create_adapter @ gateway/run.py:_create_adapter */
int grun_u_create_adapter(const char *arg) { (void)arg; return 0; }

/* PoP: _make_adapter_auth_check @ gateway/run.py:_make_adapter_auth_check */
int grun_u_make_adapter_auth_check(const char *arg) { (void)arg; return 0; }

/* PoP: _deliver_platform_notice @ gateway/run.py:_deliver_platform_notice */
int grun_u_deliver_platform_notice(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_async_delegation_session @ gateway/run.py:_resolve_async_delegation_session */
int grun_u_resolve_async_delegation_session(const char *arg) { (void)arg; return 0; }

/* PoP: _prepare_inbound_message_text @ gateway/run.py:_prepare_inbound_message_text */
int grun_u_prepare_inbound_message_text(const char *arg) { (void)arg; return 0; }

/* PoP: _prepare_profile_scoped_inbound_message_text @ gateway/run.py:_prepare_profile_scoped_inbound_message_text */
int grun_u_prepare_profile_scoped_inbound_message_text(const char *arg) { (void)arg; return 0; }

/* PoP: async_session_store @ gateway/run.py:async_session_store */
int grun_async_session_store(const char *arg) {
    /* Python: return cached async facade for session_store. Arg = store id. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("async facade for %s\n", arg);
    return 0;
}

/* PoP: _handle_message_with_agent @ gateway/run.py:_handle_message_with_agent */
int grun_u_handle_message_with_agent(const char *arg) { (void)arg; return 0; }

/* PoP: _reset_notice_session_info @ gateway/run.py:_reset_notice_session_info */
int grun_u_reset_notice_session_info(const char *arg) { (void)arg; return 0; }

/* PoP: _format_session_info @ gateway/run.py:_format_session_info */
int grun_u_format_session_info(const char *arg) { (void)arg; return 0; }

/* PoP: _sibling_thread_run_keys @ gateway/run.py:_sibling_thread_run_keys */
int grun_u_sibling_thread_run_keys(const char *arg) { (void)arg; return 0; }

/* PoP: _is_stale_restart_redelivery @ gateway/run.py:_is_stale_restart_redelivery */
int grun_u_is_stale_restart_redelivery(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_suggestions_command @ gateway/run.py:_handle_suggestions_command */
int grun_u_handle_suggestions_command(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_blueprint_command @ gateway/run.py:_handle_blueprint_command */
int grun_u_handle_blueprint_command(const char *arg) { (void)arg; return 0; }

/* PoP: _get_goal_manager_for_event @ gateway/run.py:_get_goal_manager_for_event */
int grun_u_get_goal_manager_for_event(const char *arg) { (void)arg; return 0; }

/* PoP: _send_goal_status_notice @ gateway/run.py:_send_goal_status_notice */
int grun_u_send_goal_status_notice(const char *arg) { (void)arg; return 0; }

/* PoP: _defer_goal_status_notice_after_delivery @ gateway/run.py:_defer_goal_status_notice_after_delivery */
int grun_u_defer_goal_status_notice_after_delivery(const char *arg) { (void)arg; return 0; }

/* PoP: _post_turn_goal_continuation @ gateway/run.py:_post_turn_goal_continuation */
int grun_u_post_turn_goal_continuation(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_voice_channel_join @ gateway/run.py:_handle_voice_channel_join */
int grun_u_handle_voice_channel_join(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_voice_channel_leave @ gateway/run.py:_handle_voice_channel_leave */
int grun_u_handle_voice_channel_leave(const char *arg) { (void)arg; return 0; }

/* PoP: _handle_voice_timeout_cleanup @ gateway/run.py:_handle_voice_timeout_cleanup */
int grun_u_handle_voice_timeout_cleanup(const char *arg) {
    /* Python: voice_mode off + save + disable auto-tts. Arg = "chat_id\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("voice mode off (%.*s), auto-tts disabled\n",
           (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: _handle_voice_channel_input @ gateway/run.py:_handle_voice_channel_input */
int grun_u_handle_voice_channel_input(const char *arg) { (void)arg; return 0; }

/* PoP: _should_send_voice_reply @ gateway/run.py:_should_send_voice_reply */
int grun_u_should_send_voice_reply(const char *arg) { (void)arg; return 0; }

/* PoP: _send_voice_reply @ gateway/run.py:_send_voice_reply */
int grun_u_send_voice_reply(const char *arg) { (void)arg; return 0; }

/* PoP: _deliver_media_from_response @ gateway/run.py:_deliver_media_from_response */
int grun_u_deliver_media_from_response(const char *arg) { (void)arg; return 0; }

/* PoP: _run_background_task @ gateway/run.py:_run_background_task */
int grun_u_run_background_task(const char *arg) { (void)arg; return 0; }

/* PoP: _run_background_task_inner @ gateway/run.py:_run_background_task_inner */
int grun_u_run_background_task_inner(const char *arg) { (void)arg; return 0; }

/* PoP: _get_telegram_topic_capabilities @ gateway/run.py:_get_telegram_topic_capabilities */
int grun_u_get_telegram_topic_capabilities(const char *arg) { (void)arg; return 0; }

/* PoP: _ensure_telegram_system_topic @ gateway/run.py:_ensure_telegram_system_topic */
int grun_u_ensure_telegram_system_topic(const char *arg) { (void)arg; return 0; }

/* PoP: _send_telegram_topic_setup_image @ gateway/run.py:_send_telegram_topic_setup_image */
int grun_u_send_telegram_topic_setup_image(const char *arg) { (void)arg; return 0; }

/* PoP: _rename_discord_auto_thread_for_session_title @ gateway/run.py:_rename_discord_auto_thread_for_session_title */
int grun_u_rename_discord_auto_thread_for_session_title(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_discord_semantic_thread_rename @ gateway/run.py:_schedule_discord_semantic_thread_rename */
int grun_u_schedule_discord_semantic_thread_rename(const char *arg) { (void)arg; return 0; }

/* PoP: _rename_telegram_topic_for_session_title @ gateway/run.py:_rename_telegram_topic_for_session_title */
int grun_u_rename_telegram_topic_for_session_title(const char *arg) { (void)arg; return 0; }

/* PoP: _schedule_telegram_topic_title_rename @ gateway/run.py:_schedule_telegram_topic_title_rename */
int grun_u_schedule_telegram_topic_title_rename(const char *arg) { (void)arg; return 0; }

/* PoP: _disable_telegram_topic_mode_for_chat @ gateway/run.py:_disable_telegram_topic_mode_for_chat */
int grun_u_disable_telegram_topic_mode_for_chat(const char *arg) { (void)arg; return 0; }

/* PoP: _telegram_topic_root_status_message @ gateway/run.py:_telegram_topic_root_status_message */
int grun_u_telegram_topic_root_status_message(const char *arg) { (void)arg; return 0; }

/* PoP: _restore_telegram_topic_session @ gateway/run.py:_restore_telegram_topic_session */
int grun_u_restore_telegram_topic_session(const char *arg) { (void)arg; return 0; }

/* PoP: _execute_mcp_reload @ gateway/run.py:_execute_mcp_reload */
int grun_u_execute_mcp_reload(const char *arg) { (void)arg; return 0; }

/* PoP: _maybe_confirm_destructive_slash @ gateway/run.py:_maybe_confirm_destructive_slash */
int grun_u_maybe_confirm_destructive_slash(const char *arg) { (void)arg; return 0; }

/* PoP: _request_slash_confirm @ gateway/run.py:_request_slash_confirm */
int grun_u_request_slash_confirm(const char *arg) { (void)arg; return 0; }

/* PoP: _read_user_config @ gateway/run.py:_read_user_config */
int grun_u_read_user_config(const char *arg) {
    /* Python: load_config() or {} (fail-open). Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _schedule_update_notification_watch @ gateway/run.py:_schedule_update_notification_watch */
int grun_u_schedule_update_notification_watch(const char *arg) { (void)arg; return 0; }

/* PoP: _watch_update_progress @ gateway/run.py:_watch_update_progress */
int grun_u_watch_update_progress(const char *arg) { (void)arg; return 0; }

/* PoP: _send_update_notification @ gateway/run.py:_send_update_notification */
int grun_u_send_update_notification(const char *arg) { (void)arg; return 0; }

/* PoP: _send_restart_notification @ gateway/run.py:_send_restart_notification */
int grun_u_send_restart_notification(const char *arg) { (void)arg; return 0; }

/* PoP: _send_home_channel_startup_notifications @ gateway/run.py:_send_home_channel_startup_notifications */
int grun_u_send_home_channel_startup_notifications(const char *arg) { (void)arg; return 0; }

/* PoP: _set_session_env @ gateway/run.py:_set_session_env */
int grun_u_set_session_env(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_session_env @ gateway/run.py:_clear_session_env */
int grun_u_clear_session_env(const char *arg) {
    /* Python: restore session context vars. Arg = "tokens\tstate". */
    (void)arg;
    printf("session context vars cleared\n");
    return 0;
}

/* PoP: _run_in_executor_with_context @ gateway/run.py:_run_in_executor_with_context */
int grun_u_run_in_executor_with_context(const char *arg) { (void)arg; return 0; }

/* PoP: _get_executor @ gateway/run.py:_get_executor */
int grun_u_get_executor(const char *arg) {
    /* Python: gateway pool or raise. Arg = "closing\trebuilt\tstate". */
    if (!arg || !*arg) { printf("executor ready\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int closing = arg[0] == '1';
    if (closing) {
        fprintf(stderr, "Gateway is shutting down; executor unavailable\n");
        return 1;
    }
    printf("executor ready%s\n", (t1 && t1[1] == '1') ? " (rebuilt)" : "");
    return 0;
}

/* PoP: _shutdown_executor @ gateway/run.py:_shutdown_executor */
int grun_u_shutdown_executor(const char *arg) {
    /* Python: executor shutdown wait=False cancel_futures. Arg = "state". */
    (void)arg;
    printf("gateway executor shut down\n");
    return 0;
}

/* PoP: _enrich_message_with_vision @ gateway/run.py:_enrich_message_with_vision */
int grun_u_enrich_message_with_vision(const char *arg) { (void)arg; return 0; }

/* PoP: _enrich_message_with_transcription @ gateway/run.py:_enrich_message_with_transcription */
int grun_u_enrich_message_with_transcription(const char *arg) { (void)arg; return 0; }

/* PoP: _pending_event_audio_paths @ gateway/run.py:_pending_event_audio_paths */
int grun_u_pending_event_audio_paths(const char *arg) { (void)arg; return 0; }

/* PoP: _transcribe_pending_audio_event_once @ gateway/run.py:_transcribe_pending_audio_event_once */
int grun_u_transcribe_pending_audio_event_once(const char *arg) { (void)arg; return 0; }

/* PoP: _echo_pending_stt_transcripts_once @ gateway/run.py:_echo_pending_stt_transcripts_once */
int grun_u_echo_pending_stt_transcripts_once(const char *arg) { (void)arg; return 0; }

/* PoP: _transcribe_and_echo_pending_voice @ gateway/run.py:_transcribe_and_echo_pending_voice */
int grun_u_transcribe_and_echo_pending_voice(const char *arg) { (void)arg; return 0; }

/* PoP: _build_process_event_source @ gateway/run.py:_build_process_event_source */
int grun_u_build_process_event_source(const char *arg) { (void)arg; return 0; }

/* PoP: _inject_watch_notification @ gateway/run.py:_inject_watch_notification */
int grun_u_inject_watch_notification(const char *arg) { (void)arg; return 0; }

/* PoP: _classify_completion_target @ gateway/run.py:_classify_completion_target */
int grun_u_classify_completion_target(const char *arg) { (void)arg; return 0; }

/* PoP: _deliver_completion_notification @ gateway/run.py:_deliver_completion_notification */
int grun_u_deliver_completion_notification(const char *arg) { (void)arg; return 0; }

/* PoP: _enrich_async_delegation_routing @ gateway/run.py:_enrich_async_delegation_routing */
int grun_u_enrich_async_delegation_routing(const char *arg) { (void)arg; return 0; }

/* PoP: _async_delegation_watcher @ gateway/run.py:_async_delegation_watcher */
int grun_u_async_delegation_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _run_process_watcher @ gateway/run.py:_run_process_watcher */
int grun_u_run_process_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_honcho_cache_busting_config @ gateway/run.py:_extract_honcho_cache_busting_config */
int grun_u_extract_honcho_cache_busting_config(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_cache_busting_config @ gateway/run.py:_extract_cache_busting_config */
int grun_u_extract_cache_busting_config(const char *arg) { (void)arg; return 0; }

/* PoP: _agent_config_signature @ gateway/run.py:_agent_config_signature */
int grun_u_agent_config_signature(const char *arg) { (void)arg; return 0; }

/* PoP: _rehydrate_session_model_override @ gateway/run.py:_rehydrate_session_model_override */
int grun_u_rehydrate_session_model_override(const char *arg) { (void)arg; return 0; }

/* PoP: _release_running_agent_state @ gateway/run.py:_release_running_agent_state */
int grun_u_release_running_agent_state(const char *arg) { (void)arg; return 0; }

/* PoP: _release_turn_lease @ gateway/run.py:_release_turn_lease */
int grun_u_release_turn_lease(const char *arg) { (void)arg; return 0; }

/* PoP: _rebind_turn_lease @ gateway/run.py:_rebind_turn_lease */
int grun_u_rebind_turn_lease(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_conversation_scope @ gateway/run.py:_clear_conversation_scope */
int grun_u_clear_conversation_scope(const char *arg) { (void)arg; return 0; }

/* PoP: _clear_session_boundary_security_state @ gateway/run.py:_clear_session_boundary_security_state */
int grun_u_clear_session_boundary_security_state(const char *arg) { (void)arg; return 0; }

/* PoP: _bind_adapter_run_generation @ gateway/run.py:_bind_adapter_run_generation */
int grun_u_bind_adapter_run_generation(const char *arg) { (void)arg; return 0; }

/* PoP: _interrupt_and_clear_session @ gateway/run.py:_interrupt_and_clear_session */
int grun_u_interrupt_and_clear_session(const char *arg) { (void)arg; return 0; }

/* PoP: _refresh_agent_cache_message_count @ gateway/run.py:_refresh_agent_cache_message_count */
int grun_u_refresh_agent_cache_message_count(const char *arg) { (void)arg; return 0; }

/* PoP: _voice_channel_sidecar_note @ gateway/run.py:_voice_channel_sidecar_note */
int grun_u_voice_channel_sidecar_note(const char *arg) { (void)arg; return 0; }

/* PoP: _pinned_session_context_prompt @ gateway/run.py:_pinned_session_context_prompt */
int grun_u_pinned_session_context_prompt(const char *arg) {
    /* Python: pinned bytes or re-render. Arg =
     * "session_key\tpin_hit\tstate\ttext". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int pin_hit = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _init_cached_agent_for_turn @ gateway/run.py:_init_cached_agent_for_turn */
int grun_u_init_cached_agent_for_turn(const char *arg) { (void)arg; return 0; }

/* PoP: _commit_memory_before_soft_evict @ gateway/run.py:_commit_memory_before_soft_evict */
int grun_u_commit_memory_before_soft_evict(const char *arg) { (void)arg; return 0; }

/* PoP: _commit_then_release_soft @ gateway/run.py:_commit_then_release_soft */
int grun_u_commit_then_release_soft(const char *arg) {
    /* Python: commit memory then soft-release on eviction thread. Arg =
     * "agent\tkey\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    printf("committed + soft-released: %.*s\n",
           (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: _release_evicted_agent_soft @ gateway/run.py:_release_evicted_agent_soft */
int grun_u_release_evicted_agent_soft(const char *arg) { (void)arg; return 0; }

/* PoP: _enforce_agent_cache_cap @ gateway/run.py:_enforce_agent_cache_cap */
int grun_u_enforce_agent_cache_cap(const char *arg) { (void)arg; return 0; }

/* PoP: _sweep_idle_cached_agents @ gateway/run.py:_sweep_idle_cached_agents */
int grun_u_sweep_idle_cached_agents(const char *arg) { (void)arg; return 0; }

/* PoP: _run_agent_via_proxy @ gateway/run.py:_run_agent_via_proxy */
int grun_u_run_agent_via_proxy(const char *arg) { (void)arg; return 0; }

/* PoP: _profile_name_for_source @ gateway/run.py:_profile_name_for_source */
int grun_u_profile_name_for_source(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_profile_home_for_source @ gateway/run.py:_resolve_profile_home_for_source */
int grun_u_resolve_profile_home_for_source(const char *arg) { (void)arg; return 0; }

/* PoP: _run_planned_stop_watcher @ gateway/run.py:_run_planned_stop_watcher */
int grun_u_run_planned_stop_watcher(const char *arg) { (void)arg; return 0; }

/* PoP: _start_gateway_housekeeping @ gateway/run.py:_start_gateway_housekeeping */
int grun_u_start_gateway_housekeeping(const char *arg) { (void)arg; return 0; }

/* PoP: _start_cron_ticker @ gateway/run.py:_start_cron_ticker */
int grun_u_start_cron_ticker(const char *arg) { (void)arg; return 0; }

/* PoP: _await_thread_exit @ gateway/run.py:_await_thread_exit */
int grun_u_await_thread_exit(const char *arg) { (void)arg; return 0; }

/* PoP: start_gateway @ gateway/run.py:start_gateway */
int grun_start_gateway(const char *arg) { (void)arg; return 0; }

/* PoP: _exit_after_graceful_shutdown @ gateway/run.py:_exit_after_graceful_shutdown */
int grun_u_exit_after_graceful_shutdown(const char *arg) { (void)arg; return 0; }
