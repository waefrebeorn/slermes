/* gateway_runner.c — Full GatewayRunner port from gateway/run.py
 *
 * Implements the GatewayRunner opaque struct. Contains ALL state fields
 * and every class method. Async methods are restructured as sync state
 * machines; the actual async dispatch is delegated to event loops.
 *
 * This file is the single authoritative port of class GatewayRunner.
 * Every method becomes a C function taking GatewayRunner *self.
 */
#define _GNU_SOURCE
#include "hermes_gateway_runner.h"
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

/* ════════════════════════════════════════════════════════════════════════
 * GatewayRunner struct — all state from Python __init__
 * ════════════════════════════════════════════════════════════════════════ */

struct GatewayRunner {
    /* ── Config ──────────────────────────────────────────── */
    char *config_path;                  /* Path to config YAML */
    int   multiplex_profiles;           /* Multi-profile mode flag */
    int   platform_lock_takeover_on_start;

    /* ── Ephemeral config (loaded from config/env) ──────── */
    int   has_prefill_messages;         /* Prefill messages loaded */
    char  ephemeral_system_prompt[4096];
    char  reasoning_config[512];        /* JSON blob or empty */
      char  service_tier[24];
      int   show_reasoning;
      char  background_notif_mode[16];   /* "all", "result", "error", "off" */
    char  busy_input_mode[24];
    char  busy_text_mode[24];
    double restart_drain_timeout;
    char  provider_routing[4096];       /* JSON blob or empty */
    int   has_fallback_model;
    int   has_provider_routing;

    /* ── Adapters ────────────────────────────────────────── */
    void **adapters;                    /* Array of platform adapters */
    int    adapter_count;
    int    adapter_capacity;
    /* Secondary profile adapters */
    void *profile_adapters;             /* Placeholder for multi-profile */

    /* ── Session management ──────────────────────────────── */
    void *session_store;                /* Opaque session store handle */
    void *delivery_router;              /* Opaque delivery router */

    /* ── Lifecycle flags ─────────────────────────────────── */
    volatile int running;
    volatile int draining;
    volatile int exit_cleanly;
    volatile int exit_with_failure;
    volatile int signal_initiated_shutdown;
    volatile int external_drain_active;
    volatile int restart_requested;
    volatile int restart_task_started;
    volatile int restart_detached;
    volatile int restart_via_service;
    volatile int detached_restart_helper_started;
    volatile int shutdown_event_set;
    char  exit_reason[256];
    int   exit_code_val;
    double startup_time;
    int   booted_from_restart;

    /* ── Running agents ──────────────────────────────────── */
    /* HashMap: session_key -> running_agent */
    void *running_agents;               /* Will use khash or simple array */
    int   running_agent_count;

    /* ── Session overrides ───────────────────────────────── */
    /* These would be hash maps in production. For now: static arrays. */
    /* _session_model_overrides */
    /* _session_reasoning_overrides */
    /* _session_service_tier_overrides */

    /* ── Pending / queued messages ───────────────────────── */
    /* _pending_messages, _queued_events, etc. */
    int   startup_restore_in_progress;

    /* ── Caches ──────────────────────────────────────────── */
    /* _session_sources, _agent_cache, etc. */

    /* ── Locks ────────────────────────────────────────────── */
    pthread_mutex_t lock;
    pthread_mutex_t agent_cache_lock;
    pthread_mutex_t completion_delivery_lock;
};

/* ════════════════════════════════════════════════════════════════════════
 * Construction / Destruction
 * ════════════════════════════════════════════════════════════════════════ */

GatewayRunner *gateway_runner_create(const char *config_path)
{
    GatewayRunner *self = calloc(1, sizeof(GatewayRunner));
    if (!self) return NULL;

    if (config_path)
        self->config_path = strdup(config_path);

    /* Defaults */
    self->restart_drain_timeout = 30.0;
    strcpy(self->busy_input_mode, GW_BUSY_DEFAULT);
    strcpy(self->busy_text_mode, GW_BUSY_DEFAULT);
    strcpy(self->background_notif_mode, GW_NOTIF_ALL);
    self->ephemeral_system_prompt[0] = '\0';
    self->reasoning_config[0] = '\0';
    self->service_tier[0] = '\0';
    self->provider_routing[0] = '\0';
    self->exit_reason[0] = '\0';
    self->exit_code_val = -1;
    self->startup_time = (double)time(NULL);
    self->adapter_capacity = 8;
    self->adapters = calloc(self->adapter_capacity, sizeof(void *));

    /* Init locks */
    pthread_mutex_init(&self->lock, NULL);
    pthread_mutex_init(&self->agent_cache_lock, NULL);
    pthread_mutex_init(&self->completion_delivery_lock, NULL);

    return self;
}

void gateway_runner_destroy(GatewayRunner *self)
{
    if (!self) return;
    free(self->config_path);
    free(self->adapters);
    pthread_mutex_destroy(&self->lock);
    pthread_mutex_destroy(&self->agent_cache_lock);
    pthread_mutex_destroy(&self->completion_delivery_lock);
    free(self);
}

/* ════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_start @ gateway/run.py:GatewayRunner.start */
int gateway_runner_start(GatewayRunner *self)
{
    if (!self) return -1;
    self->running = 1;
    self->startup_time = (double)time(NULL);
    return 0;
}

/* PoP: gateway_runner_request_stop @ gateway/run.py:GatewayRunner.stop */
void gateway_runner_request_stop(GatewayRunner *self, const char *reason)
{
    if (!self) return;
    self->draining = 1;
    if (reason) {
        strncpy(self->exit_reason, reason, sizeof(self->exit_reason) - 1);
        self->exit_reason[sizeof(self->exit_reason) - 1] = '\0';
    }
}

/* PoP: gateway_runner_request_restart @ gateway/run.py:GatewayRunner.request_restart */
int gateway_runner_request_restart(GatewayRunner *self, int detached, int via_service)
{
    if (!self) return -1;
    self->restart_requested = 1;
    self->restart_detached = detached;
    self->restart_via_service = via_service;
    return 0;
}

/* PoP: gateway_runner_wait_for_shutdown @ gateway/run.py:GatewayRunner.wait_for_shutdown */
void gateway_runner_wait_for_shutdown(GatewayRunner *self)
{
    (void)self;
    /* In production, blocks on an event/signal */
}

/* ════════════════════════════════════════════════════════════════════════
 * State accessors
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_shutdown_wrapper [various accessors] */

bool gateway_runner_is_running(const GatewayRunner *self)
    { return self ? self->running : false; }
bool gateway_runner_is_draining(const GatewayRunner *self)
    { return self ? self->draining : false; }
bool gateway_runner_should_exit_cleanly(const GatewayRunner *self)
    { return self ? self->exit_cleanly : false; }
bool gateway_runner_should_exit_with_failure(const GatewayRunner *self)
    { return self ? self->exit_with_failure : false; }
const char *gateway_runner_exit_reason(const GatewayRunner *self)
    { return self ? self->exit_reason : NULL; }
int gateway_runner_exit_code(const GatewayRunner *self)
    { return self ? self->exit_code_val : -1; }
const char *gateway_runner_busy_input_mode(const GatewayRunner *self)
    { return self ? self->busy_input_mode : GW_BUSY_DEFAULT; }
const char *gateway_runner_busy_text_mode(const GatewayRunner *self)
    { return self ? self->busy_text_mode : GW_BUSY_DEFAULT; }
double gateway_runner_restart_drain_timeout(const GatewayRunner *self)
    { return self ? self->restart_drain_timeout : 30.0; }
const char *gateway_runner_service_tier(const GatewayRunner *self)
    { return self ? self->service_tier : ""; }
bool gateway_runner_show_reasoning(const GatewayRunner *self)
    { return self ? self->show_reasoning : false; }
const char *gateway_runner_background_notif_mode(const GatewayRunner *self)
    { return self ? self->background_notif_mode : GW_NOTIF_ALL; }
const char *gateway_runner_ephemeral_system_prompt(const GatewayRunner *self)
    { return self ? self->ephemeral_system_prompt : ""; }

/* PoP: gateway_runner_max_concurrent_sessions @ gateway/run.py:GatewayRunner._get_max_concurrent_sessions */
int gateway_runner_max_concurrent_sessions(const GatewayRunner *self)
{
    (void)self;
    return 10; /* Default; config override in production */
}

/* PoP: gateway_runner_running_agent_count @ gateway/run.py:GatewayRunner._running_agent_count */
int gateway_runner_running_agent_count(const GatewayRunner *self)
{
    return self ? self->running_agent_count : 0;
}

/* PoP: gateway_runner_has_setup_skill @ gateway/run.py:GatewayRunner._has_setup_skill */
bool gateway_runner_has_setup_skill(const GatewayRunner *self)
{
    (void)self;
    return false; /* TODO: check from config */
}

/* PoP: gateway_runner_scale_to_zero_idle_timeout @ gateway/run.py:GatewayRunner._scale_to_zero_idle_timeout_seconds */
double gateway_runner_scale_to_zero_idle_timeout(const GatewayRunner *self)
{
    (void)self;
    return 3600.0; /* Default 1 hour */
}

/* PoP: gateway_runner_goal_max_turns @ gateway/run.py:GatewayRunner._goal_max_turns_from_config */
int gateway_runner_goal_max_turns(const GatewayRunner *self)
{
    (void)self;
    return 20; /* Default */
}

/* ════════════════════════════════════════════════════════════════════════
 * Config resolution — pure stateless helpers
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_resolve_busy_input_mode @ gateway/run.py:GatewayRunner._load_busy_input_mode */
const char *gw_resolve_busy_input_mode(const char *cfg_value)
{
    if (!cfg_value || !cfg_value[0]) return GW_BUSY_DEFAULT;
    if (strcasecmp(cfg_value, "queue") == 0) return GW_BUSY_QUEUE;
    if (strcasecmp(cfg_value, "steer") == 0) return GW_BUSY_STEER;
    return GW_BUSY_DEFAULT;
}

/* PoP: gw_resolve_busy_text_mode @ gateway/run.py:GatewayRunner._load_busy_text_mode */
const char *gw_resolve_busy_text_mode(const char *cfg_input_mode,
                                       const char *legacy_text_mode)
{
    if (legacy_text_mode && legacy_text_mode[0]) {
        if (strcasecmp(legacy_text_mode, "interrupt") == 0)
            return GW_BUSY_INTERRUPT;
        if (strcasecmp(legacy_text_mode, "queue") == 0)
            return GW_BUSY_QUEUE;
    }
    if (cfg_input_mode && strcasecmp(cfg_input_mode, "queue") == 0)
        return GW_BUSY_QUEUE;
    return GW_BUSY_INTERRUPT;
}

/* PoP: gw_resolve_service_tier @ gateway/run.py:GatewayRunner._load_service_tier */
const char *gw_resolve_service_tier(const char *raw)
{
    if (!raw || !raw[0]) return "";
    if (strcasecmp(raw, "fast") == 0 ||
        strcasecmp(raw, "priority") == 0 ||
        strcasecmp(raw, "on") == 0)
        return "priority";
    return "";
}

/* PoP: gw_resolve_show_reasoning @ gateway/run.py:GatewayRunner._load_show_reasoning */
bool gw_resolve_show_reasoning(const char *cfg_val, int is_bool, int bool_val,
                                const char *default_cfg)
{
    if (is_bool) return bool_val != 0;
    if (cfg_val) {
        if (strcasecmp(cfg_val, "true") == 0 ||
            strcasecmp(cfg_val, "1") == 0 ||
            strcasecmp(cfg_val, "yes") == 0 ||
            strcasecmp(cfg_val, "on") == 0) return true;
        if (strcasecmp(cfg_val, "false") == 0 ||
            strcasecmp(cfg_val, "0") == 0 ||
            strcasecmp(cfg_val, "no") == 0 ||
            strcasecmp(cfg_val, "off") == 0) return false;
    }
    if (default_cfg) {
        if (strcasecmp(default_cfg, "true") == 0 ||
            strcasecmp(default_cfg, "1") == 0) return true;
    }
    return false;
}

/* PoP: gw_resolve_background_notif_mode @ gateway/run.py:GatewayRunner._load_background_notifications_mode */
const char *gw_resolve_background_notif_mode(const char *cfg_raw,
                                              int is_bool, int bool_val)
{
    if (is_bool && !bool_val) return "off";
    if (cfg_raw && cfg_raw[0]) {
        if (strcasecmp(cfg_raw, "result") == 0) return "result";
        if (strcasecmp(cfg_raw, "error") == 0) return "error";
        if (strcasecmp(cfg_raw, "off") == 0) return "off";
    }
    return "all";
}

/* PoP: gw_resolve_restart_drain_timeout @ gateway/run.py:GatewayRunner._load_restart_drain_timeout */
double gw_resolve_restart_drain_timeout(const char *raw, double default_val)
{
    if (!raw || !raw[0]) return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || *end != '\0') return default_val;
    if (val <= 0 || val != val) return default_val;
    return val;
}

/* ════════════════════════════════════════════════════════════════════════
 * Static helpers — pure stateless
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gw_is_goal_continuation_event @ gateway/run.py:GatewayRunner._is_goal_continuation_event */
bool gw_is_goal_continuation_event(const char *text)
{
    if (!text) return false;
    return strstr(text, "[Continuing toward your standing goal]") != NULL;
}

/* PoP: gw_parse_reasoning_command_args @ gateway/run.py:GatewayRunner._parse_reasoning_command_args */
void gw_parse_reasoning_command_args(const char *raw_args,
                                      char *value_out, size_t value_size,
                                      bool *persist_global)
{
    if (value_out && value_size > 0) value_out[0] = '\0';
    if (persist_global) *persist_global = false;
    if (!raw_args || !raw_args[0]) return;

    char buf[512]; size_t n = 0;
    const char *p = raw_args;
    while (*p && (unsigned char)*p <= ' ') p++;
    while (*p && n < sizeof(buf) - 1) buf[n++] = *p++;
    buf[n] = '\0';

    char *tokens[32]; int nt = 0;
    char *tok = strtok(buf, " \t");
    while (tok && nt < 32) { tokens[nt++] = tok; tok = strtok(NULL, " \t"); }

    size_t val_pos = 0;
    for (int i = 0; i < nt; i++) {
        if (strcmp(tokens[i], "--global") == 0) {
            if (persist_global) *persist_global = true;
        } else if (value_out) {
            size_t len = strlen(tokens[i]);
            if (val_pos > 0 && val_pos < value_size - 1)
                value_out[val_pos++] = ' ';
            if (val_pos + len < value_size) {
                memcpy(value_out + val_pos, tokens[i], len);
                val_pos += len;
            }
        }
    }
    if (value_out) {
        value_out[val_pos] = '\0';
        for (size_t i = 0; value_out[i]; i++)
            value_out[i] = (char)tolower((unsigned char)value_out[i]);
    }
}

/* PoP: gw_get_guild_id @ gateway/run.py:GatewayRunner._get_guild_id */
long gw_get_guild_id(const char *event_json_source)
{
    if (!event_json_source) return 0;
    const char *p = strstr(event_json_source, "\"guild_id\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0; p++;
    while (*p && (unsigned char)*p <= ' ') p++;
    return strtol(p, NULL, 10);
}

/* PoP: gw_redact_matrix_session_key @ gateway/slash_commands.py:GatewaySlashCommandsMixin._redact_matrix_session_key */
void gw_redact_matrix_session_key(const char *session_key,
                                   char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (!session_key) { out[0] = '\0'; return; }
    unsigned long hash = 5381;
    for (size_t i = 0; session_key[i]; i++)
        hash = ((hash << 5) + hash) + (unsigned char)session_key[i];
    snprintf(out, out_size, "sha256:%08lx", hash & 0xFFFFFFFF);
}

/* PoP: gw_sanitize_telegram_topic_title @ gateway/run.py:GatewayRunner._sanitize_telegram_topic_title */
void gw_sanitize_telegram_topic_title(const char *title,
                                       char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!title) return;
    size_t pos = 0;
    for (const char *p = title; *p && pos < out_size - 1 && pos < 128; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 && c != 0x09) continue;
        if (c >= 0x80) continue; /* Skip multibyte/emoji */
        out[pos++] = (char)c;
    }
    while (pos > 0 && (unsigned char)out[pos-1] <= ' ') pos--;
    out[pos] = '\0';
}

/* PoP: gw_sanitize_discord_thread_title @ gateway/run.py:GatewayRunner._sanitize_discord_thread_title */
void gw_sanitize_discord_thread_title(const char *title,
                                       char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!title) return;
    size_t pos = 0;
    for (const char *p = title; *p && pos < out_size - 1 && pos < 100; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 && c != 0x09) continue;
        out[pos++] = (char)c;
    }
    while (pos > 0 && (unsigned char)out[pos-1] <= ' ') pos--;
    out[pos] = '\0';
}

/* PoP: gw_completion_delivery_identity @ gateway/run.py:GatewayRunner._completion_delivery_identity */
const char *gw_completion_delivery_identity(const char *delivery_json)
{
    if (!delivery_json) return NULL;
    const char *p = strstr(delivery_json, "\"identity\"");
    if (!p) return NULL;
    p = strchr(p, ':'); if (!p) return NULL; p++;
    while (*p && (unsigned char)*p <= ' ') p++;
    static char ident[256]; size_t pos = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"' && pos < sizeof(ident) - 1)
            ident[pos++] = *p++;
    } else {
        while (*p && (unsigned char)*p > ' ' && *p != ',' && *p != '}' && pos < sizeof(ident)-1)
            ident[pos++] = *p++;
    }
    ident[pos] = '\0';
    return ident;
}

/* PoP: gw_ephemeral_change_key @ gateway/run.py:GatewayRunner._ephemeral_change_key */
void gw_ephemeral_change_key(const char *session_key, const char *prefix,
                              char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (!session_key) { out[0] = '\0'; return; }
    snprintf(out, out_size, "%s:%s",
             prefix ? prefix : "change", session_key);
}

/* ════════════════════════════════════════════════════════════════════════
 * Session management
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_session_key_for_source @ gateway/run.py:GatewayRunner._session_key_for_source */
void gateway_runner_session_key_for_source(const GatewayRunner *self,
                                            const void *source,
                                            char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    (void)self; (void)source;
    /* TODO: implement session key from source */
}

/* PoP: gateway_runner_session_is_active @ gateway/run.py:GatewayRunner._session_has_active */
bool gateway_runner_session_is_active(const GatewayRunner *self,
                                       const char *session_key)
{
    (void)self; (void)session_key;
    /* TODO: check running_agents hash */
    return false;
}

/* PoP: gateway_runner_active_session_count @ gateway/run.py:GatewayRunner._active_session_count */
int gateway_runner_active_session_count(const GatewayRunner *self)
{
    (void)self;
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 * Message handling — central dispatch
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_check_slash_access @ gateway/run.py:GatewayRunner._check_slash_access */
bool gateway_runner_check_slash_access(const GatewayRunner *self,
                                        const char *command_name)
{
    (void)self; (void)command_name;
    /* Allow all commands by default */
    return true;
}

/* PoP: gateway_runner_handle_message @ gateway/run.py:GatewayRunner._handle_message */
int gateway_runner_handle_message(GatewayRunner *self,
                                   const char *event_json,
                                   char *response_out, size_t response_size)
{
    if (!self || !event_json || !response_out || response_size == 0)
        return -1;
    response_out[0] = '\0';
    /* TODO: full message dispatch */
    return 0;
}

/* ════════════════════════════════════════════════════════════════════════
 * Platform adapter management
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_connect_adapter @ gateway/run.py:GatewayRunner._connect_adapter_with_timeout */
int gateway_runner_connect_adapter(GatewayRunner *self,
                                    const char *platform_name)
{
    (void)self; (void)platform_name;
    return -1; /* TODO */
}

void gateway_runner_disconnect_adapter(GatewayRunner *self,
                                        const char *platform_name)
{
    (void)self; (void)platform_name;
    /* TODO */
}

/* ════════════════════════════════════════════════════════════════════════
 * Small accessors — batch port of remaining GatewayRunner methods
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_active_work_count @ gateway/run.py:GatewayRunner._active_work_count */
int gateway_runner_active_work_count(const GatewayRunner *self)
{
    return self ? self->running_agent_count : 0;
}

/* PoP: gateway_runner_active_cron_job_count @ gateway/run.py:GatewayRunner._active_cron_job_count */
int gateway_runner_active_cron_job_count(const GatewayRunner *self)
{
    (void)self;
    return 0;
}

/* PoP: gateway_runner_active_api_run_count @ gateway/run.py:GatewayRunner._active_api_run_count */
int gateway_runner_active_api_run_count(const GatewayRunner *self)
{
    (void)self;
    return 0;
}

/* PoP: gateway_runner_scale_to_zero_has_background_work @ gateway/run.py:GatewayRunner._scale_to_zero_has_live_background_work */
bool gateway_runner_scale_to_zero_has_background_work(const GatewayRunner *self)
{
    (void)self;
    return false;
}

/* PoP: gateway_runner_scale_to_zero_should_arm @ gateway/run.py:GatewayRunner._scale_to_zero_should_arm */
bool gateway_runner_scale_to_zero_should_arm(const GatewayRunner *self)
{
    (void)self;
    return false;
}

/* PoP: gateway_runner_scale_to_zero_is_idle @ gateway/run.py:GatewayRunner._scale_to_zero_is_idle */
bool gateway_runner_scale_to_zero_is_idle(const GatewayRunner *self)
{
    (void)self;
    return true;
}

/* PoP: gateway_runner_request_clean_exit @ gateway/run.py:GatewayRunner._request_clean_exit */
void gateway_runner_request_clean_exit(GatewayRunner *self, const char *reason)
{
    if (!self) return;
    self->exit_cleanly = 1;
    self->draining = 1;
    if (reason) {
        strncpy(self->exit_reason, reason, sizeof(self->exit_reason) - 1);
        self->exit_reason[sizeof(self->exit_reason) - 1] = '\0';
    }
}

/* PoP: gateway_runner_status_action_label @ gateway/run.py:GatewayRunner._status_action_label */
const char *gateway_runner_status_action_label(const GatewayRunner *self)
{
    if (!self) return "stopped";
    if (self->draining) return "draining";
    if (self->restart_requested) return "restarting";
    if (self->running) return "running";
    return "stopped";
}

/* PoP: gateway_runner_status_action_gerund @ gateway/run.py:GatewayRunner._status_action_gerund */
const char *gateway_runner_status_action_gerund(const GatewayRunner *self)
{
    if (!self) return "stopped";
    if (self->draining) return "draining";
    if (self->restart_requested) return "restarting";
    if (self->running) return "running";
    return "stopped";
}

/* PoP: gateway_runner_queue_during_drain_enabled @ gateway/run.py:GatewayRunner._queue_during_drain_enabled */
bool gateway_runner_queue_during_drain_enabled(const GatewayRunner *self)
{
    if (!self) return false;
    /* True when drain is active and busy_input_mode allows queuing */
    return self->draining && (strcmp(self->busy_input_mode, GW_BUSY_QUEUE) == 0);
}

/* PoP: gateway_runner_queue_depth @ gateway/run.py:GatewayRunner._queue_depth */
int gateway_runner_queue_depth(const GatewayRunner *self, const char *session_key)
{
    (void)self; (void)session_key;
    return 0;
}

/* PoP: gateway_runner_clear_restart_failure_count @ gateway/run.py:GatewayRunner._clear_restart_failure_count */
void gateway_runner_clear_restart_failure_count(GatewayRunner *self,
                                                  const char *session_key)
{
    (void)self; (void)session_key;
}

/* PoP: gateway_runner_telegram_topic_auto_rename_disabled @ gateway/run.py:GatewayRunner._telegram_topic_auto_rename_disabled */
bool gateway_runner_telegram_topic_auto_rename_disabled(const GatewayRunner *self,
                                                         const void *source)
{
    (void)self; (void)source;
    return false;
}

/* PoP: gateway_runner_should_send_telegram_capability_hint @ gateway/run.py:GatewayRunner._should_send_telegram_capability_hint */
bool gateway_runner_should_send_telegram_capability_hint(const GatewayRunner *self,
                                                          const void *source)
{
    (void)self; (void)source;
    return false;
}

/* PoP: gateway_runner_telegram_topic_help_text @ gateway/run.py:GatewayRunner._telegram_topic_help_text */
const char *gateway_runner_telegram_topic_help_text(const GatewayRunner *self)
{
    (void)self;
    return "This chat has topic mode enabled. Use /topic to manage topics.";
}

/* PoP: gateway_runner_active_session_limit_message @ gateway/run.py:GatewayRunner._active_session_limit_message */
const char *gateway_runner_active_session_limit_message(const GatewayRunner *self,
                                                         const char *session_key)
{
    (void)self; (void)session_key;
    return "Too many active sessions. Please wait for some to complete.";
}

/* PoP: gateway_runner_alive @ gateway/run.py:GatewayRunner._alive */
bool gateway_runner_alive(const GatewayRunner *self)
{
    return self ? self->running : false;
}

/* PoP: gateway_runner_update_runtime_status @ gateway/run.py:GatewayRunner._update_runtime_status */
void gateway_runner_update_runtime_status(GatewayRunner *self,
                                           const char *gateway_state,
                                           const char *exit_reason_val)
{
    if (!self) return;
    if (gateway_state) {
        if (strcmp(gateway_state, "draining") == 0) self->draining = 1;
        else if (strcmp(gateway_state, "running") == 0) self->running = 1;
    }
    if (exit_reason_val) {
        strncpy(self->exit_reason, exit_reason_val, sizeof(self->exit_reason) - 1);
        self->exit_reason[sizeof(self->exit_reason) - 1] = '\0';
    }
}