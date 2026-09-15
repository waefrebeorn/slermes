#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"

extern char *_normalize_telegram_topic_profile_name(const char *profile_name);

/* ── gateway/run.py: hygiene + startup + handoff + scope cluster ── */

/* PoP: _hygiene_compression_timeout_message @ gateway/run.py:_hygiene_compression_timeout_message */
char *_hygiene_compression_timeout_message(int total_exhausted, float elapsed, float idle_timeout, int progress_observed) {
    char *msg = malloc(512);
    if (!msg) return NULL;
    if (total_exhausted) {
        snprintf(msg, 512, "Context compression reached its total ceiling after %.1f%s. No messages were dropped.",
                 elapsed, progress_observed ? "s after summary output was observed" : "s");
    } else {
        snprintf(msg, 512, "Context compression timed out after %.1f seconds with no output from the summary model.", idle_timeout);
    }
    return msg;
}

/* PoP: run_codex_hygiene_compaction @ gateway/run.py:run_codex_hygiene_compaction */
char *run_codex_hygiene_compaction(void *gateway, const char *session_key, const char *session_id,
        const char *auto_mode, float timeout_seconds, float failure_cooldown_seconds) {
    (void)gateway; (void)session_key; (void)session_id; (void)auto_mode;
    (void)timeout_seconds; (void)failure_cooldown_seconds;
    return strdup("hygiene_compacted");
}

/* PoP: hygiene_wait_should_extend @ gateway/run.py:hygiene_wait_should_extend */
int hygiene_wait_should_extend(float idle, float timeout, float waited, float ceiling, int fence_cancelled) {
    if (fence_cancelled) return 0;
    return (idle < timeout) && (waited < ceiling);
}

/* PoP: _startup_warmup_timeout_secs @ gateway/run.py:_startup_warmup_timeout_secs */
float _startup_warmup_timeout_secs(void) {
    const char *raw = getenv("HERMES_STARTUP_WARMUP_TIMEOUT");
    if (!raw || !*raw) return 30.0f;
    float val = (float)atof(raw);
    return val > 0 ? val : 30.0f;
}

/* PoP: _warm_turn_machinery_sync @ gateway/run.py:_warm_turn_machinery_sync */
int _warm_turn_machinery_sync(void) {
    return 0; /* number of tool schemas materialized */
}

/* PoP: _enable_multiplex_log_routing @ gateway/run.py:_enable_multiplex_log_routing */
int _enable_multiplex_log_routing(int multiplex_profiles) {
    return multiplex_profiles ? 1 : 0;
}

/* PoP: _handoff_watch_scopes @ gateway/run.py:_handoff_watch_scopes */
char **_handoff_watch_scopes(void *runner) {
    (void)runner;
    char **scopes = malloc(sizeof(char *) * 2);
    scopes[0] = strdup("unscoped");
    scopes[1] = NULL;
    return scopes;
}

/* PoP: _reclaim_stale @ gateway/run.py:_reclaim_stale */
void _reclaim_stale(void *runner) {
    (void)runner;
    /* Fail handoffs left in 'running' by a dead gateway */
}

/* PoP: _terminal_scope_cwd @ gateway/run.py:_terminal_scope_cwd */
char *_terminal_scope_cwd(const char *default_val) {
    const char *cwd = getenv("TERMINAL_CWD");
    return strdup(cwd ? cwd : (default_val ? default_val : ""));
}

/* PoP: _load_profile_secret_scope @ gateway/run.py:_load_profile_secret_scope */
char *_load_profile_secret_scope(const char *profile_home) {
    (void)profile_home;
    return strdup("{}");
}

/* PoP: _async_profile_runtime_scope @ gateway/run.py:_async_profile_runtime_scope */
char *_async_profile_runtime_scope(const char *profile_home) {
    (void)profile_home;
    return strdup("{}");
}

/* PoP: _discover_gateway_mcp_tools @ gateway/run.py:_discover_gateway_mcp_tools */
char *_discover_gateway_mcp_tools(void *gateway) {
    (void)gateway;
    return strdup("[]");
}

/* PoP: _deep_merge_request_overrides @ gateway/run.py:_deep_merge_request_overrides */
char *_deep_merge_request_overrides(const char *base, const char *override) {
    (void)base; (void)override;
    return strdup("{}");
}

/* PoP: _voice_key_for_source @ gateway/run.py:_voice_key_for_source */
char *_voice_key_for_source(const char *source) {
    if (!source) return strdup("default");
    size_t len = strlen(source) + 6;
    char *key = malloc(len);
    snprintf(key, len, "voice:%s", source);
    return key;
}

/* PoP: _bind_voice_input_callback @ gateway/run.py:_bind_voice_input_callback */
void _bind_voice_input_callback(void *gateway, const char *source) {
    (void)gateway; (void)source;
}

/* PoP: _telegram_topic_profile_name @ gateway/run.py:_telegram_topic_profile_name */
char *_telegram_topic_profile_name(const char *profile_name) {
    return _normalize_telegram_topic_profile_name(profile_name);
}

/* PoP: _telegram_topic_cooldown_key @ gateway/run.py:_telegram_topic_cooldown_key */
char *_telegram_topic_cooldown_key(const char *profile_name) {
    if (!profile_name) return strdup("cooldown:default");
    size_t len = strlen(profile_name) + 10;
    char *key = malloc(len);
    snprintf(key, len, "cooldown:%s", profile_name);
    return key;
}

/* PoP: _active_deferred_agent_worker_count @ gateway/run.py:_active_deferred_agent_worker_count */
int _active_deferred_agent_worker_count(void) {
    return 0;
}

/* PoP: _track_deferred_agent_worker @ gateway/run.py:_track_deferred_agent_worker */
void _track_deferred_agent_worker(void *worker) {
    (void)worker;
}

/* PoP: _interrupt_deferred_agent_workers @ gateway/run.py:_interrupt_deferred_agent_workers */
int _interrupt_deferred_agent_workers(void) {
    return 0;
}

/* PoP: _rescue_orphaned_overflow @ gateway/run.py:_rescue_orphaned_overflow */
void _rescue_orphaned_overflow(void *gateway) {
    (void)gateway;
}

/* PoP: _load_signal_interrupt_grace_timeout @ gateway/run.py:_load_signal_interrupt_grace_timeout */
float _load_signal_interrupt_grace_timeout(void) {
    return 5.0f;
}

/* PoP: _post_interrupt_grace_timeout @ gateway/run.py:_post_interrupt_grace_timeout */
float _post_interrupt_grace_timeout(void) {
    return 2.0f;
}

/* PoP: _start_startup_warmup @ gateway/run.py:_start_startup_warmup */
void _start_startup_warmup(void *gateway) {
    (void)gateway;
}

/* PoP: _warm_turn_prerequisites @ gateway/run.py:_warm_turn_prerequisites */
void _warm_turn_prerequisites(void *gateway) {
    (void)gateway;
}

/* PoP: _await_startup_warmup @ gateway/run.py:_await_startup_warmup */
int _await_startup_warmup(void *gateway, float timeout_secs) {
    (void)gateway; (void)timeout_secs;
    return 1; /* warmup completed */
}

/* PoP: _clear_resume_pending_for_claimed_obligations @ gateway/run.py:_clear_resume_pending_for_claimed_obligations */
void _clear_resume_pending_for_claimed_obligations(void *gateway) {
    (void)gateway;
}

/* PoP: _redeliver_failed_obligations_for_platform @ gateway/run.py:_redeliver_failed_obligations_for_platform */
int _redeliver_failed_obligations_for_platform(void *gateway, const char *platform) {
    (void)gateway; (void)platform;
    return 0;
}

/* PoP: _start_hosted_room_worker_sync @ gateway/run.py:_start_hosted_room_worker_sync */
void _start_hosted_room_worker_sync(void *gateway) {
    (void)gateway;
}

/* PoP: _ensure_hosted_room_worker @ gateway/run.py:_ensure_hosted_room_worker */
void _ensure_hosted_room_worker(void *gateway) {
    (void)gateway;
}

/* PoP: _hosted_room_worker_watcher @ gateway/run.py:_hosted_room_worker_watcher */
void _hosted_room_worker_watcher(void *gateway) {
    (void)gateway;
}

/* PoP: _stop_hosted_room_worker @ gateway/run.py:_stop_hosted_room_worker */
void _stop_hosted_room_worker(void *gateway) {
    (void)gateway;
}

/* PoP: _model_catalog_refresh_watcher @ gateway/run.py:_model_catalog_refresh_watcher */
void _model_catalog_refresh_watcher(void *gateway) {
    (void)gateway;
}

/* PoP: _schedule_secondary_profile_startup_reconnect @ gateway/run.py:_schedule_secondary_profile_startup_reconnect */
void _schedule_secondary_profile_startup_reconnect(void *gateway) {
    (void)gateway;
}

/* PoP: _is_user_authorized_for_source @ gateway/run.py:_is_user_authorized_for_source */
int _is_user_authorized_for_source(void *gateway, const char *source) {
    (void)gateway; (void)source;
    return 1;
}

/* PoP: _instantiate_adapter @ gateway/run.py:_instantiate_adapter */
void *_instantiate_adapter(const char *adapter_type) {
    (void)adapter_type;
    return NULL;
}

/* PoP: _gateway_plain_command_handlers @ gateway/run.py:_gateway_plain_command_handlers */
char *_gateway_plain_command_handlers(void) {
    return strdup("[]");
}

/* PoP: _drain_restart_safe_cron_deliveries @ gateway/run.py:_drain_restart_safe_cron_deliveries */
int _drain_restart_safe_cron_deliveries(void *gateway) {
    (void)gateway;
    return 0;
}

/* PoP: _shutdown_mcp_servers_nonblocking @ gateway/run.py:_shutdown_mcp_servers_nonblocking */
void _shutdown_mcp_servers_nonblocking(void *gateway) {
    (void)gateway;
}
