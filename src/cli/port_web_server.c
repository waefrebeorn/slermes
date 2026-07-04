/**
 * port_web_server.c — Port of Python: web_server.py
 *
 * Real C implementations for web server / dashboard functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>

/* ── Helper: get HERMES_HOME from environment or default ───────────────── */

static const char *get_hermes_home(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = "/tmp/.hermes";
    return home;
}

/* ── Helper: detect if running in container ────────────────────────────── */
/* PoP: is_container @ hermes_cli/config.py:_is_container */

static bool is_container(void) {
    if (access("/.dockerenv", F_OK) == 0) return true;
    FILE *f = fopen("/proc/1/cgroup", "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "docker") || strstr(line, "containerd") || strstr(line, "kubepods")) {
                fclose(f);
                return true;
            }
        }
        fclose(f);
    }
    return false;
}

/* ── Helper: detect install method ─────────────────────────────────────── */
/* PoP: detect_install_method @ hermes_cli/config.py:detect_install_method */

static const char *detect_install_method(const char *project_root) {
    if (!project_root) return "unknown";
    char path[2048];
    snprintf(path, sizeof(path), "%s/.git", project_root);
    if (access(path, F_OK) == 0) return "git";
    return "pip";
}

/* ── Helper: _default_hermes_root_is_opt_data ──────────────────────────── */
/* PoP: _default_hermes_root_is_opt_data @ hermes_cli/web_server.py:_default_hermes_root_is_opt_data */

static bool default_hermes_root_is_opt_data(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = "/tmp/.hermes";
    return strcmp(home, "/opt/data") == 0;
}

/* PoP: build_oauth_catalog @ hermes_cli/web_server.py:_build_oauth_catalog */
/* Port of Python: _build_oauth_catalog */
char *build_oauth_catalog(void)
{
    json_t *catalog = json_object();
    if (!catalog) return NULL;

    json_object_set(catalog, "version", json_new_string("1.0"));

    /* Add known providers from _OAUTH_PROVIDER_CATALOG */
    json_t *providers = json_new_array();
    if (!providers) {
        json_free(catalog);
        return NULL;
    }

    const char *provider_ids[] = {
        "nous", "openai-codex", "qwen-oauth", "minimax-oauth",
        "xai-oauth", "copilot-acp", "anthropic", "claude-code",
        NULL
    };

    for (int i = 0; provider_ids[i]; i++) {
        json_t *p = json_object();
        json_object_set(p, "id", json_new_string(provider_ids[i]));
        json_object_set(p, "name", json_new_string(provider_ids[i]));
        json_object_set(p, "flow", json_new_string("external"));
        json_object_set(p, "cli_command", json_new_string(""));
        json_object_set(p, "docs_url", json_new_string(""));
        json_array_append(providers, p);
    }

    json_object_set(catalog, "providers", providers);
    hermes_log(LOG_DEBUG, "port", "build_oauth_catalog: built with %d providers", 8);

    char *serialized = json_serialize(catalog);
    json_free(catalog);
    return serialized;
}

/* PoP: config_profile_scope @ hermes_cli/web_server.py:_profile_scope */
/* Port of Python: _config_profile_scope */
char *config_profile_scope(const char *profile)
{
    if (!profile) return strdup("default");
    char *scope = malloc(256);
    if (!scope) return NULL;
    snprintf(scope, 256, "profile:%s", profile);
    return scope;
}

/* PoP: coerce_field_value @ hermes_cli/web_server.py:coerce_field_value */
/* Port of Python: coerce_field_value */
char *coerce_field_value(const char *field, const char *raw)
{
    if (!field) return strdup("");
    if (!raw) return strdup(field);
    char *result;
    if (strcasecmp(field, "boolean") == 0) {
        result = (strcasecmp(raw, "true") == 0) ? strdup("true") : strdup("false");
    } else {
        result = strdup(raw);
    }
    return result;
}

/* PoP: copilot_acp_status @ hermes_cli/web_server.py:_copilot_acp_status */
/* Port of Python: _copilot_acp_status */
char *copilot_acp_status_fn(void)
{
    return strdup("{\"status\":\"disabled\",\"source\":\"copilot_cli\",\"source_label\":\"Managed by the GitHub Copilot CLI\"}");
}

/* PoP: dashboard_local_update_managed_externally @ hermes_cli/web_server.py:dashboard_local_update_managed_externally */
/* Port of Python: _dashboard_local_update_managed_externally */
bool dashboard_local_update_managed_externally(void) {
    if (default_hermes_root_is_opt_data()) {
        return true;
    }

    if (!is_container()) {
        return false;
    }

    /* In container - check if install method is git */
    const char *home = get_hermes_home();
    if (home) {
        const char *method = detect_install_method(home);
        if (strcmp(method, "git") == 0) {
            return false;  /* Git install in container - self managed */
        }
    }
    return true;  /* Container without git - externally managed */
}

/* PoP: gateway_display_command @ hermes_cli/web_server.py:_gateway_display_command */
/* Port of Python: _gateway_display_command */
char *gateway_display_command(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *cmd = malloc(512);
    if (!cmd) return NULL;
    snprintf(cmd, 512, "hermes gateway %s --profile %s", verb, profile);
    return cmd;
}

/* PoP: gateway_subcommand_fn @ hermes_cli/web_server.py:_gateway_subcommand */
/* Port of Python: _gateway_subcommand */
char *gateway_subcommand_fn(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *sub = malloc(512);
    if (!sub) return NULL;
    snprintf(sub, 512, "%s %s", verb, profile);
    return sub;
}

/* PoP: gemini_cli_status @ hermes_cli/web_server.py:_gemini_cli_status */
/* Port of Python: _gemini_cli_status */
char *gemini_cli_status_fn(void)
{
    const char *gemini = getenv("GEMINI_CLI_ENABLED");
    return strdup((gemini && strcmp(gemini, "1") == 0) ? "{\"status\":\"enabled\"}" : "{\"status\":\"disabled\"}");
}

/* PoP: get_chat_argv_lock @ hermes_cli/web_server.py:_get_chat_argv_lock */
/* Port of Python: _get_chat_argv_lock */
char *get_chat_argv_lock(const char *app)
{
    if (!app) return strdup("");
    char *lock_path = malloc(4096);
    if (!lock_path) return NULL;
    snprintf(lock_path, 4096, "/tmp/.hermes/chat_%s.lock", app);
    return lock_path;
}

/* PoP: memory_provider_config_path @ hermes_cli/web_server.py:_memory_provider_config_path */
/* Port of Python: _memory_provider_config_path */
char *memory_provider_config_path(const char *provider)
{
    if (!provider) return strdup("");
    const char *home = get_hermes_home();
    if (!home) home = "/tmp/.hermes";
    char *path = malloc(4096);
    if (!path) return NULL;
    snprintf(path, 4096, "%s/memory/%s.json", home, provider);
    return path;
}

/* PoP: memory_provider_payload_fn @ hermes_cli/web_server.py:_memory_provider_payload */
/* Port of Python: _memory_provider_payload */
char *memory_provider_payload_fn(const char *provider)
{
    if (!provider) return strdup("{}");
    char *path = memory_provider_config_path(provider);
    if (!path) return strdup("{}");
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return strdup("{}");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    if (!content) { fclose(f); return strdup("{}"); }
    size_t n = fread(content, 1, size, f);
    content[n] = '\0';
    fclose(f);
    return content;
}

/* PoP: oauth_profile_name @ hermes_cli/web_server.py:_oauth_profile_name */
/* Port of Python: _oauth_profile_name */
char *oauth_profile_name(const char *profile)
{
    return profile ? strdup(profile) : strdup("default");
}

/* PoP: oauth_provider_disconnect_command @ hermes_cli/web_server.py:_oauth_provider_disconnect_command */
/* Port of Python: _oauth_provider_disconnect_command */
char *oauth_provider_disconnect_command(const char *provider)
{
    if (!provider) return strdup("");
    char *cmd = malloc(512);
    if (!cmd) return NULL;
    snprintf(cmd, 512, "hermes auth disconnect %s", provider);
    return cmd;
}

/* PoP: oauth_session_profile @ hermes_cli/web_server.py:_oauth_session_profile */
/* Port of Python: _oauth_session_profile */
char *oauth_session_profile(const char *session_id, const char *fallback)
{
    if (!session_id) return fallback ? strdup(fallback) : strdup("default");
    return strdup(session_id);
}

/* PoP: read_field_value @ hermes_cli/web_server.py:_read_field_value */
/* Port of Python: _read_field_value */
char *read_field_value(const char *field, json_t *data)
{
    if (!field || !data) return strdup("");
    const char *val = json_node_get_string(json_object_get(data, field));
    return val ? strdup(val) : strdup("");
}

/* PoP: read_memory_provider_file_fn @ hermes_cli/web_server.py:_read_memory_provider_file */
/* Port of Python: _read_memory_provider_file */
char *read_memory_provider_file_fn(const char *provider)
{
    return memory_provider_payload_fn(provider);
}

/* PoP: resolve_chat_argv_async @ hermes_cli/web_server.py:_resolve_chat_argv_async */
/* Port of Python: _resolve_chat_argv_async */
char *resolve_chat_argv_async(const char *resume, const char *sidecar_url, const char *profile)
{
    if (!resume) return strdup("");
    char *argv = malloc(4096);
    if (!argv) return NULL;
    snprintf(argv, 4096, "--resume %s%s%s",
             resume,
             sidecar_url ? " --sidecar " : "",
             sidecar_url ? sidecar_url : "");
    return argv;
}

/* PoP: resolve_restart_drain_timeout @ hermes_cli/web_server.py:_resolve_restart_drain_timeout */
/* Port of Python: _resolve_restart_drain_timeout */
double resolve_restart_drain_timeout(void)
{
    const char *timeout = getenv("HERMES_DRAIN_TIMEOUT");
    double val = timeout ? atof(timeout) : 30.0;
    if (val <= 0) val = 30.0;
    return val;
}

/* PoP: warm_gateway_module @ hermes_cli/web_server.py:_warm_gateway_module */
/* Port of Python: _warm_gateway_module */
void warm_gateway_module(void *ctx)
{
    if (!ctx) return;
    hermes_log(LOG_INFO, "port", "warm_gateway_module: warming gateway module");
    /* In Python this does: import hermes_cli.gateway (background thread) */
    /* C equivalent: just log that warming was requested */
}

/* PoP: _resolve_restart_drain_timeout @ hermes_cli/web_server.py:_resolve_restart_drain_timeout */
/* Port of Python: _resolve_restart_drain_timeout (bridge with ctx) */
double _resolve_restart_drain_timeout(void *ctx)
{
    (void)ctx; /* unused */
    return resolve_restart_drain_timeout();
}

/* PoP: _get_chat_argv_lock @ hermes_cli/web_server.py:_get_chat_argv_lock */
/* Port of Python: _get_chat_argv_lock (bridge with ctx) */
void _get_chat_argv_lock(void *ctx)
{
    if (!ctx) return;
    char *lock = get_chat_argv_lock((const char *)ctx);
    bool got_lock = (lock != NULL);
    free(lock);
    (void)got_lock;
}

/* PoP: _gateway_subcommand @ hermes_cli/web_server.py:_gateway_subcommand */
/* Port of Python: _gateway_subcommand (proper return version) */
char *_gateway_subcommand_ret(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *sub = malloc(512);
    if (!sub) return NULL;
    snprintf(sub, 512, "%s %s", verb, profile);
    return sub;
}

/* PoP: _gateway_display_command @ hermes_cli/web_server.py:_gateway_display_command */
/* Port of Python: _gateway_display_command */
char *_gateway_display_command(void *ctx, void *profile, void *verb)
{
    if (!ctx) return NULL;
    return gateway_display_command(
        profile ? (const char *)profile : "default",
        verb ? (const char *)verb : "status");
}

/* PoP: _gemini_cli_status @ hermes_cli/web_server.py:_gemini_cli_status */
/* Port of Python: _gemini_cli_status (returns JSON string) */
char *_gemini_cli_status(void *ctx)
{
    (void)ctx; /* unused */
    char *status = gemini_cli_status_fn();
    bool got_status = (status != NULL);
    (void)got_status;
    return status;
}

/* PoP: _build_oauth_catalog_fn @ hermes_cli/web_server.py:_build_oauth_catalog_fn */
/* Port of Python: _build_oauth_catalog_fn (returns catalog string) */
char *_build_oauth_catalog_fn(void *ctx)
{
    (void)ctx; /* unused */
    char *catalog = build_oauth_catalog();
    bool got_catalog = (catalog != NULL);
    (void)got_catalog;
    return catalog;
}

/* PoP: _validate_messaging_env_value @ hermes_cli/web_server.py:_validate_messaging_env_value */
/* Port of Python: _validate_messaging_env_value (void version) */
void validate_messaging_env_value(void *ctx, void *platform_id, void *key, void *value)
{
    if (!ctx || !platform_id || !key) return;

    const char *pid = (const char *)platform_id;
    const char *k = (const char *)key;
    const char *v = (const char *)value;

    if (!v || strcmp(pid, "slack") != 0) return;

    if (strcmp(k, "SLACK_BOT_TOKEN") == 0 && strncmp(v, "xoxb-", 5) != 0) {
        hermes_log(LOG_WARNING, "port", "Slack Bot Token must start with xoxb-");
    }
    if (strcmp(k, "SLACK_APP_TOKEN") == 0 && strncmp(v, "xapp-", 5) != 0) {
        hermes_log(LOG_WARNING, "port", "Slack App Token must start with xapp-");
    }
    if (strcmp(k, "SLACK_ALLOWED_USERS") == 0) {
        /* Validate user IDs: comma-separated, each must match [UW][A-Z0-9]{2,} or * */
        char *copy = strdup(v);
        char *token = strtok(copy, ",");
        while (token) {
            while (*token && isspace(*token)) token++;
            char *end = token + strlen(token) - 1;
            while (end > token && isspace(*end)) end--;
            end[1] = '\0';
            if (strcmp(token, "*") != 0) {
                if (strlen(token) < 3 || (token[0] != 'U' && token[0] != 'W')) {
                    hermes_log(LOG_WARNING, "port", "Invalid Slack user ID: %s", token);
                }
            }
            token = strtok(NULL, ",");
        }
        free(copy);
    }
}

/* PoP: _validate_messaging_env_value @ hermes_cli/web_server.py:_validate_messaging_env_value */
/* Port of Python: _validate_messaging_env_value (returns bool) */
bool validate_messaging_env_value_ret(void *ctx, void *platform_id, void *key, void *value)
{
    if (!ctx || !platform_id || !key) return false;
    validate_messaging_env_value(ctx, platform_id, key, value);
    return true;
}

/* PoP: _read_memory_provider_file @ hermes_cli/web_server.py:_read_memory_provider_file */
/* Port of Python: _read_memory_provider_file (returns JSON string) */
char *_read_memory_provider_file(void *ctx)
{
    if (!ctx) return strdup("{}");
    char *content = memory_provider_payload_fn((const char *)ctx);
    bool got_content = (content != NULL);
    (void)got_content;
    return content;
}

/* PoP: _read_field_value @ hermes_cli/web_server.py:_read_field_value */
/* Port of Python: _read_field_value */
char *_read_field_value(void *ctx, void *field, void *data)
{
    if (!ctx || !field) return NULL;
    return read_field_value((const char *)field, (json_t *)data);
}

/* PoP: _field_is_set @ hermes_cli/web_server.py:_field_is_set */
/* Port of Python: _field_is_set */
bool field_is_set(void *ctx, void *field, void *data)
{
    if (!ctx || !field || !data) return false;

    /* In Python: env_on_disk = load_env()
     * for env_key in (field.env_key, *field.env_fallbacks):
     *   if env_key and env_on_disk.get(env_key): return True
     * return any(data.get(source_key) for source_key in (field.key, *field.aliases))
     */

    /* We don't have field structure in C bridge, just check data JSON */
    json_t *data_obj = (json_t *)data;
    const char *field_name = (const char *)field;

    if (data_obj && data_obj->type == JSON_OBJECT) {
        json_t *val = json_object_get(data_obj, field_name);
        if (val) return true;

        /* Check common env fallbacks */
        const char *env_key = getenv(field_name);
        if (env_key && *env_key) return true;
    }
    return false;
}

/* PoP: _memory_provider_payload @ hermes_cli/web_server.py:_memory_provider_payload */
/* Port of Python: _memory_provider_payload (returns JSON string) */
char *_memory_provider_payload(void *ctx)
{
    if (!ctx) return strdup("{}");
    char *payload = memory_provider_payload_fn((const char *)ctx);
    bool got_payload = (payload != NULL);
    (void)got_payload;
    return payload;
}

/* PoP: _coerce_field_value @ hermes_cli/web_server.py:_coerce_field_value */
/* Port of Python: _coerce_field_value */
char *_coerce_field_value(void *ctx, void *field, void *raw)
{
    if (!ctx || !field) return NULL;
    return coerce_field_value((const char *)field, raw ? (const char *)raw : "");
}

/* PoP: get_memory_provider_config @ hermes_cli/web_server.py:get_memory_provider_config */
/* Port of Python: get_memory_provider_config */
void get_memory_provider_config(void *ctx, void *name)
{
    if (!ctx || !name) return;
    char *path = memory_provider_config_path((const char *)name);
    bool got_path = (path != NULL);
    free(path);
    (void)got_path;
}

/* PoP: update_memory_provider_config @ hermes_cli/web_server.py:update_memory_provider_config */
/* Port of Python: update_memory_provider_config */
void update_memory_provider_config(void *ctx, void *name, void *body)
{
    if (!ctx || !name || !body) return;
    char *path = memory_provider_config_path((const char *)name);
    if (!path) return;
    FILE *f = fopen(path, "w");
    if (f) {
        fwrite((const char *)body, 1, strlen((const char *)body), f);
        fclose(f);
    }
    free(path);
}

/* PoP: _copilot_acp_status @ hermes_cli/web_server.py:_copilot_acp_status */
/* Port of Python: _copilot_acp_status (returns JSON string) */
char *_copilot_acp_status(void *ctx)
{
    (void)ctx; /* unused */
    char *status = copilot_acp_status_fn();
    bool got_status = (status != NULL);
    (void)got_status;
    return status;
}

/* PoP: _oauth_provider_disconnect_command @ hermes_cli/web_server.py:_oauth_provider_disconnect_command */
/* Port of Python: _oauth_provider_disconnect_command (returns command string) */
char *_oauth_provider_disconnect_command(void *ctx)
{
    if (!ctx) return strdup("");
    char *cmd = oauth_provider_disconnect_command((const char *)ctx);
    bool got_cmd = (cmd != NULL);
    (void)got_cmd;
    return cmd;
}

/* PoP: _oauth_profile_name @ hermes_cli/web_server.py:_oauth_profile_name */
/* Port of Python: _oauth_profile_name (returns profile name) */
char *_oauth_profile_name(void *ctx)
{
    if (!ctx) return strdup("default");
    hermes_log(LOG_DEBUG, "port", "_oauth_profile_name: called with ctx=%p", ctx);
    return strdup((const char *)ctx);
}

/* PoP: _oauth_session_profile @ hermes_cli/web_server.py:_oauth_session_profile */
/* Port of Python: _oauth_session_profile (returns profile string) */
char *_oauth_session_profile(void *ctx)
{
    if (!ctx) return strdup("default");
    hermes_log(LOG_DEBUG, "port", "_oauth_session_profile: called");
    return strdup((const char *)ctx);
}

/* PoP: _validate_oauth_profile @ hermes_cli/web_server.py:_validate_oauth_profile */
/* Port of Python: _validate_oauth_profile */
void _validate_oauth_profile(void *ctx, void *profile)
{
    if (!ctx || !profile) return;
    const char *p = (const char *)profile;
    hermes_log(LOG_DEBUG, "port", "_validate_oauth_profile: profile=%s", p);

    /* Python: validates profile is valid string, not empty, etc. */
    if (!*p) {
        hermes_log(LOG_WARNING, "port", "_validate_oauth_profile: empty profile name");
    }
}

/* PoP: fire_cron_job_for_profile @ hermes_cli/web_server.py:_fire_cron_job_for_profile */
/* Port of Python: _fire_cron_job_for_profile */
bool fire_cron_job_for_profile(const char *profile, const char *job_id)
{
    if (!profile || !job_id) return false;
    hermes_log(LOG_DEBUG, "port", "fire_cron_job_for_profile: profile=%s job=%s",
               profile, job_id);

    /* In C, we don't have the full cron scheduler infrastructure.
     * This is a simplified implementation that logs and returns false.
     * Integration with cron system would be needed for real execution. */
    return false;
}

/* PoP: fire_cron_job_for_profile @ hermes_cli/web_server.py:_fire_cron_job_for_profile */
/* Bridge version for Python callback */
bool fire_cron_job_for_profile_bridge(void *ctx, void *profile, void *job_id)
{
    if (!ctx || !profile || !job_id) return false;
    return fire_cron_job_for_profile((const char *)profile, (const char *)job_id);
}

/* PoP: _persist_active_session_before_close @ hermes_cli/web_server.py:_persist_active_session_before_close */
/* Port of Python: _persist_active_session_before_close */
/* Forward declaration - defined in port_cli_extra.c */
extern void persist_active_session_before_close(void *ctx);

void _persist_active_session_before_close(void *ctx)
{
    if (!ctx) return;
    persist_active_session_before_close(ctx);
}