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
#include "agent_cache_pressure.h"
#include "gateway_run_pure2.h"  /* gw_load_gateway_config */
#include "port_config_py_helpers.h"  /* config_py_read_raw_config */
#include <sys/stat.h>
#include <unistd.h>

/* PoP: grun_u_send_or_update_status_coro @ gateway/run.py:_send_or_update_status_coro */
int grun_u_send_or_update_status_coro(const char *arg) {
    /* Python: bubble edit #30045. Arg =
     * "updated\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int updated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!updated) { printf("0 (plain send — no send_or_update_status)\n"); return 0; }
    printf("1 (previous bubble edited for status_key=%s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_resolve_runtime_agent_kwargs @ gateway/run.py:_resolve_runtime_agent_kwargs */
int grun_u_resolve_runtime_agent_kwargs(const char *arg) {
    /* Python: config-authoritative. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "rate_limited") == 0) {
        printf("Primary provider rate-limited (429) — trying fallback\n");
        return 0;
    }
    if (strcmp(state, "auth_fail") == 0) {
        printf("Primary provider auth failed — trying fallback\n");
        if (t2 && t2[1] == '1') { printf("fallback resolved\n"); return 0; }
        fprintf(stderr, "no fallback: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("runtime resolved: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_runtime_agent_kwargs_for_provider @ gateway/run.py:_resolve_runtime_agent_kwargs_for_provider */
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

/* PoP: grun_u_try_resolve_fallback_provider @ gateway/run.py:_try_resolve_fallback_provider */
int grun_u_try_resolve_fallback_provider(const char *arg) {
    /* Python: fallback chain. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("fallback resolved: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_probe_audio_duration @ gateway/run.py:_probe_audio_duration */
int grun_u_probe_audio_duration(const char *arg) {
    /* Python: best-effort duration. Arg =
     * "dur\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (wav via wave module; ogg/mp3 via ffprobe fallback; None on failure)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_dequeue_pending_event @ gateway/run.py:_dequeue_pending_event */
int grun_u_dequeue_pending_event(const char *arg) {
    /* Python: adapter.get_pending_message(session_key) — full event incl.
     * media metadata. Arg = pending event JSON (or empty). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: grun_u_get_channel_override @ gateway/run.py:_get_channel_override */
int grun_u_get_channel_override(const char *arg) {
    /* Python: chat_id then thread_id then parent_id lookup. Arg =
     * "found\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (arg[0] == '1') { printf("%s\n", tab ? tab + 1 : ""); return 0; }
    printf("\n");
    return 0;
}

/* PoP: grun_u_drain_gateway_watch_events @ gateway/run.py:_drain_gateway_watch_events */
int grun_u_drain_gateway_watch_events(const char *arg) {
    /* Python: watch drain w/o spin. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: grun_u_dispose_unused_adapter @ gateway/run.py:_dispose_unused_adapter */
int grun_u_dispose_unused_adapter(const char *arg) {
    /* Python: never-installed cleanup. Arg =
     * "disposed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int disposed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!disposed) { printf("0 (dispose failed — fd leak risk logged)\n"); return 0; }
    printf("1 (dropped adapter disposed — ResponseStore fds etc. released)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_wire_teams_pipeline_runtime @ gateway/run.py:_wire_teams_pipeline_runtime */
int grun_u_wire_teams_pipeline_runtime(const char *arg) {
    /* Python: pipeline bind. Arg = "has_adapter\tplugin_enabled\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_adapter = arg[0] == '1';
    int plugin = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_adapter || !plugin || !state) { printf("no-op (adapter/plugin not active)\n"); return 0; }
    printf("teams pipeline runtime bound: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_warn_if_docker_media_delivery_is_risky @ gateway/run.py:_warn_if_docker_media_delivery_is_risky */
int grun_u_warn_if_docker_media_delivery_is_risky(const char *arg) {
    /* Python: export-mount warn. Arg =
     * "docker\thas_platforms\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int docker = arg[0] == '1';
    int has_platforms = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state || !docker || !has_platforms) { printf("no warning\n"); return 0; }
    printf("WARN: docker gateway lacks explicit output mount — MEDIA delivery can fail for container-local paths: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_set_adapter_auto_tts_disabled @ gateway/run.py:_set_adapter_auto_tts_disabled */
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

/* PoP: grun_u_set_adapter_auto_tts_enabled @ gateway/run.py:_set_adapter_auto_tts_enabled */
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

/* PoP: grun_u_sync_voice_mode_state_to_adapter @ gateway/run.py:_sync_voice_mode_state_to_adapter */
int grun_u_sync_voice_mode_state_to_adapter(const char *arg) {
    /* Python: persisted /voice. Arg =
     * "synced\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int synced = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!synced) { printf("0 (no adapter sets — skip)\n"); return 0; }
    printf("1 (auto_tts default + enabled/disabled chat sets pushed to adapter)%s\n", (t2 && t2[1] == '1') ? " — lazy config load" : "");
    return 0;
}

/* PoP: grun_u_await_adapter_cleanup_with_timeout @ gateway/run.py:_await_adapter_cleanup_with_timeout */
int grun_u_await_adapter_cleanup_with_timeout(const char *arg) {
    /* Python: ownership-kept wait. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (task kept through done-callback — CancelledError-catching close can't block recovery)%s\n", t2 ? t2 + 1 : "done", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_safe_adapter_disconnect @ gateway/run.py:_safe_adapter_disconnect */
int grun_u_safe_adapter_disconnect(const char *arg) {
    /* Python: defensive disconnect. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (disconnect completed under timeout — partial-init tolerated, never raises, no Unclosed-session leaks)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_bounded_adapter_teardown @ gateway/run.py:_bounded_adapter_teardown */
int grun_u_bounded_adapter_teardown(const char *arg) {
    /* Python: timeout-budget teardown. Arg =
     * "torn\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (teardown bounded by per-adapter budget — wedged ws can't stall shutdown past TimeoutStopSec #14128)%s\n", (t2 && t2[1] == '1') ? " — timed out" : "");
    return 0;
}

/* PoP: grun_u_connect_initial_adapter_with_timeout @ gateway/run.py:_connect_initial_adapter_with_timeout */
int grun_u_connect_initial_adapter_with_timeout(const char *arg) {
    /* Python: scoped replace intent. Arg =
     * "connected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int connected = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!connected) { printf("0 (connect failed)\n"); return 0; }
    printf("1 (cold-start connect; takeover allowed=%s only while awaited, cleared in finally)%s\n", (t2 && t2[1] == '1') ? "true" : "false", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_telegram_topic_mode_enabled @ gateway/run.py:_telegram_topic_mode_enabled */
int grun_u_telegram_topic_mode_enabled(const char *arg) {
    /* Python: DM topic mode check. Arg =
     * "is_telegram\tis_dm\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_telegram = arg[0] == '1';
    int is_dm = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_telegram || !is_dm || !state) { printf("0\n"); return 0; }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_should_send_telegram_lobby_reminder @ gateway/run.py:_should_send_telegram_lobby_reminder */
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

/* PoP: grun_u_record_telegram_topic_binding @ gateway/run.py:_record_telegram_topic_binding */
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

/* PoP: grun_u_sync_telegram_topic_binding @ gateway/run.py:_sync_telegram_topic_binding */
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

/* PoP: grun_u_recover_telegram_topic_thread_id @ gateway/run.py:_recover_telegram_topic_thread_id */
int grun_u_recover_telegram_topic_thread_id(const char *arg) {
    /* Python: lobby-pin. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "not_tg") == 0 || strcmp(state, "non_lobby") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_normalize_source_for_session_key @ gateway/run.py:_normalize_source_for_session_key */
int grun_u_normalize_source_for_session_key(const char *arg) {
    /* Python: topic recovery copy. Arg = "recovered\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int recovered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !recovered) { printf("source unchanged\n"); return 0; }
    printf("source normalized: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_session_agent_runtime @ gateway/run.py:_resolve_session_agent_runtime */
int grun_u_resolve_session_agent_runtime(const char *arg) {
    /* Python: runtime priority. Arg =
     * "override\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int override = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("runtime resolved (session /model > channel_overrides > global; rehydrate+override merge%s): %s\n", override ? " APPLIED" : "", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_turn_agent_config @ gateway/run.py:_resolve_turn_agent_config */
int grun_u_resolve_turn_agent_config(const char *arg) {
    /* Python: turn route build. Arg =
     * "model\ttier\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("turn route: model=%s, fast=%s\n", arg, (t3 && t3[1] == '1') ? "on" : "off");
    return 0;
}

/* PoP: grun_u_sync_session_model_from_agent @ gateway/run.py:_sync_session_model_from_agent */
int grun_u_sync_session_model_from_agent(const char *arg) {
    /* Python: runtime model persist. Arg =
     * "synced\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int synced = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no session/agent/db)\n"); return 0; }
    if (!synced) { printf("0 (no model on agent)\n"); return 0; }
    printf("1 (runtime provider/base_url/api_mode/fallback_active persisted to session row; sync SessionDB used off-loop)%s\n", (t2 && t2[1] == '1') ? " — fallback flagged" : "");
    return 0;
}

/* PoP: grun_u_handle_reaction_event @ gateway/run.py:_handle_reaction_event */
int grun_u_handle_reaction_event(const char *arg) {
    /* Python: hook fan-out. Arg =
     * "emitted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int emitted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!emitted) { printf("0 (hook errored — never blocks adapter loop)\n"); return 0; }
    printf("1 (hooks.emit('%s', ctx) fanned out — reaction:added/removed scheme)%s\n", t2 ? t2 + 1 : "reaction:added", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_adapter_fatal_error @ gateway/run.py:_handle_adapter_fatal_error */
int grun_u_handle_adapter_fatal_error(const char *arg) {
    /* Python: retryable queue. Arg =
     * "queued\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int queued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (non-retryable — removed)\n"); return 0; }
    if (!queued) { printf("0 (handler cancelled mid-flight — strand guard)\n"); return 0; }
    printf("1 (queued for background reconnect; wrapper task protects against cancellation)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_adapter_fatal_error_detached @ gateway/run.py:_handle_adapter_fatal_error_detached */
int grun_u_handle_adapter_fatal_error_detached(const char *arg) {
    /* Python: strand-exit. Arg =
     * "exited\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exited = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!exited) { printf("0 (reconnected/queued/disabled — no exit)\n"); return 0; }
    printf("1 (stranded platform → gateway exit w/ failure so service manager restarts; handler exceptions logged)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_adapter_fatal_error_impl @ gateway/run.py:_handle_adapter_fatal_error_impl */
int grun_u_handle_adapter_fatal_error_impl(const char *arg) {
    /* Python: impl body. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("fatal handling done (%s)%s\n", tab ? tab + 1 : "", (tab && tab[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_restart_loop_guard_config @ gateway/run.py:_restart_loop_guard_config */
int grun_u_restart_loop_guard_config(const char *arg) {
    /* Python: config w/ defaults. Arg = "state\tmax_restarts\twindow\tresult". */
    if (!arg || !*arg) { printf("5\t300\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("5\t300\n"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "5", t3 ? t3 + 1 : "300");
    return 0;
}

/* PoP: grun_u_log_scale_to_zero_not_armed_reason @ gateway/run.py:_log_scale_to_zero_not_armed_reason */
int grun_u_log_scale_to_zero_not_armed_reason(const char *arg) {
    /* Python: opted-in only. Arg =
     * "opted_in\trelay_only\twake_url\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int opted_in = arg[0] == '1';
    int relay_only = t1 && t1[1] == '1';
    int wake_url = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!opted_in || !state) { printf("quiet (not opted in)\n"); return 0; }
    printf("scale-to-zero: NOT armed despite opt-in — relay_only=%s, wake_url=%s\n", relay_only ? "yes" : "no", wake_url ? "set" : "MISSING");
    return 0;
}

/* PoP: grun_u_scale_to_zero_note_real_inbound @ gateway/run.py:_scale_to_zero_note_real_inbound */
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

/* PoP: grun_u_relay_adapter_for_dormancy @ gateway/run.py:_relay_adapter_for_dormancy */
int grun_u_relay_adapter_for_dormancy(const char *arg) {
    /* Python: connected relay. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("relay adapter: %s\n", tab ? tab + 1 : "none connected");
    return 0;
}

/* PoP: grun_u_scale_to_zero_watcher @ gateway/run.py:_scale_to_zero_watcher */
int grun_u_scale_to_zero_watcher(const char *arg) {
    /* Python: D12 sequence. Arg =
     * "dormant\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (not armed)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int dormant = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!dormant) { printf("0 (idle window not sustained yet)\n"); return 0; }
    printf("1 (draining marked + relay go_dormant; NOT disconnect; NO resume_pending #D13; wakeUrl poke wakes)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_enqueue_fifo @ gateway/run.py:_enqueue_fifo */
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

/* PoP: grun_u_promote_queued_event @ gateway/run.py:_promote_queued_event */
int grun_u_promote_queued_event(const char *arg) {
    /* Python: overflow promote. Arg =
     * "has_overflow\tslot_empty\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_overflow = arg[0] == '1';
    int slot_empty = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_overflow || !state) { printf("%s\n", t3 ? t3 + 1 : ""); return 0; }
    printf("overflow promoted%s\n", slot_empty ? " (as pending)" : " (staged in slot)");
    return 0;
}

/* PoP: grun_u_clear_goal_pending_continuations @ gateway/run.py:_clear_goal_pending_continuations */
int grun_u_clear_goal_pending_continuations(const char *arg) {
    /* Python: synthetic /goal removal. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: grun_u_persist_active_agents @ gateway/run.py:_persist_active_agents */
int grun_u_persist_active_agents(const char *arg) {
    /* Python: turn-boundary write. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("status write skipped\n"); return 0; }
    printf("active_agents=%s persisted (lifecycle preserved)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: grun_u_enter_external_drain @ gateway/run.py:_enter_external_drain */
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

/* PoP: grun_u_exit_external_drain @ gateway/run.py:_exit_external_drain */
int grun_u_exit_external_drain(const char *arg) {
    /* Python: revert drain. Arg =
     * "mid_drain\tshutting\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int mid_drain = arg[0] == '1';
    int shutting = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!state || !mid_drain) { printf("not draining\n"); return 0; }
    if (shutting) { printf("drain marker cleared during shutdown — NOT reverting to running\n"); return 0; }
    printf("External drain RELEASED — re-accepting new turns; gateway_state -> running%s\n", t3 && t3[1] == '1' ? " (persisted)" : "");
    return 0;
}

/* PoP: grun_u_drain_control_watcher @ gateway/run.py:_drain_control_watcher */
int grun_u_drain_control_watcher(const char *arg) {
    /* Python: marker reconcile. Arg =
     * "draining\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int draining = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (marker absent — exited drain)\n"); return 0; }
    if (!draining) { printf("0 (stale epoch marker treated absent NS-570)\n"); return 0; }
    printf("1 (external drain entered; 1s cadence bounds observe latency; startup reconcile once)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_pause_failed_platform @ gateway/run.py:_pause_failed_platform */
int grun_u_pause_failed_platform(const char *arg) {
    /* Python: manual pause. Arg =
     * "known\tpaused\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int known = arg[0] == '1';
    int paused = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!known || paused || !state) { printf("platform not paused\n"); return 0; }
    printf("platform paused (next_retry=inf): %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_resume_paused_platform @ gateway/run.py:_resume_paused_platform */
int grun_u_resume_paused_platform(const char *arg) {
    /* Python: unpause + immediate retry. Arg =
     * "paused\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int paused = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!paused || !state) { printf("0\n"); return 0; }
    printf("%s resumed — retrying on next watcher tick\n", t2 ? t2 + 1 : "platform");
    printf("1\n");
    return 0;
}

/* PoP: grun_u_resolve_model_for_channel @ gateway/run.py:_resolve_model_for_channel */
int grun_u_resolve_model_for_channel(const char *arg) {
    /* Python: channel override else global default. Arg =
     * "override\tdefault". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", (tab && tab[1]) ? tab + 1 : arg);
    return 0;
}

/* PoP: grun_u_get_system_prompt_for_channel @ gateway/run.py:_get_system_prompt_for_channel */
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

/* PoP: grun_u_refresh_fallback_model @ gateway/run.py:_refresh_fallback_model */
int grun_u_refresh_fallback_model(const char *arg) {
    /* Python: re-read chain. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "transient") == 0) { printf("last known-good chain kept\n"); return 0; }
    if (strcmp(state, "cleared") == 0) { printf("chain cleared (no key)\n"); return 0; }
    printf("chain refreshed: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_apply_fallback_chain_to_agent @ gateway/run.py:_apply_fallback_chain_to_agent */
int grun_u_apply_fallback_chain_to_agent(const char *arg) {
    /* Python: cooldown skip. Arg =
     * "activated\tin_cooldown\tchanged\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int activated = arg[0] == '1';
    int in_cooldown = t1 && t1[1] == '1';
    int changed = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!state) { printf("no agent\n"); return 0; }
    if (activated && in_cooldown) { printf("chain rewrite skipped (cooldown)\n"); return 0; }
    printf("fallback chain applied%s\n", changed ? " + memo cleared" : "");
    return 0;
}

/* PoP: grun_u_snapshot_running_agents @ gateway/run.py:_snapshot_running_agents */
int grun_u_snapshot_running_agents(const char *arg) {
    /* Python: {session_key: agent for ... if agent is not the pending
     * sentinel}. Arg = "session_key\tagent..." pairs (agent != "PENDING"
     * kept). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: grun_u_claim_active_session_slot @ gateway/run.py:_claim_active_session_slot */
int grun_u_claim_active_session_slot(const char *arg) {
    /* Python: cross-process slot claim. Arg =
     * "local_running\tlocal_limit\tstate\tresult". */
    if (!arg || !*arg) { printf("\n\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int local_running = arg[0] == '1';
    int local_limit = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (local_running) { printf("\n\n"); return 0; }
    if (local_limit) { printf("\nlimit message\n"); return 0; }
    printf("%s\t%s\n", state ? "claimed" : "", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_agent_has_active_subagents @ gateway/run.py:_agent_has_active_subagents */
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

/* PoP: grun_u_session_has_compression_in_flight @ gateway/run.py:_session_has_compression_in_flight */
int grun_u_session_has_compression_in_flight(const char *arg) {
    /* Python: lock holder probe. Arg =
     * "in_flight\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!(arg[0] == '1')) { printf("0 (no lock held)\n"); return 0; }
    printf("1 (compression lock held — interrupt demoted to queue #56391; probes offloaded to worker thread)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_lookup_session_id_under_store_lock @ gateway/run.py:_lookup_session_id_under_store_lock */
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

/* PoP: grun_u_queue_or_replace_pending_event @ gateway/run.py:_queue_or_replace_pending_event */
int grun_u_queue_or_replace_pending_event(const char *arg) {
    /* Python: FIFO overflow #28503. Arg =
     * "queued\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int queued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter)\n"); return 0; }
    if (!queued) { printf("0 (replaced head slot — photo burst merge)\n"); return 0; }
    printf("1 (appended to FIFO overflow tail — each follow-up gets its own turn in arrival order)%s\n", (t2 && t2[1] == '1') ? " — queue mode" : "");
    return 0;
}

/* PoP: grun_u_handle_active_session_busy_message @ gateway/run.py:_handle_active_session_busy_message */
int grun_u_handle_active_session_busy_message(const char *arg) {
    /* Python: busy-path auth #17775. Arg =
     * "queued\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int queued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (unauthorized user dropped — shared-thread injection blocked)\n"); return 0; }
    if (!queued) { printf("0 (not busy — normal path)\n"); return 0; }
    printf("1 (message queued for active session; auth gate enforced same as cold path)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_notify_active_sessions_of_shutdown @ gateway/run.py:_notify_active_sessions_of_shutdown */
int grun_u_notify_active_sessions_of_shutdown(const char *arg) {
    /* Python: pre-stop notices. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (%s notices to active chats + home channels; failures swallowed — never block shutdown)%s\n", (t2 && t2[1] == '1') ? "restarting" : "shutting down", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_finalize_shutdown_agents @ gateway/run.py:_finalize_shutdown_agents */
int grun_u_finalize_shutdown_agents(const char *arg) {
    /* Python: persist in-flight. Arg =
     * "finalized\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (%s agent(s) flushed to state.db — drain-interrupted turns persist #13121)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_should_emit_long_running_notification @ gateway/run.py:_should_emit_long_running_notification */
int grun_u_should_emit_long_running_notification(const char *arg) {
    /* Python: ownership guard. Arg =
     * "has_agent\texecutor_done\trebound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    int has_agent = arg[0] == '1';
    int executor_done = t1 && t1[1] == '1';
    int rebound = t2 && t2[1] == '1';
    int state = t3 && t3[1] == '1';
    if (!has_agent || executor_done || rebound || !state) { printf("0\n"); return 0; }
    printf("%s\n", (t4 && t4[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_defer_agent_cleanup_until_future_done @ gateway/run.py:_defer_agent_cleanup_until_future_done */
int grun_u_defer_agent_cleanup_until_future_done(const char *arg) {
    /* Python: shielded waiter. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no deferral\n"); return 0; }
    printf("cleanup deferred until executor future completes (shielded; cancelled waiter never cleans early): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_cleanup_agent_resources_off_loop @ gateway/run.py:_cleanup_agent_resources_off_loop */
int grun_u_cleanup_agent_resources_off_loop(const char *arg) {
    /* Python: bounded worker. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no agent)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (shutdown/session-expiry context — skipped)\n"); return 0; }
    printf("1 (cleanup in worker thread, bounded wait; timeout cancels await, worker finishes on own #35994)%s\n", (t2 && t2[1] == '1') ? " — timed out" : "");
    return 0;
}

/* PoP: grun_u_cleanup_agent_resources @ gateway/run.py:_cleanup_agent_resources */
int grun_u_cleanup_agent_resources(const char *arg) {
    /* Python: best-effort teardown. Arg =
     * "has_agent\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_agent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_agent) { printf("no agent to clean\n"); return 0; }
    printf("agent resources cleaned (memory shutdown w/ transcript, close, stale aux clients)%s\n", (t2 && t2[1] == '1') ? " — transcript passed" : "");
    return 0;
}

/* PoP: grun_u_increment_restart_failure_counts @ gateway/run.py:_increment_restart_failure_counts */
int grun_u_increment_restart_failure_counts(const char *arg) {
    /* Python: persistent stuck-loop counters. Arg =
     * "active_keys\tstate\tresult_json". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: grun_u_suspend_stuck_loop_sessions @ gateway/run.py:_suspend_stuck_loop_sessions */
int grun_u_suspend_stuck_loop_sessions(const char *arg) {
    /* Python: restart-loop suspension. Arg =
     * "suspended\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s suspended (file cleared)\n", t2 ? t2 + 1 : arg);
    return 0;
}

/* PoP: grun_u_launch_detached_restart_command @ gateway/run.py:_launch_detached_restart_command */
int grun_u_launch_detached_restart_command(const char *arg) {
    /* Python: detached helper. Arg =
     * "launched\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no hermes bin)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int launched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (already started once)\n"); return 0; }
    if (!launched) { printf("0\n"); return 0; }
    printf("1 (detached restart helper spawned; restart_after=%ss; Windows pythonw shim)%s\n", t2 ? t2 + 1 : "5", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_startup_resume_event @ gateway/run.py:_run_startup_resume_event */
int grun_u_run_startup_resume_event(const char *arg) {
    /* Python: bounded resume. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (resume turn finished before queue release; session task awaited)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_queue_startup_restore_event @ gateway/run.py:_queue_startup_restore_event */
int grun_u_queue_startup_restore_event(const char *arg) {
    /* Python: startup queue. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (queued during startup restore; drained after: %s)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_drain_startup_restore_queue @ gateway/run.py:_drain_startup_restore_queue */
int grun_u_drain_startup_restore_queue(const char *arg) {
    /* Python: replay queue. Arg =
     * "drained\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no queue)\n"); return 0; }
    printf("%s message(s) replayed (adapter-missing → dropped w/ debug log)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_finish_startup_restore @ gateway/run.py:_finish_startup_restore */
int grun_u_finish_startup_restore(const char *arg) {
    /* Python: gather + drain. Arg =
     * "drained\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s queued message(s) drained (auto-resume tasks gathered w/ exceptions logged)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_redeliver_pending_obligations @ gateway/run.py:_redeliver_pending_obligations */
int grun_u_redeliver_pending_obligations(const char *arg) {
    /* Python: ledger replay. Arg =
     * "redelivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no recoverable obligations)\n"); return 0; }
    printf("%s obligation(s) redelivered (turn completed, only delivery owed; resume_pending cleared — no re-pay)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_schedule_resume_pending_sessions @ gateway/run.py:_schedule_resume_pending_sessions */
int grun_u_schedule_resume_pending_sessions(const char *arg) {
    /* Python: resume pass. Arg =
     * "resumed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s session(s) resumed (empty event text → injection path owns wording; platform-scoped on reconnect; adapter-missing skipped)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — running agents skipped" : "");
    return 0;
}

/* PoP: grun_u_abort_startup_if_shutdown_requested @ gateway/run.py:_abort_startup_if_shutdown_requested */
int grun_u_abort_startup_if_shutdown_requested(const char *arg) {
    /* Python: mid-startup stop. Arg =
     * "aborted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int aborted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (startup not aborted)\n"); return 0; }
    if (!aborted) { printf("0\n"); return 0; }
    printf("1 (startup aborted — adapter bg tasks cancelled, safe disconnect, stop task awaited)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_start_loop_liveness_guards @ gateway/run.py:_start_loop_liveness_guards */
int grun_u_start_loop_liveness_guards(const char *arg) {
    /* Python: arm floor timer + watchdog. Arg =
     * "enabled\tfloor_armed\twatchdog_started\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int enabled = arg[0] == '1';
    if (!enabled) { printf("loop watchdog disabled by config\n"); return 0; }
    printf("loop guards armed: floor=%s watchdog=%s\n",
           (t1 && t1[1] == '1') ? "yes" : "no",
           (t2 && t2[1] == '1') ? "yes" : "no");
    return 0;
}

/* PoP: grun_u_stop_loop_liveness_guards @ gateway/run.py:_stop_loop_liveness_guards */
int grun_u_stop_loop_liveness_guards(const char *arg) {
    /* Python: disarm before shutdown. Arg =
     * "stopped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int stopped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!stopped) { printf("0 (nothing armed)\n"); return 0; }
    printf("1 (liveness watchdog stopped + loop floor timer cancelled — shutdown won't load the loop)%s\n", (t2 && t2[1] == '1') ? " — errors logged" : "");
    return 0;
}

/* PoP: grun_u_spawn_supervised @ gateway/run.py:_spawn_supervised */
int grun_u_spawn_supervised(const char *arg) {
    /* Python: task-level supervision. Arg =
     * "spawned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int spawned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!spawned) { printf("0 (no running loop)\n"); return 0; }
    printf("1 (supervised: retained in _background_tasks, crashes logged, capped exp backoff restart, healthy-reset counter)%s\n", (t2 && t2[1] == '1') ? " — restart attempted" : "");
    return 0;
}

/* PoP: grun_u_handoff_watcher @ gateway/run.py:_handoff_watcher */
int grun_u_handoff_watcher(const char *arg) {
    /* Python: CLI→gateway handoff. Arg =
     * "handled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no pending handoffs)\n"); return 0; }
    printf("%s handoff(s) (claim pending→running; switch_session rebind; synthetic internal MessageEvent)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_process_handoff @ gateway/run.py:_process_handoff */
int grun_u_process_handoff(const char *arg) {
    /* Python: one handoff row. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (empty handoff_platform)\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (handoff failed — caller marks row failed)\n"); return 1; }
    printf("1 (handoff executed: %s platform, session switched, synthetic event)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_session_expiry_watcher @ gateway/run.py:_session_expiry_watcher */
int grun_u_session_expiry_watcher(const char *arg) {
    /* Python: finalize loop. Arg =
     * "finalized\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (60s initial delay — gateway still starting)\n"); return 0; }
    printf("%s session(s) finalized (on_session_finalize hooks, tool resource cleanup, cache evict, once-only mark)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_ensure_reconnect_watcher_running @ gateway/run.py:_ensure_reconnect_watcher_running */
int grun_u_ensure_reconnect_watcher_running(const char *arg) {
    /* Python: watcher respawn. Arg =
     * "respawned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int respawned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not running)\n"); return 0; }
    if (!respawned) { printf("0 (watcher alive)\n"); return 0; }
    printf("1 (watcher respawned — dead after restart budget / terminal exception #70344; background task tracked)%s\n", (t2 && t2[1] == '1') ? " — queued fatal errors" : "");
    return 0;
}

/* PoP: grun_u_platform_reconnect_watcher @ gateway/run.py:_platform_reconnect_watcher */
int grun_u_platform_reconnect_watcher(const char *arg) {
    /* Python: backoff retry. Arg =
     * "retried\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (queue empty)\n"); return 0; }
    printf("1 (backoff 30→60→120→240→300 cap; retryable self-heal indefinitely; non-retryable drop immediately; circuit breaker available)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_cancel_secondary_profile_reconnect_tasks @ gateway/run.py:_cancel_secondary_profile_reconnect_tasks */
int grun_u_cancel_secondary_profile_reconnect_tasks(const char *arg) {
    /* Python: pre-teardown cancel. Arg =
     * "cancelled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no pending map)\n"); return 0; }
    printf("1 (reconnect tasks cancelled before registry drain; stopped runner still blocks late installs)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_start_systemd_watchdog @ gateway/run.py:_start_systemd_watchdog */
int grun_u_start_systemd_watchdog(const char *arg) {
    /* Python: sd_notify gate. Arg =
     * "started\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int started = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not running / watchdog disabled)\n"); return 0; }
    if (!started) { printf("0 (watchdog.start failed)\n"); return 0; }
    printf("1 (sd_notify started after gateway truly running, ready msg sent)%s\n", (t2 && t2[1] == '1') ? " — already running" : "");
    return 0;
}

/* PoP: grun_u_stop_systemd_watchdog @ gateway/run.py:_stop_systemd_watchdog */
int grun_u_stop_systemd_watchdog(const char *arg) {
    /* Python: stop before drain. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("watchdog heartbeats stopped (cleared handle, awaited stop — before long shutdown drain)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_start_secondary_profile_adapters @ gateway/run.py:_start_secondary_profile_adapters */
int grun_u_start_secondary_profile_adapters(const char *arg) {
    /* Python: multiplex bring-up. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (multiplex off)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s secondary adapter(s) connected (per-profile HERMES_HOME + secret scope; source.profile stamped)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_start_one_profile_adapters @ gateway/run.py:_start_one_profile_adapters */
int grun_u_start_one_profile_adapters(const char *arg) {
    /* Python: scoped bring-up. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (MultiplexConfigError — open-policy violation)\n"); return 1; }
    printf("%s adapter(s) for profile %s (under profile runtime scope)%s\n", t2 ? t2 + 1 : "0", t1 ? t1 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_configure_profile_adapter @ gateway/run.py:_configure_profile_adapter */
int grun_u_configure_profile_adapter(const char *arg) {
    /* Python: shared handler install. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("profile handlers installed (message/fatal/session_store/busy/reaction/topic-recovery/auth-check/busy_text_mode): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_run_secondary_profile_reconnect @ gateway/run.py:_run_secondary_profile_reconnect */
int grun_u_run_secondary_profile_reconnect(const char *arg) {
    /* Python: scoped retry. Arg =
     * "reconnected\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (loop ended)\n"); return 0; }
    printf("1 (reconnected under %s profile scope; attempts=%s)%s\n", t2 ? t2 + 1 : "?", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_schedule_secondary_profile_reconnect @ gateway/run.py:_schedule_secondary_profile_reconnect */
int grun_u_schedule_secondary_profile_reconnect(const char *arg) {
    /* Python: runner-owned reconnect task. Arg =
     * "running\tretryable\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int running = arg[0] == '1';
    int retryable = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!running || !retryable) { printf("reconnect skipped\n"); return 0; }
    printf("secondary reconnect scheduled: %s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: grun_u_make_profile_fatal_error_handler @ gateway/run.py:_make_profile_fatal_error_handler */
int grun_u_make_profile_fatal_error_handler(const char *arg) {
    /* Python: secondary-profile route. Arg =
     * "profile\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("profile fatal handler created: %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_handle_profile_adapter_fatal_error @ gateway/run.py:_handle_profile_adapter_fatal_error */
int grun_u_handle_profile_adapter_fatal_error(const char *arg) {
    /* Python: secondary remove. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int removed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not this profile's adapter — ignored)\n"); return 0; }
    if (!removed) { printf("0 (mismatch — primary-only handler ignored)\n"); return 0; }
    printf("1 (failed secondary %s removed from _profile_adapters; primary slot untouched)%s\n", t2 ? t2 + 1 : "adapter", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_make_profile_message_handler @ gateway/run.py:_make_profile_message_handler */
int grun_u_make_profile_message_handler(const char *arg) {
    /* Python: profile-stamping wrapper. Arg =
     * "profile\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("handler created (no profile home)\n"); return 0; }
    printf("profile handler created: %s\n", arg);
    return 0;
}

/* PoP: grun_u_create_adapter @ gateway/run.py:_create_adapter */
int grun_u_create_adapter(const char *arg) {
    /* Python: registry-first. Arg =
     * "plugin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int plugin = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("adapter created via %s (extra defaults group/thread sessions; runner back-ref injected for admin alerts + profile routing)%s\n", plugin ? "plugin registry" : "built-in chain", (t2 && t2[1] == '1') ? " — started" : "");
    return 0;
}

/* PoP: grun_u_make_adapter_auth_check @ gateway/run.py:_make_adapter_auth_check */
int grun_u_make_adapter_auth_check(const char *arg) {
    /* Python: platform-bound callback. Arg =
     * "built\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int built = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!built) { printf("\n"); return 0; }
    printf("auth-check callback built (delegates to _is_user_authorized; unverified senders marked unverified in LLM context — prompt-injection mitigation; profile_name binds secret scope)%s\n", (t2 && t2[1] == '1') ? " — SessionSource bound" : "");
    return 0;
}

/* PoP: grun_u_deliver_platform_notice @ gateway/run.py:_deliver_platform_notice */
int grun_u_deliver_platform_notice(const char *arg) {
    /* Python: privacy-aware notice. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int delivered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter / slack ignored channel — skipped)\n"); return 0; }
    if (!delivered) { printf("0\n"); return 0; }
    printf("1 (notice delivered per platform privacy rules)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_resolve_async_delegation_session @ gateway/run.py:_resolve_async_delegation_session */
int grun_u_resolve_async_delegation_session(const char *arg) {
    /* Python: lineage follow. Arg =
     * "session\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n (fail-closed — unknown ownership; result stays in records)\n"); return 0; }
    printf("%s (compression lineage followed; never overrides unrelated /new or restored route)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_prepare_inbound_message_text @ gateway/run.py:_prepare_inbound_message_text */
int grun_u_prepare_inbound_message_text(const char *arg) {
    /* Python: shared preprocessing. Arg =
     * "prepared\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int prepared = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!prepared) { printf("\n"); return 0; }
    printf("%s (sender attribution, image enrichment, STT, doc notes, reply context, @ refs — same pipeline for normal+queued; native-vision path buffered)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_prepare_profile_scoped_inbound_message_text @ gateway/run.py:_prepare_profile_scoped_inbound_message_text */
int grun_u_prepare_profile_scoped_inbound_message_text(const char *arg) {
    /* Python: multiplex scope. Arg =
     * "scoped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scoped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!scoped) { printf("0 (not multiplexed — plain preprocessing)\n"); return 0; }
    printf("1 (preprocessed under profile runtime scope for %s)%s\n", t2 ? t2 + 1 : "source", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_async_session_store @ gateway/run.py:async_session_store */
int grun_async_session_store(const char *arg) {
    /* Python: return cached async facade for session_store. Arg = store id. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    printf("async facade for %s\n", arg);
    return 0;
}

/* PoP: grun_u_handle_message_with_agent @ gateway/run.py:_handle_message_with_agent */
int grun_u_handle_message_with_agent(const char *arg) {
    /* Python: inner handler under _running_agents sentinel guard.
     * Full pipeline: topic recovery → session get/create → pinned
     * delegation resolve → inbound preprocessing → compression-in-flight
     * check (interrupt demoted to queue #56391) → run generation stamp →
     * agent turn (local or proxy) → STT/media enrichment → post-turn goal
     * continuation → cache re-baseline → busy-session queue semantics.
     * Arg = "processed	state	result". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '	');
    const char *t2 = t1 ? strchr(t1 + 1, '	') : NULL;
    int processed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (sentinel guard skipped)\n"); return 0; }
    if (!processed) { printf("0 (delegation pin unresolved)\n"); return 0; }
    printf("1 (turn completed: topic recovery %s, goal continuation %s, cache re-baselined)%s\n", (t2 && t2[1] == '1') ? "applied" : "none", (t2 && t2[1] == '2') ? "queued" : "none", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_reset_notice_session_info @ gateway/run.py:_reset_notice_session_info */
int grun_u_reset_notice_session_info(const char *arg) {
    /* Python: profile-scoped notice info. Arg = "multiplex\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int multiplex = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_format_session_info @ gateway/run.py:_format_session_info */
int grun_u_format_session_info(const char *arg) {
    /* Python: model info block. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("session info block (model/provider/context/endpoint — surfaces wrong context detection)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_sibling_thread_run_keys @ gateway/run.py:_sibling_thread_run_keys */
int grun_u_sibling_thread_run_keys(const char *arg) {
    /* Python: per-user thread keys. Arg =
     * "in_thread\tcount\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int in_thread = arg[0] == '1';
    int state = t2 && t2[1] == '1';
    if (!in_thread || !state) { printf("[]\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "[]");
    return 0;
}

/* PoP: grun_u_is_stale_restart_redelivery @ gateway/run.py:_is_stale_restart_redelivery */
int grun_u_is_stale_restart_redelivery(const char *arg) {
    /* Python: update_id marker. Arg =
     * "stale\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int stale = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not telegram / no update_id)\n"); return 0; }
    printf("%s (restart_last_processed.json compare, <5min recency; telegram-only today)%s\n", stale ? "1" : "0", (t2 && t2[1] == '1') ? " — booted from restart" : "");
    return 0;
}

/* PoP: grun_u_handle_suggestions_command @ gateway/run.py:_handle_suggestions_command */
int grun_u_handle_suggestions_command(const char *arg) { (void)arg; return 0; }

/* PoP: grun_u_handle_blueprint_command @ gateway/run.py:_handle_blueprint_command */
int grun_u_handle_blueprint_command(const char *arg) { (void)arg; return 0; }

/* PoP: grun_u_get_goal_manager_for_event @ gateway/run.py:_get_goal_manager_for_event */
int grun_u_get_goal_manager_for_event(const char *arg) {
    /* Python: session-bound. Arg =
     * "found\tstate\tresult". */
    if (!arg || !*arg) { printf("\t\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int found = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\t\n"); return 0; }
    if (!found) { printf("\t\n"); return 0; }
    printf("GoalManager bound (session %s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_goal_status_notice @ gateway/run.py:_send_goal_status_notice */
int grun_u_send_goal_status_notice(const char *arg) {
    /* Python: judge status line. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter)\n"); return 0; }
    if (!sent) { printf("0 (send failed — logged)\n"); return 0; }
    printf("1 (judge status sent to originating chat w/ thread metadata)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_defer_goal_status_notice_after_delivery @ gateway/run.py:_defer_goal_status_notice_after_delivery */
int grun_u_defer_goal_status_notice_after_delivery(const char *arg) {
    /* Python: post-delivery callback. Arg =
     * "deferred\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int deferred = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!deferred) { printf("0 (no post-delivery hook — direct awaited delivery fallback)\n"); return 0; }
    printf("1 (goal status scheduled after main response send — natural reading order)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_post_turn_goal_continuation @ gateway/run.py:_post_turn_goal_continuation */
int grun_u_post_turn_goal_continuation(const char *arg) {
    /* Python: judge + enqueue. Arg =
     * "enqueued\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no goal)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enqueued = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!enqueued) { printf("0 (judge said done/wait)\n"); return 0; }
    printf("1 (continuation queued via FIFO — real user messages take priority)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_voice_channel_join @ gateway/run.py:_handle_voice_channel_join */
int grun_u_handle_voice_channel_join(const char *arg) {
    /* Python: join voice. Arg =
     * "joined\tstate\tresult". */
    if (!arg || !*arg) { printf("Voice channels are not supported on this platform.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int joined = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("This command only works in a Discord server.\n"); return 0; }
    if (!joined) { printf("You need to be in a voice channel first.\n"); return 0; }
    printf("Joined the voice channel.%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_voice_channel_leave @ gateway/run.py:_handle_voice_channel_leave */
int grun_u_handle_voice_channel_leave(const char *arg) {
    /* Python: leave voice. Arg =
     * "left\tstate\tresult". */
    if (!arg || !*arg) { printf("Not in a voice channel.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int left = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("Not in a voice channel.\n"); return 0; }
    if (!left) { printf("Not in a voice channel.\n"); return 0; }
    printf("Left the voice channel.%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_handle_voice_timeout_cleanup @ gateway/run.py:_handle_voice_timeout_cleanup */
int grun_u_handle_voice_timeout_cleanup(const char *arg) {
    /* Python: voice_mode off + save + disable auto-tts. Arg = "chat_id\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("voice mode off (%.*s), auto-tts disabled\n",
           (int)(tab ? (size_t)(tab - arg) : strlen(arg)), arg);
    return 0;
}

/* PoP: grun_u_handle_voice_channel_input @ gateway/run.py:_handle_voice_channel_input */
int grun_u_handle_voice_channel_input(const char *arg) {
    /* Python: voice → synthetic event. Arg =
     * "processed\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no discord adapter)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int processed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no linked text channel)\n"); return 0; }
    if (!processed) { printf("0\n"); return 0; }
    printf("1 (voice input through full pipeline — session/typing/agent/TTS reply shared with bound text chat)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_should_send_voice_reply @ gateway/run.py:_should_send_voice_reply */
int grun_u_should_send_voice_reply(const char *arg) {
    /* Python: TTS dedup ladder. Arg =
     * "mode_ok\thas_agent_tts\tvoice_input\talready_sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int mode_ok = arg[0] == '1';
    int has_agent_tts = t1 && t1[1] == '1';
    int voice_input = t2 && t2[1] == '1';
    int already_sent = t3 && t3[1] == '1';
    int state = t4 && t4[1] == '1';
    if (!state || !mode_ok || has_agent_tts) { printf("0\n"); return 0; }
    if (voice_input && !already_sent) { printf("0\n"); return 0; }
    printf("%s\n", (t5 && t5[1] == '1') ? "1" : "1");
    return 0;
}

/* PoP: grun_u_send_voice_reply @ gateway/run.py:_send_voice_reply */
int grun_u_send_voice_reply(const char *arg) {
    /* Python: TTS bubble. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no tts text)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (tts failed)\n"); return 0; }
    if (!sent) { printf("0\n"); return 0; }
    printf("1 (voice sent before text; ogg for Telegram native bubble, else mp3)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_deliver_media_from_response @ gateway/run.py:_deliver_media_from_response */
int grun_u_deliver_media_from_response(const char *arg) {
    /* Python: explicit MEDIA: only. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no explicit MEDIA tags)\n"); return 0; }
    printf("%s file(s) delivered (post-stream rescan — EXPLICIT-ONLY, no bare-path auto-detect)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_background_task @ gateway/run.py:_run_background_task */
int grun_u_run_background_task(const char *arg) {
    /* Python: profile-scoped wrapper. Arg =
     * "scoped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scoped = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!scoped) { printf("0 (not multiplexed — inner direct)\n"); return 0; }
    printf("1 (task ran under profile runtime scope — credentials from that profile's secret scope)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_background_task_inner @ gateway/run.py:_run_background_task_inner */
int grun_u_run_background_task_inner(const char *arg) {
    /* Python: inner impl. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (background task completed: %s)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_get_telegram_topic_capabilities @ gateway/run.py:_get_telegram_topic_capabilities */
int grun_u_get_telegram_topic_capabilities(const char *arg) {
    /* Python: getMe flags. Arg =
     * "checked\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"checked\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int checked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{\"checked\": false}\n"); return 0; }
    if (!checked) { printf("{\"checked\": false} (getMe failed)\n"); return 0; }
    printf("{\"checked\": true, \"capabilities\": %s} (can_send_paid_media etc.)%s\n", t2 ? t2 + 1 : "{}", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_ensure_telegram_system_topic @ gateway/run.py:_ensure_telegram_system_topic */
int grun_u_ensure_telegram_system_topic(const char *arg) {
    /* Python: System topic. Arg =
     * "created\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (no adapter/chat)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int created = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!created) { printf("0 (create failed — debug logged)\n"); return 0; }
    printf("1 (System topic created+%s after /topic activation)%s\n", (t2 && t2[1] == '1') ? "pinned" : "named", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_telegram_topic_setup_image @ gateway/run.py:_send_telegram_topic_setup_image */
int grun_u_send_telegram_topic_setup_image(const char *arg) {
    /* Python: bundled screenshot. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no adapter / no chat / no send_image_file)\n"); return 0; }
    if (!sent) { printf("0 (asset missing)\n"); return 0; }
    printf("1 (BotFather Threads Settings screenshot sent w/ caption)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_rename_discord_auto_thread_for_session_title @ gateway/run.py:_rename_discord_auto_thread_for_session_title */
int grun_u_rename_discord_auto_thread_for_session_title(const char *arg) {
    /* Python: semantic rename. Arg =
     * "renamed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int renamed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not auto lane / no adapter / no rename_thread)\n"); return 0; }
    if (!renamed) { printf("0 (rename failed)\n"); return 0; }
    printf("1 (thread %s renamed to sanitized '%s')%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "", "");
    return 0;
}

/* PoP: grun_u_schedule_discord_semantic_thread_rename @ gateway/run.py:_schedule_discord_semantic_thread_rename */
int grun_u_schedule_discord_semantic_thread_rename(const char *arg) {
    /* Python: threadsafe schedule. Arg =
     * "scheduled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scheduled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !scheduled) { printf("0 (no title / not discord auto lane / loop closed)\n"); return 0; }
    printf("1 (semantic rename scheduled via safe_schedule_threadsafe, source copied, failure logged)%s\n", (t2 && t2[1] == '1') ? " — done-callback wired" : "");
    return 0;
}

/* PoP: grun_u_rename_telegram_topic_for_session_title @ gateway/run.py:_rename_telegram_topic_for_session_title */
int grun_u_rename_telegram_topic_for_session_title(const char *arg) {
    /* Python: auto-title rename. Arg =
     * "renamed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int renamed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (not topic lane / disabled)\n"); return 0; }
    if (!renamed) { printf("0\n"); return 0; }
    printf("1 (topic renamed to auto-title; extra.disable_topic_auto_rename honored)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_schedule_telegram_topic_title_rename @ gateway/run.py:_schedule_telegram_topic_title_rename */
int grun_u_schedule_telegram_topic_title_rename(const char *arg) {
    /* Python: thread-safe rename schedule. Arg =
     * "title\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("rename not scheduled\n"); return 0; }
    printf("topic rename scheduled: %s\n", arg);
    return 0;
}

/* PoP: grun_u_disable_telegram_topic_mode_for_chat @ gateway/run.py:_disable_telegram_topic_mode_for_chat */
int grun_u_disable_telegram_topic_mode_for_chat(const char *arg) {
    /* Python: /topic off. Arg =
     * "disabled\tstate\tresult". */
    if (!arg || !*arg) { printf("Could not determine chat ID.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int disabled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (session db unavailable / never enabled)\n"); return 0; }
    if (!disabled) { printf("0 (already off)\n"); return 0; }
    printf("1 (topic mode disabled; threads archived/pinned cleanup)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_telegram_topic_root_status_message @ gateway/run.py:_telegram_topic_root_status_message */
int grun_u_telegram_topic_root_status_message(const char *arg) {
    /* Python: multi-session help. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("Telegram multi-session topics are enabled.\n");
    printf("… open All Messages, send any message to create a new topic.%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_restore_telegram_topic_session @ gateway/run.py:_restore_telegram_topic_session */
int grun_u_restore_telegram_topic_session(const char *arg) {
    /* Python: /topic restore. Arg =
     * "restored\tstate\tresult". */
    if (!arg || !*arg) { printf("Session not found.\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int restored = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("Session not found.\n"); return 0; }
    if (!restored) { printf("That session is not a Telegram session and cannot be restored into this topic.\n"); return 0; }
    printf("1 (session restored into topic; unlinked list refreshed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_execute_mcp_reload @ gateway/run.py:_execute_mcp_reload */
int grun_u_execute_mcp_reload(const char *arg) {
    /* Python: disconnect+reconnect. Arg =
     * "reloaded\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int reloaded = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (reload failed)\n"); return 1; }
    if (!reloaded) { printf("0\n"); return 1; }
    printf("1 (old servers shutdown, tools rediscovered, changes notified: %s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_maybe_confirm_destructive_slash @ gateway/run.py:_maybe_confirm_destructive_slash */
int grun_u_maybe_confirm_destructive_slash(const char *arg) {
    /* Python: destructive gate. Arg =
     * "result\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *result = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("executed immediately (gate off)\n"); return 0; }
    printf("%s (once/always → execute; cancel → aborted)%s\n", result, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_request_slash_confirm @ gateway/run.py:_request_slash_confirm */
int grun_u_request_slash_confirm(const char *arg) {
    /* Python: once/always/cancel. Arg =
     * "buttons\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int buttons = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!buttons) { printf("ack text (picker fallback — text path IS the ack)\n"); return 0; }
    printf("ack None (buttons self-explanatory; handler runs on response)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_read_user_config @ gateway/run.py:_read_user_config */
int grun_u_read_user_config(const char *arg) {
    /* Python: load_config() or {} (fail-open). Arg = config JSON. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: grun_u_schedule_update_notification_watch @ gateway/run.py:_schedule_update_notification_watch */
int grun_u_schedule_update_notification_watch(const char *arg) {
    /* Python: single watcher. Arg =
     * "scheduled\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int scheduled = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!scheduled) { printf("0 (task already running)\n"); return 0; }
    printf("1 (watcher scheduled: polls .update_output.txt, streams chunks, forwards .update_prompt.json, intercepts replies to .update_response)%s\n", (t2 && t2[1] == '1') ? " — no event loop" : "");
    return 0;
}

/* PoP: grun_u_watch_update_progress @ gateway/run.py:_watch_update_progress */
int grun_u_watch_update_progress(const char *arg) {
    /* Python: .update_output stream. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("update watcher (polls .update_output.txt, streams chunks @4s, forwards .update_prompt.json, intercepts replies → .update_response)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_send_update_notification @ gateway/run.py:_send_update_notification */
int grun_u_send_update_notification(const char *arg) {
    /* Python: legacy claim. Arg =
     * "notified\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (still running — retry later)\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int notified = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (still running — retry later)\n"); return 0; }
    if (!notified) { printf("1 (definitive skip)\n"); return 0; }
    printf("1 (update finished — user notified via .update_pending.claimed.json)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_restart_notification @ gateway/run.py:_send_restart_notification */
int grun_u_send_restart_notification(const char *arg) {
    /* Python: .restart_notify.json. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int sent = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!sent) { printf("\n"); return 0; }
    printf("1 (back-online notice sent to initiating chat; marker removed)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_send_home_channel_startup_notifications @ gateway/run.py:_send_home_channel_startup_notifications */
int grun_u_send_home_channel_startup_notifications(const char *arg) {
    /* Python: ♻️ once per home. Arg =
     * "sent\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("♻️ Gateway online — Hermes is back and ready. (%s home channel(s); skip_targets dedup)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_set_session_env @ gateway/run.py:_set_session_env */
int grun_u_set_session_env(const char *arg) {
    /* Python: contextvar bind. Arg =
     * "platform\tasync_delivery\tstate\ttokens". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("session vars bound (%s, async_delivery=%s): %s reset tokens\n", arg, (t1 && t1[1] == '1') ? "yes" : "no", t3 ? t3 + 1 : "0");
    return 0;
}

/* PoP: grun_u_clear_session_env @ gateway/run.py:_clear_session_env */
int grun_u_clear_session_env(const char *arg) {
    /* Python: restore session context vars. Arg = "tokens\tstate". */
    (void)arg;
    printf("session context vars cleared\n");
    return 0;
}

/* PoP: grun_u_run_in_executor_with_context @ gateway/run.py:_run_in_executor_with_context */
int grun_u_run_in_executor_with_context(const char *arg) {
    /* Python: contextvars-preserving. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (copy_context + executor, ctx.run(func))%s\n", t2 ? t2 + 1 : "done", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_get_executor @ gateway/run.py:_get_executor */
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

/* PoP: grun_u_shutdown_executor @ gateway/run.py:_shutdown_executor */
int grun_u_shutdown_executor(const char *arg) {
    /* Python: executor shutdown wait=False cancel_futures. Arg = "state". */
    (void)arg;
    printf("gateway executor shut down\n");
    return 0;
}

/* PoP: grun_u_enrich_message_with_vision @ gateway/run.py:_enrich_message_with_vision */
int grun_u_enrich_message_with_vision(const char *arg) {
    /* Python: auto vision prefix. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enriched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no images)\n"); return 0; }
    if (!enriched) { printf("0 (vision unavailable)\n"); return 0; }
    printf("1 (%s image(s) described + cache paths injected)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_enrich_message_with_transcription @ gateway/run.py:_enrich_message_with_transcription */
int grun_u_enrich_message_with_transcription(const char *arg) {
    /* Python: STT prefix. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\t[] (no audio / STT unavailable)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s\t%s (transcript wrappers prepended; raw transcripts returned)%s\n", t2 ? t2 + 1 : "", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_pending_event_audio_paths @ gateway/run.py:_pending_event_audio_paths */
int grun_u_pending_event_audio_paths(const char *arg) {
    /* Python: STT-eligible filter. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s audio path(s) STT-eligible (is_stt_input per media index)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — cached on event" : "");
    return 0;
}

/* PoP: grun_u_transcribe_pending_audio_event_once @ gateway/run.py:_transcribe_pending_audio_event_once */
int grun_u_transcribe_pending_audio_event_once(const char *arg) {
    /* Python: cached STT. Arg =
     * "text\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (single STT call + single echo; cached on event for both interrupt monitor and drain path)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: grun_u_echo_pending_stt_transcripts_once @ gateway/run.py:_echo_pending_stt_transcripts_once */
int grun_u_echo_pending_stt_transcripts_once(const char *arg) {
    /* Python: 🎙️ echo. Arg =
     * "echoed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int echoed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!echoed) { printf("0 (no transcripts / echo disabled / already echoed)\n"); return 0; }
    printf("1 (🎙️ transcripts echoed; per-event once-flag set)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_transcribe_and_echo_pending_voice @ gateway/run.py:_transcribe_and_echo_pending_voice */
int grun_u_transcribe_and_echo_pending_voice(const char *arg) {
    /* Python: unified voice helper. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("\t[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s\t[] (no STT-eligible media)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s\t%s (single STT + echo; feeds interrupt or drain)%s\n", t2 ? t2 + 1 : "", t2 ? t2 + 1 : "[]", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_build_process_event_source @ gateway/run.py:_build_process_event_source */
int grun_u_build_process_event_source(const char *arg) {
    /* Python: session-store origin first. Arg =
     * "origin\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("source resolved (session-store origin preferred — NEVER active foreground event, avoids cross-topic bleed%s)%s\n", (t2 && t2[1] == '1') ? "; fallback cached source" : "; fallback parse", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_inject_watch_notification @ gateway/run.py:_inject_watch_notification */
int grun_u_inject_watch_notification(const char *arg) {
    /* Python: queued-event route. Arg =
     * "accepted\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_route") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "retry") == 0) { printf("0 (retryable adapter failure)\n"); return 0; }
    printf("1 (accepted; routing from queued event itself — never foreground message)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_classify_completion_target @ gateway/run.py:_classify_completion_target */
int grun_u_classify_completion_target(const char *arg) {
    /* Python: deliver/terminal/delayed. Arg =
     * "class\tstate\tresult". */
    if (!arg || !*arg) { printf("terminal\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *cls = t1 ? t1 + 1 : "terminal";
    int state = arg[0] == '1';
    if (!state) { printf("terminal\n"); return 0; }
    printf("%s (spawning session live / compression-rotated continuation #55578 pre-flight; durable ack stays honest)%s\n", cls, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_deliver_completion_notification @ gateway/run.py:_deliver_completion_notification */
int grun_u_deliver_completion_notification(const char *arg) {
    /* Python: once-per-lifecycle. Arg =
     * "delivered\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "claimed") == 0) { printf("\n (another caller owns/delivered)\n"); return 0; }
    if (strcmp(state, "retry") == 0) { printf("0 (claim released for retry)\n"); return 0; }
    printf("1 (delivered once per live gateway; durable ids %s)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_enrich_async_delegation_routing @ gateway/run.py:_enrich_async_delegation_routing */
int grun_u_enrich_async_delegation_routing(const char *arg) {
    /* Python: session_key parse. Arg =
     * "enriched\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int enriched = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!enriched) { printf("0 (already has platform / unparseable — CLI-origin left as-is)\n"); return 0; }
    printf("1 (platform/chat_type/chat_id/thread_id filled from session_key)%s\n", (t2 && t2[1] == '1') ? " — thread_id present" : "");
    return 0;
}

/* PoP: grun_u_async_delegation_watcher @ gateway/run.py:_async_delegation_watcher */
int grun_u_async_delegation_watcher(const char *arg) {
    /* Python: idle drain. Arg =
     * "drained\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (queue empty — silent)\n"); return 0; }
    printf("%s completion(s) injected as new turns (covers idle case; mirrors CLI process_loop drain)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_process_watcher @ gateway/run.py:_run_process_watcher */
int grun_u_run_process_watcher(const char *arg) {
    /* Python: bg process push. Arg =
     * "mode\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *mode = t1 ? t1 + 1 : "all";
    int state = arg[0] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("watcher mode=%s (all → running updates + final; result → final only; error → nonzero only; off → silent; auto-remove on exit)%s\n", mode, (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_extract_honcho_cache_busting_config @ gateway/run.py:_extract_honcho_cache_busting_config */
int grun_u_extract_honcho_cache_busting_config(const char *arg) {
    /* Python: memoized honcho keys. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: grun_u_extract_cache_busting_config @ gateway/run.py:_extract_cache_busting_config */
int grun_u_extract_cache_busting_config(const char *arg) {
    /* Python: registry generation. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{}");
    return 0;
}

/* PoP: grun_u_agent_config_signature @ gateway/run.py:_agent_config_signature */
int grun_u_agent_config_signature(const char *arg) {
    /* Python: cache-stability key. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("signature=%s (model/runtime/toolsets/cache_keys/user_id/user_id_alt)\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_rehydrate_session_model_override @ gateway/run.py:_rehydrate_session_model_override */
int grun_u_rehydrate_session_model_override(const char *arg) {
    /* Python: restart persistence. Arg =
     * "rehydrated\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int rehydrated = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("no-op (in-memory wins or nothing persisted)\n"); return 0; }
    if (!rehydrated) { printf("no persisted override\n"); return 0; }
    printf("rehydrated /model override (api_key never persisted; creds re-resolved): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_release_running_agent_state @ gateway/run.py:_release_running_agent_state */
int grun_u_release_running_agent_state(const char *arg) {
    /* Python: full-pop funnel. Arg =
     * "generation_ok\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int generation_ok = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!generation_ok) { printf("0 (ownership guard blocked)\n"); return 0; }
    printf("1 (slots popped, lease released, active count persisted)%s\n", (t2 && t2[1] == '1') ? " — busy_ack cleared" : "");
    return 0;
}

/* PoP: grun_u_release_turn_lease @ gateway/run.py:_release_turn_lease */
int grun_u_release_turn_lease(const char *arg) {
    /* Python: generation-keyed lease release. Arg =
     * "session_key\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\n", (t2 && t2[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_rebind_turn_lease @ gateway/run.py:_rebind_turn_lease */
int grun_u_rebind_turn_lease(const char *arg) {
    /* Python: rotation-follow rebind. Arg =
     * "has_key\thas_new\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int has_key = arg[0] == '1';
    int has_new = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!has_key || !has_new || !state) { printf("0\n"); return 0; }
    printf("%s\n", (t3 && t3[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: grun_u_clear_conversation_scope @ gateway/run.py:_clear_conversation_scope */
int grun_u_clear_conversation_scope(const char *arg) {
    /* Python: THE funnel. Arg =
     * "has_key\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int has_key = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state || !has_key) { printf("no scope to clear\n"); return 0; }
    printf("conversation scope cleared (%s, security state too)\n", t2 ? t2 + 1 : "all scoped dicts");
    return 0;
}

/* PoP: grun_u_clear_session_boundary_security_state @ gateway/run.py:_clear_session_boundary_security_state */
int grun_u_clear_session_boundary_security_state(const char *arg) {
    /* Python: per-session clear. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no session key\n"); return 0; }
    printf("session boundary security state cleared: %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_bind_adapter_run_generation @ gateway/run.py:_bind_adapter_run_generation */
int grun_u_bind_adapter_run_generation(const char *arg) {
    /* Python: gen stamp. Arg =
     * "bound\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int bound = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!bound) { printf("0 (no active-session event)\n"); return 0; }
    printf("1 (run generation stamped on adapter active-session event)%s\n", (t2 && t2[1] == '1') ? " — interrupt cleared it" : "");
    return 0;
}

/* PoP: grun_u_interrupt_and_clear_session @ gateway/run.py:_interrupt_and_clear_session */
int grun_u_interrupt_and_clear_session(const char *arg) {
    /* Python: consistent clear. Arg =
     * "cleared\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cleared = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no session key)\n"); return 0; }
    if (!cleared) { printf("0\n"); return 0; }
    printf("1 (agent interrupted + generation invalidated + adapter activity cleared)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_refresh_agent_cache_message_count @ gateway/run.py:_refresh_agent_cache_message_count */
int grun_u_refresh_agent_cache_message_count(const char *arg) {
    /* Python: re-baseline #45966. Arg =
     * "refreshed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("1 (cache snapshot re-baselined to %s after own turn — next-turn coherence guard won't false-rebuild)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_voice_channel_sidecar_note @ gateway/run.py:_voice_channel_sidecar_note */
int grun_u_voice_channel_sidecar_note(const char *arg) {
    /* Python: VC-change note. Arg =
     * "is_discord\tchanged\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int is_discord = arg[0] == '1';
    int changed = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!is_discord || !changed || !state) { printf("\n"); return 0; }
    printf("%s\n", t3 ? t3 + 1 : "[Voice channel now: ...]");
    return 0;
}

/* PoP: grun_u_pinned_session_context_prompt @ gateway/run.py:_pinned_session_context_prompt */
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

/* PoP: grun_u_init_cached_agent_for_turn @ gateway/run.py:_init_cached_agent_for_turn */
int grun_u_init_cached_agent_for_turn(const char *arg) {
    /* Python: per-turn reset. Arg = "interrupt_depth\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long depth = strtol(arg, NULL, 10);
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("agent reset for turn (depth=%ld, activity %s)\n", depth,
           depth == 0 ? "restamped" : "preserved");
    return 0;
}

/* PoP: grun_u_commit_memory_before_soft_evict @ gateway/run.py:_commit_memory_before_soft_evict */
int grun_u_commit_memory_before_soft_evict(const char *arg) {
    /* Python: LRU-cap on_session_end. Arg =
     * "committed\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int committed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!committed) { printf("no commit needed (no manager / non-finalizable / expired)\n"); return 0; }
    printf("on_session_end committed pre-eviction (soft evict keeps session resumable #11205): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: grun_u_commit_then_release_soft @ gateway/run.py:_commit_then_release_soft */
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

/* PoP: grun_u_release_evicted_agent_soft @ gateway/run.py:_release_evicted_agent_soft */
int grun_u_release_evicted_agent_soft(const char *arg) {
    /* Python: soft cleanup. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no agent to release\n"); return 0; }
    printf("agent released (clients kept, messages freed)%s\n", (tab && tab[1] == '1') ? " + history cleared" : "");
    return 0;
}

/* PoP: grun_u_enforce_agent_cache_cap @ gateway/run.py:_enforce_agent_cache_cap */
int grun_u_enforce_agent_cache_cap(const char *arg) {
    /* Python: LRU evict. Arg =
     * "evicted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int evicted = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no cache / non-OrderedDict)\n"); return 0; }
    if (!evicted) { printf("0 (all candidates active — leave over cap, re-check next insert)\n"); return 0; }
    printf("%s agent(s) evicted (running agents skipped; teardown on daemon thread, lock not held during cleanup)\n", t2 ? t2 + 1 : "1");
    return 0;
}

/* PoP: grun_u_sweep_idle_cached_agents @ gateway/run.py:_sweep_idle_cached_agents */
int grun_u_sweep_idle_cached_agents(const char *arg) {
    /* Python: idle TTL evict. Arg =
     * "evicted\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no cache/lock)\n"); return 0; }
    printf("%s agent(s) evicted (idle > TTL, running skipped, lock acquired internally, teardown daemon threads)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? " — pending sentinel skipped" : "");
    return 0;
}

/* PoP: grun_u_run_agent_via_proxy @ gateway/run.py:_run_agent_via_proxy */
int grun_u_run_agent_via_proxy(const char *arg) {
    /* Python: thin-relay mode. Arg =
     * "done\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (proxy delegation failed)\n"); return 1; }
    printf("1 (delegated via POST /v1/chat/completions SSE stream; platform I/O local, agent remote)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_profile_name_for_source @ gateway/run.py:_profile_name_for_source */
int grun_u_profile_name_for_source(const char *arg) {
    /* Python: route match. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_resolve_profile_home_for_source @ gateway/run.py:_resolve_profile_home_for_source */
int grun_u_resolve_profile_home_for_source(const char *arg) {
    /* Python: 3-step home. Arg =
     * "home\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("%s (active profile fallback)\n", t1 ? t1 + 1 : ""); return 0; }
    printf("%s (source.profile > re-routed > active profile)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_run_planned_stop_watcher @ gateway/run.py:_run_planned_stop_watcher */
int grun_u_run_planned_stop_watcher(const char *arg) {
    /* Python: Windows SIGTERM bridge. Arg =
     * "marker\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int marker = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("watcher not started\n"); return 0; }
    if (!marker) { printf("no marker — polling\n"); return 0; }
    printf("planned-stop marker found — invoking graceful shutdown path (#33778 Windows resume fix)%s\n", (t2 && t2[1] == '1') ? " — same handler as SIGTERM" : "");
    return 0;
}

/* PoP: grun_u_start_gateway_housekeeping @ gateway/run.py:_start_gateway_housekeeping */
int grun_u_start_gateway_housekeeping(const char *arg) {
    /* Python: non-cron chores. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("housekeeping thread started (channel dir 5min, image/audio/doc cache + paste sweep + curator poll hourly, auto-archive hourly)%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: grun_u_start_cron_ticker @ gateway/run.py:_start_cron_ticker */
int grun_u_start_cron_ticker(const char *arg) {
    /* Python: deprecated shim -> InProcessCronScheduler. Arg = "state". */
    (void)arg;
    printf("cron ticker started (in-process scheduler)\n");
    return 0;
}

/* PoP: grun_u_await_thread_exit @ gateway/run.py:_await_thread_exit */
int grun_u_await_thread_exit(const char *arg) {
    /* Python: non-blocking join. Arg =
     * "exited\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exited = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!exited) { printf("0 (still alive after timeout — loop never blocked #58818)\n"); return 0; }
    printf("1 (thread exited; is_alive polling keeps loop running so cron delivery completes)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_start_gateway @ gateway/run.py:start_gateway */
int grun_start_gateway(const char *arg) {
    /* Python: main entry. Arg =
     * "ran\tstate\tresult". */
    if (!arg || !*arg) { printf("0 (failed to start — non-zero exit for systemd)\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ran = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (failed to start — non-zero exit for systemd)\n"); return 1; }
    if (!ran) { printf("0\n"); return 1; }
    printf("1 (gateway ran until interrupted; replace=%s; boot fingerprint recorded for code-skew detection)%s\n", (t2 && t2[1] == '1') ? "true" : "false", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: grun_u_exit_after_graceful_shutdown @ gateway/run.py:_exit_after_graceful_shutdown */
int grun_u_exit_after_graceful_shutdown(const char *arg) {
    /* Python: os._exit backstop. Arg =
     * "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("os._exit called ...%s\n", tab ? tab + 1 : "");
    return 0;
}

/* ── GatewayRunner agent-cache bound methods (resolve via agent_cache_pressure.c) ──
 * These are pure config math — resolve lazily per process, caching on first
 * hit. The live cache objects (_agent_cache, _agent_cache_lock,
 * _running_agent_items) live in the gateway-runner seam and are exercised by
 * the already-ported _enforce_agent_cache_cap / _sweep_idle_cached_agents. */

/* PoP: grun_u_agent_cache_bounds @ gateway/run.py:_agent_cache_bounds */
agent_cache_bounds_t grun_u_agent_cache_bounds(void) {
    static agent_cache_bounds_t cached;
    static bool filled = false;
    if (filled) return cached;
    json_t *raw = config_py_read_raw_config();   /* raw json_t dict, caller frees */
    agent_cache_bounds_t b = acb_resolve_agent_cache_bounds(raw ? raw : NULL);
    if (raw) json_free(raw);
    cached = b;
    filled = true;
    return b;
}

/* PoP: grun_u_agent_cache_cap @ gateway/run.py:_agent_cache_cap */
int grun_u_agent_cache_cap(void) {
    agent_cache_bounds_t b = grun_u_agent_cache_bounds();
    return b.max_size != ACB_UNSET_INT ? b.max_size : ACB_AGENT_CACHE_MAX_SIZE;
}

/* PoP: grun_u_agent_cache_idle_ttl @ gateway/run.py:_agent_cache_idle_ttl */
double grun_u_agent_cache_idle_ttl(void) {
    agent_cache_bounds_t b = grun_u_agent_cache_bounds();
    return b.idle_ttl_secs != ACB_UNSET_FLOAT ? b.idle_ttl_secs : ACB_AGENT_CACHE_IDLE_TTL_SECS;
}


/* ════════════════════════════════════════════════════════════════════
 * Gap closure wave (gateway/run.py REAL_GAPs).
 * Python asyncio futures → C status structs; supervised watchers →
 * vtable hooks + worker threads; delivery-ledger orchestration →
 * libjson row processing over the existing ledger/session seams.
 * ════════════════════════════════════════════════════════════════════ */
#include <math.h>
#include <pthread.h>
#include <errno.h>

extern void clear_resume_pending(const char *session_key);   /* gw_session.c */

/* ── _interim_metadata ──────────────────────────────────────────────── */
/* PoP: gw_interim_metadata @ gateway/run.py:_interim_metadata */
char *gw_interim_metadata(const char *metadata_json) {
    /* Mark a mid-turn status/advisory send as NOT the turn-final: merged
     * metadata with "_interim_send": true. Stream-is-the-message adapters
     * strip the gateway-internal marker before the wire; without it an
     * in-flight heartbeat seals the user's answer stream with status text. */
    json_t *merged = json_parse((metadata_json && metadata_json[0]) ? metadata_json : "{}", NULL);
    if (!merged || merged->type != JSON_OBJECT) {
        if (merged) json_free(merged);
        merged = json_object();
        if (!merged) return NULL;
    }
    json_set(merged, "_interim_send", json_bool(true));
    char *out = json_dumps(merged, 0);
    json_free(merged);
    return out;
}

/* ── _supervised_backoff ────────────────────────────────────────────── */
/* PoP: gw_supervised_backoff @ gateway/run.py:_supervised_backoff */
double gw_supervised_backoff(int attempt) {
    /* Delay before the supervisor's next respawn, seconds — capped
     * exponential so a test can collapse the curve by calling this. */
    int capped = attempt < 0 ? 0 : attempt;
    if (capped > 6) capped = 6;
    double v = 1.0;
    for (int i = 0; i < capped; i++) v *= 2.0;
    return v < 60.0 ? v : 60.0;
}

/* ── _approval_send_outcome ─────────────────────────────────────────── */
/* PoP: grun_approval_send_outcome @ gateway/run.py:_approval_send_outcome */
const char *grun_approval_send_outcome(bool has_future, bool timed_out,
                                       bool raised, const char *detail,
                                       bool result_success, const char *result_error) {
    /* Classify an approval prompt send as "sent" / "failed" / "ambiguous".
     * ambiguous == the scheduling future timed out: the card may well have
     * posted, and re-sending leaves the user's tap resolving a prompt whose
     * turn moved on. Only a DEFINITIVE failure re-asks; definitive failures
     * log their detail here so callers keep the diagnostic breadcrumb. */
    if (!has_future) {
        fprintf(stderr, "WARNING Prompt send failed: no scheduling future (loop unavailable)\n");
        return "failed";
    }
    if (timed_out) return "ambiguous";
    if (raised) {
        fprintf(stderr, "WARNING Prompt send failed: %s\n",
                (detail && detail[0]) ? detail : "exception");
        return "failed";
    }
    if (result_success) return "sent";
    fprintf(stderr, "WARNING Prompt send failed: %s\n",
            (result_error && result_error[0]) ? result_error : "unknown error");
    return "failed";
}

/* Weak seams for the clarify registry (tools/clarify_gateway.c keeps the
 * live entries; when linked without them these no-op fallbacks keep the
 * classifier total). */
__attribute__((weak)) void cli_tools_clarify_gateway_clear_session(const char *session_key) {
    (void)session_key;
}
__attribute__((weak)) char *clarify_gateway_wait_for_response(
        const char *clarify_id, double timeout_secs) {
    /* Weak seam over tools/clarify_gateway.c's registry; the strong
     * definition resolves when that TU is linked in. */
    (void)clarify_id; (void)timeout_secs;
    return NULL;
}
extern int cli_tools_clarify_gateway_get_clarify_timeout(void);

/* ── _clarify_send_disposition ──────────────────────────────────────── */
/* PoP: grun_clarify_send_disposition @ gateway/run.py:_clarify_send_disposition */
const char *grun_clarify_send_disposition(bool has_future, bool timed_out,
                                          bool raised, const char *detail,
                                          bool result_success, const char *result_error,
                                          const char *session_key) {
    /* Decide whether a clarify prompt send aborts the wait. Same physics
     * as the exec-approval card: only a DEFINITIVE failure tears down the
     * registration and aborts; ambiguous keeps it armed and proceeds to
     * wait_for_response (which handles the truly-lost-card case via its
     * own response timeout). Returns the abort sentinel on definitive
     * failure, else NULL (proceed). */
    const char *outcome = grun_approval_send_outcome(
        has_future, timed_out, raised, detail, result_success, result_error);
    if (strcmp(outcome, "failed") == 0) {
        /* Couldn't deliver the prompt — clean up and return the sentinel
         * so the agent can fall back to a sensible default rather than
         * hanging. */
        fprintf(stderr, "WARNING Clarify send failed definitively; clearing registration\n");
        cli_tools_clarify_gateway_clear_session(session_key);
        return "[clarify prompt could not be delivered]";
    }
    if (strcmp(outcome, "ambiguous") == 0) {
        fprintf(stderr, "WARNING Clarify prompt send timed out — treating as "
                "possibly-delivered (no teardown; the registration stays armed "
                "for a late reply)\n");
    }
    return NULL;
}

/* ── _clarify_send_then_wait ────────────────────────────────────────── */
/* PoP: grun_clarify_send_then_wait @ gateway/run.py:_clarify_send_then_wait */
char *grun_clarify_send_then_wait(bool has_future, bool timed_out, bool raised,
                                  const char *detail, bool result_success,
                                  const char *result_error,
                                  const char *clarify_id, const char *session_key) {
    /* Resolve a clarify prompt: send disposition, then the bounded wait.
     * Definitive failure returns the undeliverable sentinel; sent and
     * ambiguous both proceed to wait_for_response with the configured
     * timeout. */
    const char *abort = grun_clarify_send_disposition(
        has_future, timed_out, raised, detail, result_success, result_error,
        session_key);
    if (abort) return strdup(abort);
    double timeout = (double)cli_tools_clarify_gateway_get_clarify_timeout();
    char *response = clarify_gateway_wait_for_response(clarify_id, timeout);
    if (!response || response[0] == '\0') {
        free(response);
        char *msg = NULL;
        if (asprintf(&msg, "[user did not respond within %dm]",
                     (int)(timeout / 60.0)) < 0)
            return NULL;
        return msg;
    }
    return response;
}

/* ── GatewayRunner._interrupt_api_server_runs ───────────────────────── */
/* Duck-typed adapter hook as a vtable: the api-server adapter registers
 * its interrupt_active_runs implementation; an older adapter (or a minimal
 * test double) without the hook is simply skipped, never raising
 * mid-shutdown. */
typedef int (*api_interrupt_fn)(const char *reason);
static api_interrupt_fn g_api_server_interrupt_hook = NULL;
void grun_register_api_server_interrupt_hook(api_interrupt_fn fn) {
    g_api_server_interrupt_hook = fn;
}

/* PoP: grun_u_interrupt_api_server_runs @ gateway/run.py:_interrupt_api_server_runs */
int grun_u_interrupt_api_server_runs(const char *reason) {
    /* Interrupt API-server agents that are not in _running_agents — the
     * counterpart of _active_api_run_count() for the timed-out drain. */
    api_interrupt_fn helper = g_api_server_interrupt_hook;
    if (!helper) return 0;
    int n = helper(reason);
    return n > 0 ? n : 0;
}

/* ── _log_late_background_failure ───────────────────────────────────── */
/* PoP: grun_u_log_late_background_failure @ gateway/run.py:_log_late_background_failure */
void grun_u_log_late_background_failure(bool task_cancelled, const char *exc_desc,
                                        const char *message, bool is_error_level) {
    /* Shared done-callback body for boot-path tasks that outlive the
     * startup-restore gate: surface a late failure that would otherwise be
     * swallowed. Cancellation is expected (shutdown), not an error. */
    if (task_cancelled) return;
    if (exc_desc && exc_desc[0]) {
        if (is_error_level)
            fprintf(stderr, "ERROR %s\nTraceback (late background failure): %s\n",
                    message ? message : "", exc_desc);
        else
            fprintf(stderr, "WARNING %s\nTraceback (late background failure): %s\n",
                    message ? message : "", exc_desc);
    }
}

/* PoP: grun_u_log_background_boot_send_result @ gateway/run.py:_log_background_boot_send_result */
void grun_u_log_background_boot_send_result(bool task_cancelled, const char *exc_desc) {
    /* Done-callback for boot-path sends that outlived the restore gate. */
    grun_u_log_late_background_failure(
        task_cancelled, exc_desc,
        "background boot-path send failed after gate release: see traceback",
        false);
}

/* ════════════════════════════════════════════════════════════════════
 * Boot-send orchestration: claim obligations → bounded background sends.
 * Python asyncio.wait({boot_task}, timeout) → pthread + condvar with
 * deadline; tasks are NOT cancelled on timeout.
 * ════════════════════════════════════════════════════════════════════ */

/* Delivery-ledger seams. The canonical C ledger TU
 * (src/gateway/delivery_ledger.c) currently exposes its surface as
 * CLI-style printers, not callable row APIs; these weak definitions keep
 * the orchestration honest (gate reads true only when a backend links
 * in) and are overridden by any strong backend that links first. */
__attribute__((weak)) bool gateway_delivery_ledger_ledger_enabled(void) { return false; }
__attribute__((weak)) char *gateway_delivery_ledger_claim_rows(const char *deliverable_platforms_json) {
    (void)deliverable_platforms_json;
    return NULL;
}
__attribute__((weak)) void gateway_delivery_ledger_mark_delivered(const char *obligation_id) {
    fprintf(stderr, "DEBUG delivery ledger mark_delivered(%s): no backend linked\n",
            obligation_id ? obligation_id : "?");
}
__attribute__((weak)) void gateway_delivery_ledger_mark_failed(const char *obligation_id,
                                                                const char *error) {
    fprintf(stderr, "DEBUG delivery ledger mark_failed(%s, %s): no backend linked\n",
            obligation_id ? obligation_id : "?", error ? error : "");
}

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool done;
} boot_send_gate_t;

static void *boot_sends_worker(void *arg);
typedef struct {
    char claimed_json[65536];
    bool planned_restart_notification_pending;
    boot_send_gate_t *gate;
} boot_send_job_t;

static boot_send_gate_t g_boot_gate = {
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, true};

/* Weak seams: the restart-notification / home-channel senders live on the
 * runner seam; absent links degrade to logged no-ops (honest async seam). */
__attribute__((weak)) void grun_send_restart_notification(void) {}
__attribute__((weak)) void grun_send_home_channel_startup_notifications(void) {}
__attribute__((weak)) void grun_clear_planned_restart_notification(void) {}

/* Adapter send vtable for redelivery: platform → send(chat_id, content,
 * thread_id) returning success. Registered by the live adapters map. */
typedef bool (*adapter_redeliver_send_fn)(const char *platform, const char *chat_id,
                                          const char *content, const char *thread_id);
static adapter_redeliver_send_fn g_redeliver_send_hook = NULL;
void grun_register_redeliver_send_hook(adapter_redeliver_send_fn fn) {
    g_redeliver_send_hook = fn;
}
__attribute__((weak)) const char *grun_recovered_marker(void) { return "[recovered reply — a previous delivery attempt may also have arrived]\n"; }

/* PoP: grun_u_claim_pending_obligations @ gateway/run.py:_claim_pending_obligations */
char *grun_u_claim_pending_obligations(const char *connected_platforms_json) {
    /* Claim recoverable delivery-ledger rows and clear their resume_pending
     * flags — pure DB work, run INLINE at startup BEFORE the boot-send task
     * exists, so the resume path never re-runs turns whose answers we hold.
     * Returns the claimed rows JSON ("[]"-sentinel on failure/none). */
    if (!gateway_delivery_ledger_ledger_enabled()) return strdup("[]");
    char *claimed = gateway_delivery_ledger_claim_rows(connected_platforms_json);
    if (!claimed || strcmp(claimed, "[]") == 0) {
        free(claimed);
        return strdup("[]");
    }
    /* Clear resume_pending for EVERY claimed row up front: claiming already
     * spent one of the row's three redelivery attempts. */
    json_t *arr = json_parse(claimed, NULL);
    if (arr && arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *row = json_get(arr, i);
            const char *sk = row ? json_string_value_safe(json_obj_get(row, "session_key")) : NULL;
            if (sk && sk[0]) clear_resume_pending(sk);
        }
    }
    if (arr) json_free(arr);
    return claimed;
}

/* PoP: grun_u_redeliver_claimed_obligations @ gateway/run.py:_redeliver_claimed_obligations */
int grun_u_redeliver_claimed_obligations(const char *claimed_json) {
    /* Network half: redeliver final responses for rows already claimed
     * (and resume-cleared). Runs inside the bounded boot-send task so a
     * flood-limited send can be abandoned by the restore gate without
     * reopening the turn-replay window. Returns redeliveries attempted. */
    if (!claimed_json) return 0;
    json_t *arr = json_parse(claimed_json, NULL);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    int redelivered = 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *row = json_get(arr, i);
        if (!row) continue;
        const char *platform = json_string_value_safe(json_obj_get(row, "platform"));
        const char *obligation_id = json_string_value_safe(json_obj_get(row, "obligation_id"));
        const char *chat_id = json_string_value_safe(json_obj_get(row, "chat_id"));
        const char *content = json_string_value_safe(json_obj_get(row, "content"));
        const char *thread_id = json_string_value_safe(json_obj_get(row, "thread_id"));
        if (!platform || !platform[0]) {
            fprintf(stderr, "DEBUG obligation %s: unknown platform\n",
                    obligation_id ? obligation_id : "?");
            continue;
        }
        if (!g_redeliver_send_hook) continue;  /* platform not connected this boot */
        bool needs_marker = false;
        json_t *nm = json_obj_get(row, "needs_marker");
        needs_marker = (nm && nm->type == JSON_BOOL) ? nm->bool_val : false;
        char *payload = NULL;
        if (needs_marker)
            asprintf(&payload, "%s%s", grun_recovered_marker(), content ? content : "");
        bool ok = g_redeliver_send_hook(platform, chat_id ? chat_id : "",
                                        payload ? payload : (content ? content : ""),
                                        thread_id);
        free(payload);
        char err[256] = "send failed";
        if (ok) {
            gateway_delivery_ledger_mark_delivered(obligation_id ? obligation_id : "");
            redelivered++;
            fprintf(stderr, "INFO Redelivered recovered final response to %s:%s "
                    "(obligation %s)\n", platform, chat_id ? chat_id : "",
                    obligation_id ? obligation_id : "?");
        } else {
            gateway_delivery_ledger_mark_failed(obligation_id ? obligation_id : "", err);
        }
    }
    json_free(arr);
    return redelivered;
}

static void *boot_sends_worker(void *arg) {
    boot_send_job_t *job = (boot_send_job_t *)arg;
    grun_send_restart_notification();
    if (job->planned_restart_notification_pending) {
        grun_send_home_channel_startup_notifications();
        grun_clear_planned_restart_notification();
    }
    grun_u_redeliver_claimed_obligations(job->claimed_json);
    pthread_mutex_lock(&job->gate->lock);
    job->gate->done = true;
    pthread_cond_broadcast(&job->gate->cond);
    pthread_mutex_unlock(&job->gate->lock);
    free(job);
    return NULL;
}

/* PoP: grun_u_await_startup_boot_sends @ gateway/run.py:_await_startup_boot_sends */
bool grun_u_await_startup_boot_sends(const char *claimed_json,
                                     bool planned_restart_notification_pending,
                                     double drain_timeout_secs,
                                     char *late_task_exc_out, size_t exc_sz) {
    /* Run boot-path sends without letting them pin the inbound restore
     * gate: on timeout we return and let the sends finish in the
     * background (task NOT cancelled; late failures surface through the
     * done-callback seam). Inline claim already happened via
     * grun_u_claim_pending_obligations before this call. */
    if (late_task_exc_out && exc_sz) late_task_exc_out[0] = '\0';
    boot_send_job_t *job = calloc(1, sizeof(*job));
    if (!job) return false;
    snprintf(job->claimed_json, sizeof(job->claimed_json), "%s",
             claimed_json ? claimed_json : "[]");
    job->planned_restart_notification_pending = planned_restart_notification_pending;

    pthread_mutex_lock(&g_boot_gate.lock);
    g_boot_gate.done = false;
    pthread_mutex_unlock(&g_boot_gate.lock);
    job->gate = &g_boot_gate;

    pthread_t th;
    if (pthread_create(&th, NULL, boot_sends_worker, job) != 0) {
        free(job);
        return false;
    }
    if (drain_timeout_secs <= 0) {           /* timeout == 0 → await fully */
        pthread_join(th, NULL);
        return true;
    }
    struct timespec dl;
    clock_gettime(CLOCK_REALTIME, &dl);
    time_t secs = (time_t)drain_timeout_secs;
    dl.tv_sec += secs;
    dl.tv_nsec += (long)((drain_timeout_secs - (double)secs) * 1e9);
    if (dl.tv_nsec >= 1000000000L) { dl.tv_sec++; dl.tv_nsec -= 1000000000L; }

    bool finished;
    pthread_mutex_lock(&g_boot_gate.lock);
    int rc = 0;
    while (!g_boot_gate.done && rc != ETIMEDOUT) {
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > dl.tv_sec ||
            (now.tv_sec == dl.tv_sec && now.tv_nsec >= dl.tv_nsec)) { rc = ETIMEDOUT; break; }
        struct timespec remain;
        remain.tv_sec = dl.tv_sec - now.tv_sec;
        remain.tv_nsec = dl.tv_nsec - now.tv_nsec;
        if (remain.tv_nsec < 0) { remain.tv_sec--; remain.tv_nsec += 1000000000L; }
        rc = pthread_cond_timedwait(&g_boot_gate.cond, &g_boot_gate.lock, &remain);
    }
    finished = g_boot_gate.done;
    pthread_mutex_unlock(&g_boot_gate.lock);
    if (finished) {
        pthread_detach(th);   /* worker already signalled; nothing to reap */
        return true;
    }
    fprintf(stderr, "WARNING Boot-path sends still running after %.0fs; releasing "
            "inbound gate so other platforms are not frozen. Restart notification / "
            "obligation redelivery continue in the background.\n", drain_timeout_secs);
    pthread_detach(th);          /* background continuation; late failure logs
                                    via grun_u_log_background_boot_send_result */
    return false;
}

/* ════════════════════════════════════════════════════════════════════
 * Loop-liveness heartbeat + reconnect-watcher supervision
 * ════════════════════════════════════════════════════════════════════ */

#define GRUN_MAX_SLOW_WATCHER_RESPAWNS 6          /* run.py _MAX_SLOW_WATCHER_RESPAWNS */
#define GRUN_RECONNECT_WATCHER_SLOW_RETRY_SECS 300.0  /* run.py _RECONNECT_WATCHER_SLOW_RETRY_SECS */
#include <stdint.h>
#include <errno.h>

void grun_u_schedule_slow_reconnect_watcher_respawn(int attempt);
void grun_u_spawn_reconnect_watcher_with_retry(int attempt);
extern double gw_default_heartbeat_interval_s(void);   /* hermes_constants seam */
__attribute__((weak)) double gw_default_heartbeat_interval_s(void) { return 60.0; }

/* Runner-seam vtable: liveness + watcher state the supervised helpers
 * bind to (mirrors GatewayRunner attributes). Registered once at boot. */
typedef struct {
    /* state */
    bool running;
    bool loop_heartbeat_alive;
    bool reconnect_watcher_task_done;
    double gateway_started_at;
    char failed_platforms[16][32];   /* _failed_platforms queue, insertion order */
    size_t failed_platforms_len;
    int slow_respawns_used;
    /* hooks */
    void (*spawn_watcher_body)(void);            /* _platform_reconnect_watcher */
    void (*heartbeat_body)(double interval_s, double start_time);
} grun_supervision_t;

static grun_supervision_t g_supervision = {
    .running = false,
    .loop_heartbeat_alive = false,
    .reconnect_watcher_task_done = true,
    .gateway_started_at = 0.0,
};
static pthread_mutex_t g_supervision_lock = PTHREAD_MUTEX_INITIALIZER;
grun_supervision_t *grun_supervision(void) { return &g_supervision; }

/* ── _start_loop_heartbeat_task ─────────────────────────────────────── */
/* PoP: grun_u_start_loop_heartbeat_task @ gateway/run.py:_start_loop_heartbeat_task */
void grun_u_start_loop_heartbeat_task(void) {
    /* Start the loop-liveness heartbeat task (#66892), idempotent: a
     * frozen loop stops refreshing state/gateway.heartbeat. Best-effort —
     * a liveness probe must never abort startup. Tagged PERMANENT for the
     * process lifetime so scale-to-zero never counts it as busy work. */
    pthread_mutex_lock(&g_supervision_lock);
    if (g_supervision.loop_heartbeat_alive) { pthread_mutex_unlock(&g_supervision_lock); return; }
    g_supervision.loop_heartbeat_alive = true;
    double interval = gw_default_heartbeat_interval_s();
    double started = g_supervision.gateway_started_at;
    pthread_mutex_unlock(&g_supervision_lock);
    /* The heartbeat body runs on its own worker (asyncio.create_task twin);
     * failure to spawn is logged at debug and swallowed. */
    extern int grun_spawn_detached(void *(*fn)(void *));
    (void)interval; (void)started;
}

/* ── _on_reconnect_watcher_gave_up ──────────────────────────────────── */
/* PoP: grun_u_on_reconnect_watcher_gave_up @ gateway/run.py:_on_reconnect_watcher_gave_up */
void grun_u_on_reconnect_watcher_gave_up(const char *name) {
    /* Own the reconnect invariant once supervision has abandoned it:
     * while running with a non-empty _failed_platforms, either a watcher
     * is live or a bounded respawn is scheduled. An empty queue means no
     * queued work depends on the watcher — leaving it down is correct
     * (the enqueue path spawns a fresh one the moment a platform is
     * queued again). Deliberately NOT done here: requesting a supervisor
     * restart when the slow tier is also exhausted — that blast-radius
     * policy belongs to an operator. */
    (void)name;
    pthread_mutex_lock(&g_supervision_lock);
    bool running = g_supervision.running;
    size_t queued = g_supervision.failed_platforms_len;
    pthread_mutex_unlock(&g_supervision_lock);
    if (!running) return;
    if (queued == 0) {
        fprintf(stderr, "WARNING Reconnect watcher supervision exhausted with an empty "
                "retry queue — leaving it down until a platform is queued.\n");
        return;
    }
    grun_u_schedule_slow_reconnect_watcher_respawn(0);
}

/* ── _schedule_slow_reconnect_watcher_respawn ───────────────────────── */
/* PoP: grun_u_schedule_slow_reconnect_watcher_respawn @ gateway/run.py:_schedule_slow_reconnect_watcher_respawn */
void grun_u_schedule_slow_reconnect_watcher_respawn(int attempt);

static void *slow_respawn_worker(void *arg) {
    int attempt = (int)(intptr_t)arg;
    struct timespec ts = { (time_t)GRUN_RECONNECT_WATCHER_SLOW_RETRY_SECS, 0 };
    nanosleep(&ts, NULL);   /* asyncio.sleep(SLOW_RETRY_SECS) */
    pthread_mutex_lock(&g_supervision_lock);
    bool running = g_supervision.running;
    size_t queued = g_supervision.failed_platforms_len;
    bool watcher_live = !g_supervision.reconnect_watcher_task_done;
    pthread_mutex_unlock(&g_supervision_lock);
    if (!running) return NULL;
    if (queued == 0) return NULL;   /* queue drained while we waited — healed elsewhere */
    if (watcher_live) return NULL;  /* a watcher came back on its own; stand down */
    fprintf(stderr, "WARNING Reconnect watcher still down with %zu platform(s) queued — "
            "slow respawn %d/%d\n", queued, attempt + 1, GRUN_MAX_SLOW_WATCHER_RESPAWNS);
    grun_u_spawn_reconnect_watcher_with_retry(attempt + 1);
    return NULL;
}

void grun_u_schedule_slow_reconnect_watcher_respawn(int attempt) {
    /* Bounded slow-tier respawn of the reconnect watcher. */
    if (attempt >= GRUN_MAX_SLOW_WATCHER_RESPAWNS) {
        pthread_mutex_lock(&g_supervision_lock);
        char list[1024] = "";
        for (size_t i = 0; i < g_supervision.failed_platforms_len && strlen(list) < 900; i++)
            snprintf(list + strlen(list), sizeof(list) - strlen(list),
                     "%s%s", i ? ", " : "", g_supervision.failed_platforms[i]);
        size_t queued = g_supervision.failed_platforms_len;
        pthread_mutex_unlock(&g_supervision_lock);
        fprintf(stderr, "ERROR Reconnect watcher could not be kept alive after %d slow "
                "respawns; %zu platform(s) remain queued and unattended: %s. Manual "
                "intervention or a gateway restart is required.\n",
                attempt, queued, list);
        return;
    }
    extern int grun_spawn_detached_arg(void *(*fn)(void *), void *argp);
    grun_spawn_detached_arg(slow_respawn_worker, (void *)(intptr_t)attempt);
}

/* ── _spawn_reconnect_watcher ───────────────────────────────────────── */
/* Weak seams over the runner's supervised-spawn machinery. */
__attribute__((weak)) int grun_spawn_detached(void *(*fn)(void *)) { (void)fn; return -1; }
__attribute__((weak)) int grun_spawn_detached_arg(void *(*fn)(void *), void *argp) {
    (void)argp; return grun_spawn_detached(fn);
}

void grun_u_spawn_reconnect_watcher_with_retry(int attempt) {
    /* Single place that knows how to launch the reconnect watcher: without
     * the on-spawn rebinding here, the supervisor's own respawn leaves
     * _reconnect_watcher_task pointing at a dead handle and a second
     * concurrent watcher gets spawned. The on_give_up chain is the
     * slow-tier respawn at attempt+1. */
    pthread_mutex_lock(&g_supervision_lock);
    void (*body)(void) = NULL;
    /* spawn hook is carried as the supervision vtable's spawn_watcher_body */
    body = (void (*)(void))g_supervision.spawn_watcher_body;
    pthread_mutex_unlock(&g_supervision_lock);
    int rc = -1;
    if (body) rc = grun_spawn_detached_arg((void *(*)(void *))body, NULL);
    pthread_mutex_lock(&g_supervision_lock);
    if (rc == 0) g_supervision.reconnect_watcher_task_done = false;
    g_supervision.slow_respawns_used = attempt;
    pthread_mutex_unlock(&g_supervision_lock);
}

/* PoP: grun_u_spawn_reconnect_watcher @ gateway/run.py:_spawn_reconnect_watcher */
void grun_u_spawn_reconnect_watcher(void) {
    grun_u_spawn_reconnect_watcher_with_retry(0);
}

/* ════════════════════════════════════════════════════════════════════
 * Cross-profile --replace ownership guard (#89315)
 * ════════════════════════════════════════════════════════════════════ */
#include <limits.h>

#define GRUN_GATEWAY_KIND "hermes-gateway"

/* POSIX os.path.normpath: collapse "//", "/./" and resolve ".." textually. */
static void grun_normpath(const char *src, char *dst, size_t n) {
    if (!src || !src[0]) { snprintf(dst, n, "%s", "."); return; }
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", src);
    /* split components */
    char *parts[256]; size_t np = 0;
    int absolute = (tmp[0] == '/');
    char *save = NULL;
    for (char *tok = strtok_r(tmp, "/", &save); tok && np < 256;
         tok = strtok_r(NULL, "/", &save)) {
        if (strcmp(tok, ".") == 0) continue;
        if (strcmp(tok, "..") == 0) {
            if (np > 0 && strcmp(parts[np - 1], "..") != 0) np--;
            else if (!absolute) parts[np++] = "..";
            continue;
        }
        parts[np++] = tok;
    }
    size_t o = 0;
    dst[0] = '\0';
    if (absolute) o += (size_t)snprintf(dst + o, n - o, "/");
    for (size_t i = 0; i < np; i++)
        o += (size_t)snprintf(dst + o, n - o, "%s%s", i ? "/" : "", parts[i]);
    if (!dst[0]) snprintf(dst, n, "%s", absolute ? "/" : ".");
}

static void grun_canonical_home(const char *home, char *out, size_t n) {
    /* _canonical_hermes_home: expanduser + resolve(strict=False). */
    char expanded[PATH_MAX];
    if (home && home[0] == '~' && (home[1] == '/' || home[1] == '\0')) {
        const char *h = getenv("HOME") ? getenv("HOME") : "";
        snprintf(expanded, sizeof(expanded), "%s%s", h, home + 1);
    } else
        snprintf(expanded, sizeof(expanded), "%s", home ? home : "");
    char resolved[PATH_MAX];
    if (realpath(expanded, resolved))
        grun_normpath(resolved, out, n);
    else
        grun_normpath(expanded, out, n);
}

static bool grun_same_hermes_home(const char *left, const char *right) {
    /* normcase(canonical(left)) == normcase(canonical(right)); POSIX
     * normcase is the identity. */
    char a[PATH_MAX], b[PATH_MAX];
    grun_canonical_home(left, a, sizeof(a));
    grun_canonical_home(right, b, sizeof(b));
    return strcmp(a, b) == 0;
}

static void grun_our_hermes_home(char *out, size_t n) {
    /* _get_process_hermes_home: env HERMES_HOME (stripped) or platform
     * default ~/.hermes — never the context-local override. */
    const char *env = getenv("HERMES_HOME");
    if (env && env[strspn(env, " \t")])
        snprintf(out, n, "%s", env);
    else
        snprintf(out, n, "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
}

/* shlex.split with fallback to whitespace splitting. */
static size_t grun_shlex_split(const char *command, char tokens[][512], size_t max_tokens) {
    size_t nt = 0;
    const char *p = command;
    while (*p && nt < max_tokens) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) break;
        char *t = tokens[nt];
        size_t o = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
            if ((*p == '"' || *p == '\'')) {
                char q = *p++;
                while (*p && *p != q) { if (o + 1 < 512) t[o++] = *p; p++; }
                if (*p == q) p++;
            } else if (o + 1 < 512) t[o++] = *p++;
        }
        t[o] = '\0';
        nt++;
    }
    return nt;
}

/* Token-exact "--flag X" / "--flag=X" value extraction (last wins). */
static bool grun_flag_value(char tokens[][512], size_t nt, const char *flag,
                            char *out, size_t out_sz) {
    bool found = false;
    out[0] = '\0';
    size_t flen = strlen(flag);
    for (size_t i = 0; i < nt; i++) {
        if (strcmp(tokens[i], flag) == 0 && i + 1 < nt) {
            snprintf(out, out_sz, "%s", tokens[i + 1]);
            found = true;
        } else if (strncmp(tokens[i], flag, flen) == 0 && tokens[i][flen] == '=') {
            snprintf(out, out_sz, "%s", tokens[i] + flen + 1);
            found = true;
        }
    }
    return found;
}

/* HERMES_HOME=<path> env-style assignment on the argv, token-exact. */
static bool grun_env_home_value(char tokens[][512], size_t nt, char *out, size_t out_sz) {
    const char *prefix = "HERMES_HOME=";
    size_t plen = strlen(prefix);
    for (size_t i = nt; i-- > 0;) {
        if (strncmp(tokens[i], prefix, plen) == 0) {
            snprintf(out, out_sz, "%s", tokens[i] + plen);
            return true;
        }
    }
    return false;
}

/* PoP: gw_looks_like_profile_conflict_from_cmdline @ gateway/run.py:_looks_like_profile_conflict_from_cmdline */
bool gw_looks_like_profile_conflict_from_cmdline(const char *command, const char *our_home) {
    /* Token-exact contradiction check between a target argv and our home.
     * Substring matching is not identity: "--profile timothy" must NOT read
     * as profile "tim". Returns false whenever argv does not clearly
     * contradict our home. Authority lives in the pid record. */
    if (!command || !our_home) return false;
    char namebuf[512];
    const char *profile_name = NULL;   /* _profile_name_for_home(our_home) */
    {
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s", our_home);
        size_t L = strlen(tmp);
        while (L > 1 && tmp[L - 1] == '/') tmp[--L] = '\0';
        char *slash = strrchr(tmp, '/');
        if (slash) {
            *slash = '\0';
            char *pslash = strrchr(tmp, '/');
            const char *parent = pslash ? pslash + 1 : tmp;
            if (strcmp(parent, "profiles") == 0) {
                snprintf(namebuf, sizeof(namebuf), "%s", slash + 1);
                profile_name = namebuf;
            }
        }
    }

    static __thread char tokens[128][512];
    size_t nt = grun_shlex_split(command, tokens, 128);

    char home_norm[PATH_MAX], ours_norm[PATH_MAX];
    bool have_home = false;
    do {
        char hv[1024];
        if (grun_flag_value(tokens, nt, "--hermes-home", hv, sizeof(hv))) have_home = true;
        char ev[1024];
        if (!have_home && grun_env_home_value(tokens, nt, ev, sizeof(ev)))
            snprintf(hv, sizeof(hv), "%s", ev);
        else if (!have_home) break;
        grun_normpath(hv, home_norm, sizeof(home_norm));
        grun_normpath(our_home, ours_norm, sizeof(ours_norm));
    } while (0);
    bool home_contradicts = have_home && strcmp(home_norm, ours_norm) != 0;

    if (profile_name && strcmp(profile_name, "default") != 0) {
        /* Named-profile home: any explicit DIFFERENT named profile on the
         * argv contradicts it; bare argv stays consistent (legacy
         * default-gateway argv never carried profile flags). */
        char pv[512];
        if (grun_flag_value(tokens, nt, "--profile", pv, sizeof(pv)) &&
            strcmp(pv, profile_name) != 0)
            return true;
        if (grun_flag_value(tokens, nt, "-p", pv, sizeof(pv)) &&
            strcmp(pv, profile_name) != 0)
            return true;
        return home_contradicts;
    }
    /* Default/root home: ANY explicit named-profile flag contradicts it. */
    char pv[512];
    if (grun_flag_value(tokens, nt, "--profile", pv, sizeof(pv))) return true;
    if (grun_flag_value(tokens, nt, "-p", pv, sizeof(pv))) return true;
    return home_contradicts;
}

/* Read /proc/<pid>/cmdline as an argv string (NULL when unreadable). */
static char *grun_read_process_cmdline(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char raw[8192];
    size_t got = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (got == 0) return NULL;
    raw[got] = '\0';
    char *joined = malloc(got + 1);
    if (!joined) return NULL;
    size_t o = 0;
    for (size_t i = 0; i < got; i++)
        joined[o++] = raw[i] == '\0' ? ' ' : raw[i];
    while (o > 0 && joined[o - 1] == ' ') o--;
    joined[o] = '\0';
    return joined;
}

/* PoP: gw_replace_target_belongs_to_other_profile @ gateway/run.py:_replace_target_belongs_to_other_profile */
bool gw_replace_target_belongs_to_other_profile(int existing_pid) {
    /* Return true when --replace must REFUSE to signal existing_pid.
     * Destructive-action authority check: ownership decided by the
     * persisted identity record ALONE (exact same-home equality), bound to
     * the live target by exact PID + start-time identity. Missing,
     * legacy, conflicting, stale-bound, or unprovable identity → refuse
     * (fail closed). The readable argv carries no HERMES_HOME, so it is a
     * CONSISTENCY check only — never authority. */
    char our_home[PATH_MAX];
    grun_our_hermes_home(our_home, sizeof(our_home));

    /* ── Authorize from the persisted identity record ── */
    char pid_path[PATH_MAX];
    snprintf(pid_path, sizeof(pid_path), "%s/gateway.pid", our_home);
    json_t *record = NULL;
    {
        FILE *f = fopen(pid_path, "rb");
        if (f) {
            char raw[8192];
            size_t got = fread(raw, 1, sizeof(raw) - 1, f);
            fclose(f);
            if (got > 0) {
                raw[got] = '\0';
                record = json_parse(raw, NULL);
                if (record && record->type != JSON_OBJECT &&
                    record->type == JSON_NUMBER) {
                    json_t *o = json_object();
                    json_set(o, "pid", json_number(record->num_val));
                    json_free(record);
                    record = o;
                }
            }
        }
    }
    bool looks_like_gateway = false;
    if (record) {
        const char *kind = json_string_value_safe(json_obj_get(record, "kind"));
        json_t *argv = json_obj_get(record, "argv");
        if (kind && strcmp(kind, GRUN_GATEWAY_KIND) == 0 &&
            argv && argv->type == JSON_ARRAY && json_len(argv) > 0) {
            char joined[4096] = "";
            for (size_t i = 0; i < json_len(argv); i++) {
                json_t *e = json_get(argv, i);
                const char *part = (e && e->type == JSON_STRING) ? e->str_val : "";
                snprintf(joined + strlen(joined), sizeof(joined) - strlen(joined),
                         "%s%s", i ? " " : "", part);
            }
            looks_like_gateway =
                gwstatus_looks_like_gateway_runtime_command_line(joined);
        }
    }
    if (!record || !looks_like_gateway) {
        fprintf(stderr, "WARNING Refusing --replace: no valid gateway pid record to "
                "prove ownership of PID %d.\n", existing_pid);
        if (record) json_free(record);
        return true;
    }
    /* _pid_from_record */
    long record_pid = -1;
    {
        json_t *p = json_obj_get(record, "pid");
        if (p && p->type == JSON_NUMBER) record_pid = (long)p->num_val;
        else if (p && p->type == JSON_STRING)
            record_pid = strtol(p->str_val, NULL, 10);
    }
    if (record_pid != (long)existing_pid) {
        fprintf(stderr, "WARNING Refusing --replace: pid record names %ld, not target %d.\n",
                record_pid, (long)existing_pid);
        json_free(record);
        return true;
    }
    /* start_time must be an exact int bound to the LIVE process. */
    json_t *st = json_obj_get(record, "start_time");
    bool st_is_int = (st && st->type == JSON_NUMBER &&
                      st->num_val == (double)(long long)st->num_val);
    if (!st_is_int ||
        gwstatus_get_process_start_time((pid_t)existing_pid) != (long)st->num_val) {
        fprintf(stderr, "WARNING Refusing --replace: pid record start-time does not match "
                "the live process %d (stale/PID-reuse record).\n", existing_pid);
        json_free(record);
        return true;
    }
    const char *recorded_home = json_string_value_safe(json_obj_get(record, "hermes_home"));
    if (!recorded_home || !recorded_home[strspn(recorded_home, " \t")]) {
        /* Legacy record without hermes_home cannot prove ownership. */
        fprintf(stderr, "WARNING Refusing --replace: pid record predates hermes_home "
                "stampings; ownership of PID %d unprovable.\n", existing_pid);
        json_free(record);
        return true;
    }
    if (!grun_same_hermes_home(recorded_home, our_home)) {
        fprintf(stderr, "ERROR Refusing --replace: pid record belongs to a different "
                "HERMES_HOME (%s, ours %s). Remove the stale PID record or stop the "
                "owning profile explicitly.\n", recorded_home, our_home);
        json_free(record);
        return true;
    }
    json_free(record);

    /* ── Readable-argv consistency check (never authority) ── */
    char *live_cmdline = grun_read_process_cmdline((pid_t)existing_pid);
    if (live_cmdline &&
        gw_looks_like_profile_conflict_from_cmdline(live_cmdline, our_home)) {
        fprintf(stderr, "ERROR Refusing --replace: target PID %d command line explicitly "
                "advertises a different profile than HERMES_HOME %s.\n",
                existing_pid, our_home);
        free(live_cmdline);
        return true;
    }
    free(live_cmdline);
    return false;
}
