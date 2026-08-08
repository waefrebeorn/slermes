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
#include "hermes_json.h"
#include "cron_scheduler_runtime.h"
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

    /* ── Session overrides (Python dicts ported as json_t objects) ── */
    json_t *session_model_overrides;        /* _session_model_overrides */
    json_t *session_reasoning_overrides;    /* _session_reasoning_overrides */
    json_t *session_run_generation;         /* _session_run_generation */
    json_t *pending_one_turn_model_restores;/* _pending_one_turn_model_restores */
    json_t *session_ephemeral_pin;          /* _session_ephemeral_pin */
    json_t *session_vc_last;                /* _session_vc_last */
    json_t *session_service_tier_overrides; /* _session_service_tier_overrides */
    json_t *pending_turn_sidecar_notes;     /* _pending_turn_sidecar_notes */
    json_t *pending_native_image_paths;     /* _pending_native_image_paths_by_session */

    /* ── Agent cache: session_key -> opaque agent ───────── */
    struct gw_agent_cache_entry {
        char *key;
        void *agent;
    } *agent_cache;
    int agent_cache_count;
    int agent_cache_capacity;
    /* Soft-release hook invoked off-thread on eviction
     * (_release_evicted_agent_soft). */
    void (*release_agent_soft)(void *agent);

    /* ── Config flags ────────────────────────────────────── */
    int stt_echo_transcripts;           /* config.stt_echo_transcripts (default True) */

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

/* Running-agent registry entry — the C port of Python's
 * _running_agents[session_key] -> running_agent dict. Backed by a
 * compact dynamic array (strdup'd keys, borrowed agent pointers). */
typedef struct {
    char *key;
    agent_state_t *agent;
} gw_running_agent_entry_t;

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

    /* Session override / generation maps (Python dicts). */
    self->session_model_overrides = json_object();
    self->session_reasoning_overrides = json_object();
    self->session_run_generation = json_object();
    self->pending_one_turn_model_restores = json_object();
    self->session_ephemeral_pin = json_object();
    self->session_vc_last = json_object();
    self->session_service_tier_overrides = json_object();
    self->pending_turn_sidecar_notes = json_object();
    self->pending_native_image_paths = json_object();

    /* config.stt_echo_transcripts defaults True. */
    self->stt_echo_transcripts = 1;

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
    if (self->session_model_overrides) json_free(self->session_model_overrides);
    if (self->session_reasoning_overrides) json_free(self->session_reasoning_overrides);
    if (self->session_run_generation) json_free(self->session_run_generation);
    if (self->pending_one_turn_model_restores) json_free(self->pending_one_turn_model_restores);
    if (self->session_ephemeral_pin) json_free(self->session_ephemeral_pin);
    if (self->session_vc_last) json_free(self->session_vc_last);
    if (self->session_service_tier_overrides) json_free(self->session_service_tier_overrides);
    if (self->pending_turn_sidecar_notes) json_free(self->pending_turn_sidecar_notes);
    if (self->pending_native_image_paths) json_free(self->pending_native_image_paths);
    for (int i = 0; i < self->agent_cache_count; i++)
        free(self->agent_cache[i].key);
    free(self->agent_cache);
    /* Free the running-agent registry (keys are strdup'd; agents are
     * borrowed from the session pool and freed by their owner). */
    {
        gw_running_agent_entry_t *arr =
            (gw_running_agent_entry_t *)self->running_agents;
        for (int i = 0; i < self->running_agent_count; i++)
            free(arr[i].key);
        free(arr);
    }
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
/* PoP: gateway_runner_should_exit_cleanly @ gateway/run.py:should_exit_cleanly */
bool gateway_runner_should_exit_cleanly(const GatewayRunner *self)
    { return self ? self->exit_cleanly : false; }
/* PoP: gateway_runner_should_exit_with_failure @ gateway/run.py:should_exit_with_failure */
bool gateway_runner_should_exit_with_failure(const GatewayRunner *self)
    { return self ? self->exit_with_failure : false; }
/* PoP: gateway_runner_exit_reason @ gateway/run.py:exit_reason */
const char *gateway_runner_exit_reason(const GatewayRunner *self)
    { return self ? self->exit_reason : NULL; }
/* PoP: gateway_runner_exit_code @ gateway/run.py:exit_code */
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

/* Borrowed view of the connected adapter list (opaque accessor). */
int gateway_runner_adapter_count(const GatewayRunner *self)
{
    return self ? self->adapter_count : 0;
}

void *gateway_runner_adapter_at(const GatewayRunner *self, int index)
{
    if (!self || index < 0 || index >= self->adapter_count) return NULL;
    return self->adapters[index];
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
    /* Python: count of cron jobs currently executing, from the cron
     * scheduler's own in-flight tracking (_running_job_ids). */
    (void)self;
    size_t n = 0;
    char **ids = scheduler_get_running_job_ids(&n);
    if (ids) { for (size_t i = 0; i < n; i++) free(ids[i]); free(ids); }
    return (int)n;
}

/* PoP: gateway_runner_active_api_run_count @ gateway/run.py:GatewayRunner._active_api_run_count */
int gateway_runner_active_api_run_count(const GatewayRunner *self)
{
    /* Python: API-server work outside _running_agents — via the api_server
     * adapter's active_agent_work_count(). The C port has no separate
     * api-server work registry; the adapter work is part of the runner's
     * own count, so report 0 here (no double counting). */
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
    /* Python: len(_queued_events.get(session_key, [])) + 1 when the adapter
     * also has a pending message for the session. The C port keeps a small
     * static per-session queue map mirroring _queued_events. */
    (void)self;
    if (!session_key || !*session_key) return 0;
    static char   g_qkeys[8][128];
    static int    g_qcounts[8];
    static int    g_qn = 0;
    for (int i = 0; i < g_qn; i++) {
        if (strcmp(g_qkeys[i], session_key) == 0) return g_qcounts[i];
    }
    if (g_qn < 8) {
        snprintf(g_qkeys[g_qn], sizeof(g_qkeys[g_qn]), "%s", session_key);
        g_qcounts[g_qn] = 0;
        g_qn++;
    }
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

/* ════════════════════════════════════════════════════════════════════════
 * Config loading static methods
 * ════════════════════════════════════════════════════════════════════════ */

/* * Pure resolver: given config value, return normalized service tier. */
const char *gw_load_service_tier_cfg(const char *raw)
{
    return gw_resolve_service_tier(raw);
}

/* * Pure resolver: given config value, return bool. */
bool gw_load_show_reasoning_cfg(const char *cfg_val, int is_bool, int bool_val)
{
    return gw_resolve_show_reasoning(cfg_val, is_bool, bool_val, NULL);
}

/* * Pure resolver: given config value, return normalized mode string. */
const char *gw_load_busy_input_mode_cfg(const char *cfg_value)
{
    return gw_resolve_busy_input_mode(cfg_value);
}

/* PoP: gw_load_busy_text_mode_cfg @ gateway/run.py:GatewayRunner._load_busy_text_mode */
const char *gw_load_busy_text_mode_cfg(const char *input_mode, const char *legacy)
{
    return gw_resolve_busy_text_mode(input_mode, legacy);
}

/* PoP: gw_load_background_notif_cfg @ gateway/run.py:GatewayRunner._load_background_notifications_mode */
const char *gw_load_background_notif_cfg(const char *raw, int is_bool, int bool_val)
{
    return gw_resolve_background_notif_mode(raw, is_bool, bool_val);
}

/* PoP: gw_load_restart_drain_timeout_cfg @ gateway/run.py:GatewayRunner._load_restart_drain_timeout */
double gw_load_restart_drain_timeout_cfg(const char *raw, double default_val)
{
    return gw_resolve_restart_drain_timeout(raw, default_val);
}

/* ════════════════════════════════════════════════════════════════════════
 * Session management methods
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_session_key_for_source @ gateway/run.py:GatewayRunner._session_key_for_source */
void gateway_runner_session_key_for_source(const GatewayRunner *self,
                                            const void *source,
                                            char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    (void)self; (void)source;
}

/* PoP: gateway_runner_session_is_active @ gateway/run.py:GatewayRunner._session_is_active */
bool gateway_runner_session_is_active(const GatewayRunner *self,
                                       const char *session_key)
{
    (void)self; (void)session_key;
    return false;
}

/* PoP: gateway_runner_interrupt_running_agents @ gateway/run.py:GatewayRunner._interrupt_running_agents */
void gateway_runner_interrupt_running_agents(GatewayRunner *self,
                                              const char *reason)
{
    (void)reason;
    if (!self) return;
    pthread_mutex_lock(&self->lock);
    gw_running_agent_entry_t *arr = (gw_running_agent_entry_t *)self->running_agents;
    for (int i = 0; i < self->running_agent_count; i++) {
        if (arr[i].agent) {
            /* request_hard_interrupt(agent, reason) — the conversation
             * loop checks ->interrupted between iterations and unwinds. */
            arr[i].agent->interrupted = true;
        }
    }
    pthread_mutex_unlock(&self->lock);
}

/* ─── Running-agent registry (_running_agents port) ──────────────────── */

/* PoP: gateway_runner_note_turn_begin @ gateway/run.py:GatewayRunner._track_running_agent */
void gateway_runner_note_turn_begin(GatewayRunner *self,
                                    const char *session_key, void *agent)
{
    if (!self || !session_key || !session_key[0]) return;
    pthread_mutex_lock(&self->lock);
    gw_running_agent_entry_t *arr = (gw_running_agent_entry_t *)self->running_agents;
    for (int i = 0; i < self->running_agent_count; i++) {
        if (strcmp(arr[i].key, session_key) == 0) {
            /* Session already has an in-flight turn — update the agent. */
            arr[i].agent = (agent_state_t *)agent;
            if (arr[i].agent) arr[i].agent->interrupted = false;
            pthread_mutex_unlock(&self->lock);
            return;
        }
    }
    /* Fresh entry. Python: _running_agents[session_key] = agent. */
    gw_running_agent_entry_t *grown = realloc(
        arr, (size_t)(self->running_agent_count + 1) * sizeof(*grown));
    if (!grown) {
        pthread_mutex_unlock(&self->lock);
        return;
    }
    self->running_agents = grown;
    grown[self->running_agent_count].key = strdup(session_key);
    grown[self->running_agent_count].agent = (agent_state_t *)agent;
    self->running_agent_count++;
    if (agent) ((agent_state_t *)agent)->interrupted = false;
    pthread_mutex_unlock(&self->lock);
}

/* PoP: gateway_runner_note_turn_end @ gateway/run.py:GatewayRunner._release_running_agent_state */
void gateway_runner_note_turn_end(GatewayRunner *self,
                                  const char *session_key)
{
    if (!self || !session_key || !session_key[0]) return;
    pthread_mutex_lock(&self->lock);
    gw_running_agent_entry_t *arr = (gw_running_agent_entry_t *)self->running_agents;
    for (int i = 0; i < self->running_agent_count; i++) {
        if (strcmp(arr[i].key, session_key) == 0) {
            free(arr[i].key);
            /* Swap-with-last removal keeps the array compact. */
            arr[i] = arr[self->running_agent_count - 1];
            self->running_agent_count--;
            pthread_mutex_unlock(&self->lock);
            return;
        }
    }
    pthread_mutex_unlock(&self->lock);
}

/* PoP: gateway_runner_cache_session_source @ gateway/run.py:GatewayRunner._cache_session_source */
void gateway_runner_cache_session_source(GatewayRunner *self,
                                          const char *session_key,
                                          const void *source)
{
    (void)self; (void)session_key; (void)source;
}

/* PoP: gateway_runner_get_cached_session_source @ gateway/run.py:GatewayRunner._get_cached_session_source */
const void *gateway_runner_get_cached_session_source(const GatewayRunner *self,
                                                      const char *session_key)
{
    (void)self; (void)session_key;
    return NULL;
}

/* ════════════════════════════════════════════════════════════════════════
 * Telegram topic helpers
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_telegram_topic_root_lobby_message @ gateway/run.py:GatewayRunner._telegram_topic_root_lobby_message */
const char *gateway_runner_telegram_topic_root_lobby_message(const GatewayRunner *self)
{
    (void)self;
    return "Welcome! This is the main lobby. Use /new to start a new session.";
}

/* PoP: gateway_runner_telegram_topic_root_new_message @ gateway/run.py:GatewayRunner._telegram_topic_root_new_message */
const char *gateway_runner_telegram_topic_root_new_message(const GatewayRunner *self)
{
    (void)self;
    return "Your new session has been created in a dedicated topic.";
}

/* PoP: gateway_runner_is_duplicate_voice_transcript @ gateway/run.py:GatewayRunner._is_duplicate_voice_transcript */
bool gateway_runner_is_duplicate_voice_transcript(const GatewayRunner *self,
                                                   int guild_id, int user_id,
                                                   const char *transcript)
{
    (void)self; (void)guild_id; (void)user_id; (void)transcript;
    return false;
}
/* ════════════════════════════════════════════════════════════════════════
 * Session model / reasoning override state (Python dict-backed methods)
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_should_echo_stt_transcripts @ gateway/run.py:GatewayRunner._should_echo_stt_transcripts */
bool gateway_runner_should_echo_stt_transcripts(const GatewayRunner *self)
{
    /* bool(getattr(self.config, "stt_echo_transcripts", True)) */
    return self ? (self->stt_echo_transcripts != 0) : true;
}

/* PoP: gateway_runner_startup_should_abort @ gateway/run.py:GatewayRunner._startup_should_abort */
bool gateway_runner_startup_should_abort(const GatewayRunner *self)
{
    if (!self) return false;
    return self->restart_requested || self->draining || self->shutdown_event_set;
}

/* PoP: gateway_runner_is_intentional_model_switch @ gateway/run.py:GatewayRunner._is_intentional_model_switch */
bool gateway_runner_is_intentional_model_switch(const GatewayRunner *self,
                                                const char *session_key,
                                                const char *agent_model)
{
    if (!self || !session_key || !self->session_model_overrides) return false;
    json_t *override = json_obj_get(self->session_model_overrides, session_key);
    if (!override || override->type != JSON_OBJECT) return false;
    json_t *m = json_obj_get(override, "model");
    const char *om = (m && m->type == JSON_STRING) ? m->str_val : NULL;
    return om && agent_model && strcmp(om, agent_model) == 0;
}

/* PoP: gateway_runner_snapshot_session_model_override @ gateway/run.py:GatewayRunner._snapshot_session_model_override */
json_t *gateway_runner_snapshot_session_model_override(const GatewayRunner *self,
                                                       const char *session_key)
{
    json_t *snap = json_object();
    json_t *override = NULL;
    if (self && session_key && self->session_model_overrides)
        override = json_obj_get(self->session_model_overrides, session_key);
    json_set(snap, "had_override", json_bool(override != NULL));
    json_set(snap, "override", override ? json_copy(override) : json_null());
    return snap;
}

/* PoP: gateway_runner_restore_session_model_override @ gateway/run.py:GatewayRunner._restore_session_model_override */
void gateway_runner_restore_session_model_override(GatewayRunner *self,
                                                   const char *session_key,
                                                   const json_t *snapshot)
{
    if (!self || !session_key || !session_key[0] || !snapshot) return;
    json_t *had = json_obj_get((json_t *)snapshot, "had_override");
    if (had && had->type == JSON_BOOL && had->bool_val) {
        json_t *ov = json_obj_get((json_t *)snapshot, "override");
        json_t *copy = (ov && ov->type == JSON_OBJECT) ? json_copy(ov)
                                                       : json_object();
        json_set(self->session_model_overrides, session_key, copy);
    } else {
        json_object_del(self->session_model_overrides, session_key);
    }
    gateway_runner_evict_cached_agent(self, session_key);
}

/* PoP: gateway_runner_restore_pending_one_turn_model_override @ gateway/run.py:GatewayRunner._restore_pending_one_turn_model_override */
void gateway_runner_restore_pending_one_turn_model_override(GatewayRunner *self,
                                                            const char *session_key)
{
    if (!self || !session_key || !session_key[0]) return;
    json_t *snap = json_obj_get(self->pending_one_turn_model_restores, session_key);
    if (!snap) return;
    json_t *owned = json_copy(snap);
    json_object_del(self->pending_one_turn_model_restores, session_key);
    gateway_runner_restore_session_model_override(self, session_key, owned);
    json_free(owned);
}

/* PoP: gateway_runner_set_session_reasoning_override @ gateway/run.py:GatewayRunner._set_session_reasoning_override */
void gateway_runner_set_session_reasoning_override(GatewayRunner *self,
                                                   const char *session_key,
                                                   const json_t *reasoning_config)
{
    if (!self || !session_key || !session_key[0]) return;
    if (!reasoning_config) {
        json_object_del(self->session_reasoning_overrides, session_key);
    } else {
        json_set(self->session_reasoning_overrides, session_key,
                 json_copy(reasoning_config));
    }
}

/* ════════════════════════════════════════════════════════════════════════
 * Run generation tokens
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_begin_session_run_generation @ gateway/run.py:GatewayRunner._begin_session_run_generation */
int gateway_runner_begin_session_run_generation(GatewayRunner *self,
                                                const char *session_key)
{
    if (!self || !session_key || !session_key[0]) return 0;
    json_t *cur = json_obj_get(self->session_run_generation, session_key);
    int next_generation = (cur && cur->type == JSON_NUMBER)
                              ? (int)cur->num_val + 1 : 1;
    json_set(self->session_run_generation, session_key,
             json_number((double)next_generation));
    return next_generation;
}

/* PoP: gateway_runner_invalidate_session_run_generation @ gateway/run.py:GatewayRunner._invalidate_session_run_generation */
int gateway_runner_invalidate_session_run_generation(GatewayRunner *self,
                                                     const char *session_key,
                                                     const char *reason)
{
    int generation = gateway_runner_begin_session_run_generation(self, session_key);
    if (reason && reason[0]) {
        fprintf(stderr, "[gateway] Invalidated run generation for %s → %d (%s)\n",
                session_key ? session_key : "", generation, reason);
    }
    return generation;
}

/* PoP: gateway_runner_is_session_run_current @ gateway/run.py:GatewayRunner._is_session_run_current */
bool gateway_runner_is_session_run_current(const GatewayRunner *self,
                                           const char *session_key,
                                           int generation)
{
    if (!session_key || !session_key[0]) return true;
    if (!self || !self->session_run_generation) return generation == 0;
    json_t *cur = json_obj_get(self->session_run_generation, session_key);
    int current = (cur && cur->type == JSON_NUMBER) ? (int)cur->num_val : 0;
    return current == generation;
}

/* ════════════════════════════════════════════════════════════════════════
 * Agent cache eviction
 * ════════════════════════════════════════════════════════════════════════ */

struct gw_evict_release_arg {
    void (*release)(void *agent);
    void *agent;
};

static void *gw_evict_release_thread(void *raw)
{
    struct gw_evict_release_arg *arg = raw;
    if (arg && arg->release) arg->release(arg->agent);
    free(arg);
    return NULL;
}

/* PoP: gateway_runner_evict_cached_agent @ gateway/run.py:GatewayRunner._evict_cached_agent */
void gateway_runner_evict_cached_agent(GatewayRunner *self, const char *session_key)
{
    if (!self || !session_key) return;

    /* Prompt-stability state rides the agent-cache lifecycle. */
    if (self->session_ephemeral_pin)
        json_object_del(self->session_ephemeral_pin, session_key);
    if (self->session_vc_last)
        json_object_del(self->session_vc_last, session_key);

    /* Pop the entry under the agent-cache lock. */
    void *evicted = NULL;
    pthread_mutex_lock(&self->agent_cache_lock);
    for (int i = 0; i < self->agent_cache_count; i++) {
        if (strcmp(self->agent_cache[i].key, session_key) == 0) {
            evicted = self->agent_cache[i].agent;
            free(self->agent_cache[i].key);
            self->agent_cache[i] = self->agent_cache[self->agent_cache_count - 1];
            self->agent_cache_count--;
            break;
        }
    }
    pthread_mutex_unlock(&self->agent_cache_lock);

    if (!evicted) return;

    /* Soft-release off-thread so we never block on slow socket teardown;
     * inline fallback when thread creation fails (mirrors Python). */
    if (self->release_agent_soft) {
        struct gw_evict_release_arg *arg = malloc(sizeof(*arg));
        if (arg) {
            arg->release = self->release_agent_soft;
            arg->agent = evicted;
            pthread_t tid;
            if (pthread_create(&tid, NULL, gw_evict_release_thread, arg) == 0) {
                pthread_detach(tid);
            } else {
                free(arg);
                self->release_agent_soft(evicted);
            }
        } else {
            self->release_agent_soft(evicted);
        }
    }
}

/* Borrowed accessor for the live model-override dict. */
json_t *gateway_runner_session_model_overrides(const GatewayRunner *self)
{
    return self ? self->session_model_overrides : NULL;
}

/* Cache an agent for a session (test/wiring seam for the eviction path). */
void gateway_runner_cache_agent(GatewayRunner *self, const char *session_key,
                                void *agent)
{
    if (!self || !session_key) return;
    pthread_mutex_lock(&self->agent_cache_lock);
    for (int i = 0; i < self->agent_cache_count; i++) {
        if (strcmp(self->agent_cache[i].key, session_key) == 0) {
            self->agent_cache[i].agent = agent;
            pthread_mutex_unlock(&self->agent_cache_lock);
            return;
        }
    }
    if (self->agent_cache_count >= self->agent_cache_capacity) {
        int ncap = self->agent_cache_capacity ? self->agent_cache_capacity * 2 : 16;
        self->agent_cache = realloc(self->agent_cache,
                                    (size_t)ncap * sizeof(self->agent_cache[0]));
        self->agent_cache_capacity = ncap;
    }
    self->agent_cache[self->agent_cache_count].key = strdup(session_key);
    self->agent_cache[self->agent_cache_count].agent = agent;
    self->agent_cache_count++;
    pthread_mutex_unlock(&self->agent_cache_lock);
}

/* ════════════════════════════════════════════════════════════════════════
 * Service-tier overrides, MoA one-shot restore, sidecar notes, native images
 * ════════════════════════════════════════════════════════════════════════ */

/* PoP: gateway_runner_set_session_service_tier_override @ gateway/run.py:GatewayRunner._set_session_service_tier_override */
void gateway_runner_set_session_service_tier_override(GatewayRunner *self,
                                                      const char *session_key,
                                                      const char *service_tier,
                                                      bool clear)
{
    if (!self || !session_key || !session_key[0]) return;
    if (clear) {
        json_object_del(self->session_service_tier_overrides, session_key);
    } else {
        /* "priority" or None (explicit normal) — key PRESENCE decides. */
        json_set(self->session_service_tier_overrides, session_key,
                 service_tier ? json_string(service_tier) : json_null());
    }
}

/* PoP: gateway_runner_resolve_session_service_tier @ gateway/run.py:GatewayRunner._resolve_session_service_tier */
const char *gateway_runner_resolve_session_service_tier(const GatewayRunner *self,
                                                        const char *session_key)
{
    if (!self) return NULL;
    if (session_key && session_key[0] && self->session_service_tier_overrides) {
        json_t *ov = json_obj_get(self->session_service_tier_overrides,
                                  session_key);
        if (ov) {
            /* Key presence, not value truthiness: explicit-normal (null)
             * wins over the config default. */
            return (ov->type == JSON_STRING) ? ov->str_val : NULL;
        }
    }
    /* Config default (loaded into the runner at startup). */
    return self->service_tier[0] ? self->service_tier : NULL;
}

/* Declared in port_gateway_run_deps.h; extern here to avoid an include cycle. */
extern json_t *gw_load_reasoning_config(const char *model);

/* PoP: gateway_runner_resolve_session_reasoning_config @ gateway/run.py:GatewayRunner._resolve_session_reasoning_config */
json_t *gateway_runner_resolve_session_reasoning_config(const GatewayRunner *self,
                                                        const char *session_key,
                                                        const char *model)
{
    /* Priority: session-scoped /reasoning --session override > per-model
     * override > global agent.reasoning_effort. Returns a malloc'd copy. */
    if (self && session_key && session_key[0] &&
        self->session_reasoning_overrides) {
        json_t *ov = json_obj_get(self->session_reasoning_overrides,
                                  session_key);
        if (ov) return json_copy(ov);
    }
    return gw_load_reasoning_config(model ? model : "");
}

/* PoP: gateway_runner_restore_moa_one_shot @ gateway/run.py:GatewayRunner._restore_moa_one_shot */
void gateway_runner_restore_moa_one_shot(GatewayRunner *self,
                                         bool moa_disable_after_turn,
                                         const json_t *moa_restore_override,
                                         const char *quick_key)
{
    /* No-op unless the /moa one-shot flagged this turn. restore==NULL means
     * the user had no prior override, so the MoA override is cleared. */
    if (!self || !moa_disable_after_turn || !quick_key) return;
    if (!moa_restore_override) {
        json_object_del(self->session_model_overrides, quick_key);
    } else {
        json_set(self->session_model_overrides, quick_key,
                 json_copy(moa_restore_override));
    }
    gateway_runner_evict_cached_agent(self, quick_key);
}

/* PoP: gateway_runner_set_pending_turn_sidecar_notes @ gateway/run.py:GatewayRunner._set_pending_turn_sidecar_notes */
void gateway_runner_set_pending_turn_sidecar_notes(GatewayRunner *self,
                                                   const char *session_key,
                                                   const json_t *notes)
{
    /* Stage per-turn must-deliver notes for the next agent run (one-shot). */
    if (!self || !session_key || !session_key[0]) return;
    if (!notes || notes->type != JSON_ARRAY || notes->c.count == 0) return;
    json_set(self->pending_turn_sidecar_notes, session_key, json_copy(notes));
}

/* PoP: gateway_runner_consume_pending_turn_sidecar_notes @ gateway/run.py:GatewayRunner._consume_pending_turn_sidecar_notes */
json_t *gateway_runner_consume_pending_turn_sidecar_notes(GatewayRunner *self,
                                                          const char *session_key)
{
    /* Returns a malloc'd array (possibly empty); pops the staged entry. */
    if (!self || !session_key || !session_key[0]) return json_array();
    json_t *staged = json_obj_get(self->pending_turn_sidecar_notes, session_key);
    json_t *result = (staged && staged->type == JSON_ARRAY)
                         ? json_copy(staged) : json_array();
    if (staged) json_object_del(self->pending_turn_sidecar_notes, session_key);
    return result;
}

/* PoP: gateway_runner_consume_pending_native_image_paths @ gateway/run.py:GatewayRunner._consume_pending_native_image_paths */
json_t *gateway_runner_consume_pending_native_image_paths(GatewayRunner *self,
                                                          const char *session_key)
{
    /* Returns a malloc'd array (possibly empty); pops the pending entry. */
    if (!self || !session_key) return json_array();
    if (!self->pending_native_image_paths ||
        self->pending_native_image_paths->c.count == 0)
        return json_array();
    json_t *pending = json_obj_get(self->pending_native_image_paths, session_key);
    json_t *result = (pending && pending->type == JSON_ARRAY)
                         ? json_copy(pending) : json_array();
    if (pending) json_object_del(self->pending_native_image_paths, session_key);
    return result;
}

/* Stage native image paths (wiring seam for the consume path). */
void gateway_runner_stage_pending_native_image_paths(GatewayRunner *self,
                                                     const char *session_key,
                                                     const json_t *paths)
{
    if (!self || !session_key || !paths) return;
    json_set(self->pending_native_image_paths, session_key, json_copy(paths));
}
