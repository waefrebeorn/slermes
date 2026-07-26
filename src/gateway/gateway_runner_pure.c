/* gateway_runner_pure.c — GatewayRunner data model + pure accessors
 *
 * Ports:
 *   gateway/run.py class GatewayRunner static methods & accessors
 *   gateway/slash_commands.py static methods
 *
 * Opaque struct holds the runtime state snapshot. All accessors are
 * pure transformations of that state — no async, no I/O.
 */
#define _GNU_SOURCE
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

/* ─── Gateway runner mode strings ─────────────────────────────────── */

#define GW_BUSY_INTERRUPT "interrupt"
#define GW_BUSY_QUEUE     "queue"
#define GW_BUSY_STEER     "steer"

/* ─── GatewayRunner runtime state snapshot ─────────────────────────── */

typedef struct {
    /* Busy/drain behavior */
    char busy_input_mode[24];     /* "interrupt", "queue", "steer" */
    char busy_text_mode[24];      /* "interrupt", "queue" */
    double restart_drain_timeout; /* seconds */
    char service_tier[24];        /* "priority" or "" */
    bool show_reasoning;
    char background_notif_mode[16]; /* "all", "result", "error", "off" */
    /* Lifecycle flags */
    bool running;
    bool draining;
    bool exit_cleanly;
    bool exit_with_failure;
    char exit_reason[256];
    int exit_code;
    double gateway_started_at;
    /* Restart */
    bool restart_requested;
    bool restart_detached;
    bool restart_via_service;
    /* External drain */
    bool external_drain_active;
    /* Scale-to-zero */
    double scale_to_zero_idle_timeout;
    /* Prefill / ephemeral */
    char ephemeral_system_prompt[4096];
    bool has_prefill_messages;
    /* Fallback / routing */
    bool has_fallback_model;
    bool has_provider_routing;
    /* Session limit */
    int max_concurrent_sessions;
} GatewayRunnerState;

/* ─── Config string-to-enum helpers ─────────────────────────────────── */

/* PoP: gw_resolve_busy_input_mode @ gateway/run.py:GatewayRunner._load_busy_input_mode */
const char *gw_resolve_busy_input_mode(const char *cfg_value)
{
    if (!cfg_value || !cfg_value[0]) return GW_BUSY_INTERRUPT;
    if (strcasecmp(cfg_value, "queue") == 0) return GW_BUSY_QUEUE;
    if (strcasecmp(cfg_value, "steer") == 0) return GW_BUSY_STEER;
    return GW_BUSY_INTERRUPT;
}

/* PoP: gw_resolve_busy_text_mode @ gateway/run.py:GatewayRunner._load_busy_text_mode */
const char *gw_resolve_busy_text_mode(const char *cfg_input_mode,
                                       const char *legacy_text_mode)
{
    /* Legacy explicit override wins */
    if (legacy_text_mode && legacy_text_mode[0]) {
        if (strcasecmp(legacy_text_mode, "interrupt") == 0)
            return GW_BUSY_INTERRUPT;
        if (strcasecmp(legacy_text_mode, "queue") == 0)
            return GW_BUSY_QUEUE;
    }
    /* Follow busy_input_mode */
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
    /* Explicit value in config */
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
    /* Default */
    if (default_cfg) {
        if (strcasecmp(default_cfg, "true") == 0 ||
            strcasecmp(default_cfg, "1") == 0) return true;
    }
    return false;
}

/* PoP: gw_resolve_background_notif_mode @ gateway/run.py:GatewayRunner._load_background_notifications_mode */
const char *gw_resolve_background_notif_mode(const char *cfg_raw, int is_bool, int bool_val)
{
    /* Config value false → off */
    if (is_bool && !bool_val) return "off";
    /* String value */
    if (cfg_raw && cfg_raw[0]) {
        if (strcasecmp(cfg_raw, "result") == 0) return "result";
        if (strcasecmp(cfg_raw, "error") == 0) return "error";
        if (strcasecmp(cfg_raw, "off") == 0) return "off";
    }
    return "all"; /* default */
}

/* PoP: gw_resolve_restart_drain_timeout @ gateway/run.py:GatewayRunner._load_restart_drain_timeout */
double gw_resolve_restart_drain_timeout(const char *raw, double default_val)
{
    if (!raw || !raw[0]) return default_val;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || *end != '\0') return default_val;
    if (val <= 0) return default_val;
    if (val != val) return default_val; /* NaN */
    return val;
}

/* ─── GatewayRunner pure accessors ──────────────────────────────────── */

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

    char buf[512];
    const char *p = raw_args;
    while (*p && (unsigned char)*p <= ' ') p++;
    size_t n = 0;
    while (*p && n < sizeof(buf) - 1) {
        buf[n++] = *p; p++;
    }
    buf[n] = '\0';

    /* Simple tokenizer: split by whitespace */
    char *tokens[32];
    int nt = 0;
    char *tok = strtok(buf, " \t");
    while (tok && nt < 32) {
        tokens[nt++] = tok;
        tok = strtok(NULL, " \t");
    }

    /* Process tokens */
    size_t val_pos = 0;
    for (int i = 0; i < nt; i++) {
        if (strcmp(tokens[i], "--global") == 0) {
            if (persist_global) *persist_global = true;
        } else {
            if (value_out && val_pos < value_size) {
                size_t len = strlen(tokens[i]);
                if (val_pos > 0 && val_pos < value_size - 1)
                    value_out[val_pos++] = ' ';
                if (val_pos + len < value_size) {
                    memcpy(value_out + val_pos, tokens[i], len);
                    val_pos += len;
                }
            }
        }
    }
    if (value_out) value_out[val_pos] = '\0';
    /* Downcase */
    if (value_out) {
        for (size_t i = 0; value_out[i]; i++)
            value_out[i] = (char)tolower((unsigned char)value_out[i]);
    }
}

/* PoP: gw_get_guild_id @ gateway/run.py:GatewayRunner._get_guild_id */
long gw_get_guild_id(const char *event_json_source)
{
    /* Extract guild_id from serialized source JSON */
    if (!event_json_source) return 0;
    const char *p = strstr(event_json_source, "\"guild_id\"");
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && (unsigned char)*p <= ' ') p++;
    return strtol(p, NULL, 10);
}

/* PoP: gw_redact_matrix_session_key @ gateway/slash_commands.py:GatewaySlashCommandsMixin._redact_matrix_session_key */
void gw_redact_matrix_session_key(const char *session_key,
                                   char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (!session_key) { out[0] = '\0'; return; }

    /* Simple hash-like fingerprint (hex tail of input) */
    size_t len = strlen(session_key);
    unsigned long hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)session_key[i];
    }
    snprintf(out, out_size, "sha256:%08lx", hash & 0xFFFFFFFF);
}

/* PoP: gw_sanitize_telegram_topic_title @ gateway/run.py:GatewayRunner._sanitize_telegram_topic_title */
void gw_sanitize_telegram_topic_title(const char *title,
                                       char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!title) return;

    /* Telegram topic titles: max 128 chars, strip emoji/control */
    size_t pos = 0;
    for (const char *p = title; *p && pos < out_size - 1 && pos < 128; p++) {
        unsigned char c = (unsigned char)*p;
        /* Skip control chars and standalone combining marks (crude) */
        if (c < 0x20 && c != 0x09) continue;
        /* Skip surrogate-like high bytes (crude emoji skip for ASCII output) */
        if (c >= 0x80) continue;
        out[pos++] = (char)c;
    }
    /* Trim trailing whitespace */
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

    /* Discord thread titles: max 100 chars, basic sanitize */
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
    /* Extract delivery identity from JSON blob */
    if (!delivery_json) return NULL;
    const char *p = strstr(delivery_json, "\"identity\"");
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p && (unsigned char)*p <= ' ') p++;
    if (*p == '"') {
        p++;
        static char ident[256];
        size_t pos = 0;
        while (*p && *p != '"' && pos < sizeof(ident) - 1)
            ident[pos++] = *p++;
        ident[pos] = '\0';
        return ident;
    }
    /* Numeric identity */
    static char ident[256];
    size_t pos = 0;
    while (*p && (unsigned char)*p > ' ' && *p != ',' && *p != '}' && pos < sizeof(ident) - 1)
        ident[pos++] = *p++;
    ident[pos] = '\0';
    return ident;
}

/* PoP: gw_ephemeral_change_key @ gateway/run.py:GatewayRunner._ephemeral_change_key */
void gw_ephemeral_change_key(const char *session_key, const char *prefix,
                              char *out, size_t out_size)
{
    if (!out || out_size == 0) return;
    if (!session_key) { out[0] = '\0'; return; }
    if (prefix)
        snprintf(out, out_size, "%s:%s", prefix, session_key);
    else
        snprintf(out, out_size, "change:%s", session_key);
}

/* ─── GatewayRunner lifecycle accessors ───────────────────────────── */

/* PoP: gw_should_exit_cleanly @ gateway/run.py:GatewayRunner.should_exit_cleanly */
bool gw_should_exit_cleanly(const GatewayRunnerState *state)
{
    return state ? state->exit_cleanly : false;
}

/* PoP: gw_should_exit_with_failure @ gateway/run.py:GatewayRunner.should_exit_with_failure */
bool gw_should_exit_with_failure(const GatewayRunnerState *state)
{
    return state ? state->exit_with_failure : false;
}

/* PoP: gw_exit_reason @ gateway/run.py:GatewayRunner.exit_reason */
const char *gw_exit_reason(const GatewayRunnerState *state)
{
    return state ? state->exit_reason : NULL;
}

/* PoP: gw_exit_code @ gateway/run.py:GatewayRunner.exit_code */
int gw_exit_code(const GatewayRunnerState *state)
{
    return state ? state->exit_code : -1;
}

/* PoP: gw_running_agent_count @ gateway/run.py:GatewayRunner._running_agent_count */
int gw_running_agent_count(const GatewayRunnerState *state)
{
    (void)state;
    /* Runtime-provided — stub returns -1 (unknown) */
    return -1;
}

/* PoP: gw_has_setup_skill @ gateway/run.py:GatewayRunner._has_setup_skill */
bool gw_has_setup_skill(const char *config_value)
{
    return config_value && config_value[0];
}

/* PoP: gw_scale_to_zero_idle_timeout @ gateway/run.py:GatewayRunner._scale_to_zero_idle_timeout_seconds */
double gw_scale_to_zero_idle_timeout(const char *cfg_val, double default_val)
{
    if (!cfg_val || !cfg_val[0]) return default_val;
    char *end = NULL;
    double v = strtod(cfg_val, &end);
    if (end == cfg_val || *end != '\0') return default_val;
    if (v <= 0 || v != v) return default_val;
    return v;
}

/* PoP: gw_goal_max_turns_from_config @ gateway/run.py:GatewayRunner._goal_max_turns_from_config */
int gw_goal_max_turns_from_config(const char *cfg_val, int default_val)
{
    if (!cfg_val || !cfg_val[0]) return default_val;
    char *end = NULL;
    long v = strtol(cfg_val, &end, 10);
    if (end == cfg_val || *end != '\0') return default_val;
    if (v < 0) return default_val;
    return (int)v;
}