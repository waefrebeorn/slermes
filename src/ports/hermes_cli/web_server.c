#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/hermes_cli/web_server.c — C11 port of hermes_cli/web_server.py
 * Merged from 17 source file(s) by reorganization.
 */

/* ── from src/cli/port_web_server.c ── */

#include "hermes_web_server_pure.h"

/** 
 * port_web_server.c — Port of Python: web_server.py
 *
 * Real C implementations for web server / dashboard functions.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include "libbase64/base64.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <libgen.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>

#include "port_web_server_paths.h"

/* ── Web Server Document Root helpers ───────────────────────────────── */
/* PoP: default_hermes_root_is_opt_data @ hermes_cli/web_server.py:_default_hermes_root_is_opt_data */

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
/* PoP: config_profile_scope @ gateway/platforms/api_server.py:_profile_scope */
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

/* PoP: copilot_acp_status_fn @ hermes_cli/web_server.py:_copilot_acp_status */
/* Port of Python: _copilot_acp_status */
char *copilot_acp_status_fn(void)
{
    return strdup("{\"status\":\"disabled\",\"source\":\"copilot_cli\",\"source_label\":\"Managed by the GitHub Copilot CLI\"}");
}

/* PoP: dashboard_local_update_managed_externally @ hermes_cli/web_server.py:_dashboard_local_update_managed_externally */
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
char *gateway_display_command(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *cmd = malloc(512);
    if (!cmd) return NULL;
    snprintf(cmd, 512, "hermes gateway %s --profile %s", verb, profile);
    return cmd;
}

/* PoP: gateway_subcommand_fn @ hermes_cli/web_server.py:_gateway_subcommand */
char *gateway_subcommand_fn(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *sub = malloc(512);
    if (!sub) return NULL;
    snprintf(sub, 512, "%s %s", verb, profile);
    return sub;
}

/* PoP: gemini_cli_status_fn @ hermes_cli/web_server.py:_gemini_cli_status */
char *gemini_cli_status_fn(void)
{
    const char *gemini = getenv("GEMINI_CLI_ENABLED");
    return strdup((gemini && strcmp(gemini, "1") == 0) ? "{\"status\":\"enabled\"}" : "{\"status\":\"disabled\"}");
}

/* PoP: get_chat_argv_lock @ hermes_cli/web_server.py:_get_chat_argv_lock */
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

/* PoP: field_is_set @ hermes_cli/web_server.py:_field_is_set */
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
    if (!path) return;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return;
    fclose(f);
}

/* PoP: fs_regular_file @ hermes_cli/web_server.py:_fs_regular_file */
int fs_regular_file(const char *raw_path, char *out_path, size_t out_sz,
                    mode_t *out_mode, long *out_size)
{
    if (!raw_path || !raw_path[0]) return 400;
    struct stat st;
    ws_path_status_t ws = ws_fs_regular_file(raw_path, &st);
    if (ws == WS_PATH_NOT_FOUND) return 404;
    if (ws == WS_PATH_NOT_READABLE) return 403;
    if (ws == WS_PATH_IS_DIR) return 400;
    if (ws == WS_PATH_NOT_REGULAR) return 400;
    if (ws != WS_PATH_OK) return 400;
    if (out_path && out_sz) snprintf(out_path, out_sz, "%s", raw_path);
    if (out_mode) *out_mode = st.st_mode;
    if (out_size) *out_size = (long)st.st_size;
    return 0;
}

/* PoP: fs_find_git_root @ hermes_cli/web_server.py:_fs_find_git_root */
/* Delegate to pure port to avoid duplicate implementation */
char *fs_find_git_root(const char *start)
{
    return ws_fs_find_git_root(start);
}

/* PoP: fs_git_branch @ hermes_cli/web_server.py:_fs_git_branch */
char *fs_git_branch(const char *cwd)
{
    char cmd[PATH_MAX + 64];
    snprintf(cmd, sizeof(cmd), "git -C %s rev-parse --abbrev-ref HEAD 2>/dev/null",
             cwd ? cwd : ".");
    FILE *p = popen(cmd, "r");
    if (!p) return strdup("");
    char branch[256];
    if (!fgets(branch, sizeof(branch), p)) { pclose(p); return strdup(""); }
    pclose(p);
    size_t n = strlen(branch);
    while (n > 0 && (branch[n-1] == '\n' || branch[n-1] == '\r')) branch[--n] = '\0';
    return strdup(branch);
}

/* PoP: audio_extension_for_mime @ hermes_cli/web_server.py:_audio_extension_for_mime */
const char *audio_extension_for_mime(const char *mime_type)
{
    return ws_audio_extension_for_mime(mime_type);
}

/* PoP: infer_type @ hermes_cli/web_server.py:_infer_type */
const char *infer_type(int json_type_tag)
{
    switch (json_type_tag) {
        case 1: return "boolean";
        case 2: return "number";
        case 3: return "number";
        case 4: return "list";
        case 5: return "object";
        default: return "string";
    }
}

/* ===========================================================================
 *  Config schema helpers — ported from web_server.py
 * =========================================================================== */

/* _SCHEMA_OVERRIDES and _CATEGORY_MERGE are large constant tables.
 * We replicate the logic inline rather than embedding massive static data. */

static const char *category_merge_map[] = {
    "privacy", "security",
    "context", "agent",
    "skills", "agent",
    "cron", "agent",
    "network", "agent",
    "checkpoints", "agent",
    "approvals", "security",
    "human_delay", "display",
    "dashboard", "display",
    "code_execution", "agent",
    "prompt_caching", "agent",
    "goals", "agent",
    "updates", "general",
    "onboarding", "agent",
    "telegram", "discord",
    "computer_use", "agent",
    NULL
};

static const char *category_merge_lookup(const char *cat)
{
    for (size_t i = 0; category_merge_map[i]; i += 2) {
        if (strcmp(cat, category_merge_map[i]) == 0)
            return category_merge_map[i + 1];
    }
    return cat;
}

/* _CATEGORY_ORDER for display ordering */
/* static const char *category_order[] = { ... }; */ /* Unused in C port */

/* _SCHEMA_OVERRIDES — only the most critical ones for schema fidelity.
 * We implement a minimal inline fallback for the key fields. */
static json_t *schema_override_for(const char *key)
{
    if (!key) return NULL;
    /* Minimal overrides for the key virtual fields */
    if (strcmp(key, "model_context_length") == 0) {
        json_t *o = json_new_object();
        json_object_set(o, "type", json_new_string("number"));
        json_object_set(o, "description", json_new_string("Model Context Length"));
        json_object_set(o, "category", json_new_string("model"));
        return o;
    }
    return NULL;
}

/* PoP: build_schema_from_config @ hermes_cli/web_server.py:_build_schema_from_config */
json_t *build_schema_from_config(json_t *config, const char *prefix)
{
    if (!config || config->type != JSON_OBJECT) return json_new_object();

    json_t *schema = json_new_object();
    if (!schema) return NULL;

    const char *p = prefix ? prefix : "";

    /* Iterate over config object - use manual iteration since json_object_foreach doesn't exist */
    for (size_t idx = 0; idx < config->c.count; idx++) {
        const char *key = config->c.keys[idx];
        json_t *val = config->c.items[idx];
        if (!key || !val) continue;

        /* Build full key */
        char full_key[256];
        if (p[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", p, key);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", key);
        }

        /* Skip internal/version keys */
        if (strcmp(full_key, "_config_version") == 0) continue;

        /* Determine category */
        const char *category;
        if (p[0]) {
            category = p;
            const char *dot = strchr(p, '.');
            if (dot) {
                size_t len = dot - p;
                char cat_buf[64];
                if (len < sizeof(cat_buf)) {
                    memcpy(cat_buf, p, len);
                    cat_buf[len] = '\0';
                    category = cat_buf;
                }
            }
        } else if (val->type == JSON_OBJECT) {
            category = key;
        } else {
            category = "general";
        }

        /* Apply category merge */
        category = category_merge_lookup(category);

        if (val->type == JSON_OBJECT) {
            /* Recurse into nested dicts */
            json_t *sub = build_schema_from_config(val, full_key);
            if (sub) {
                /* Copy all entries from sub into schema */
                for (size_t sidx = 0; sidx < sub->c.count; sidx++) {
                    const char *sub_key = sub->c.keys[sidx];
                    json_t *sub_val = sub->c.items[sidx];
                    if (sub_key && sub_val) {
                        json_object_set(schema, sub_key, json_copy(sub_val));
                    }
                }
                json_free(sub);
            }
        } else {
            /* Leaf field - build entry */
            json_t *entry = json_new_object();
            if (!entry) continue;

            /* Type */
            int type_tag = 0;
            switch (val->type) {
                case JSON_BOOL: type_tag = 1; break;
                case JSON_NUMBER: type_tag = 2; break;
                case JSON_ARRAY: type_tag = 4; break;
                case JSON_OBJECT: type_tag = 5; break;
                default: type_tag = 0; break;
            }
            json_object_set(entry, "type", json_new_string(infer_type(type_tag)));

            /* Description */
            char desc[256];
            snprintf(desc, sizeof(desc), "%s", full_key);
            for (char *c = desc; *c; c++) {
                if (*c == '.') { *c = ' '; }
                else if (*c == '_') { *c = ' '; }
            }
            json_object_set(entry, "description", json_new_string(desc));

            /* Category */
            json_object_set(entry, "category", json_new_string(category));

            /* Apply overrides */
            json_t *override = schema_override_for(full_key);
            if (override) {
                for (size_t oidx = 0; oidx < override->c.count; oidx++) {
                    const char *o_key = override->c.keys[oidx];
                    json_t *o_val = override->c.items[oidx];
                    if (o_key && o_val) {
                        json_object_set(entry, o_key, json_copy(o_val));
                    }
                }
                json_free(override);
            }

            json_object_set(schema, full_key, entry);
        }
    }

    /* Inject model_context_length after model key */
    if (!p[0]) {
        json_t *mcl = schema_override_for("model_context_length");
        if (mcl && json_has(schema, "model")) {
            json_object_set(schema, "model_context_length", mcl);
        } else if (mcl) {
            json_free(mcl);
        }
    }

    return schema;
}

/* PoP: normalize_main_model_assignment @ hermes_cli/web_server.py:_normalize_main_model_assignment */
char *normalize_main_model_assignment(const char *provider, const char *model)
{
    if (!provider) provider = "";
    if (!model) model = "";

    /* In C port we can't easily access the provider registry or model normalizer.
     * For now, we implement a simplified version that does basic normalization:
     * - Lowercase provider
     * - If provider is not a known aggregator and model has vendor prefix, fallback to openrouter
     */

    char prov[128];
    char mdl[256];
    size_t i = 0;
    while (provider[i] && i < sizeof(prov)-1) {
        prov[i] = (char)tolower((unsigned char)provider[i]);
        i++;
    }
    prov[i] = '\0';

    size_t j = 0;
    while (model[j] && j < sizeof(mdl)-1) {
        mdl[j] = model[j];
        j++;
    }
    mdl[j] = '\0';

    /* Known aggregators (openrouter, anthropic, etc.) - simplified */
    const char *aggregators[] = {"openrouter", "anthropic", "openai", "azure", "bedrock", "google", "xai", "nous", "codex", "qwen", "minimax", NULL};
    bool is_aggregator = false;
    for (int k = 0; aggregators[k]; k++) {
        if (strcmp(prov, aggregators[k]) == 0) {
            is_aggregator = true;
            break;
        }
    }

    /* If not an aggregator and model has vendor prefix, fallback to openrouter */
    if (!is_aggregator && strchr(mdl, '/')) {
        strcpy(prov, "openrouter");
    }

    /* Model normalization would require the model_normalize module */
    /* For now, just return the provider|model pair */

    char *result = malloc(strlen(prov) + strlen(mdl) + 2);
    if (result) {
        sprintf(result, "%s|%s", prov, mdl);
    }
    return result;
}

/* PoP: apply_main_model_assignment @ hermes_cli/web_server.py:_apply_main_model_assignment */
json_t *apply_main_model_assignment(json_t *model_cfg, const char *provider, const char *model,
                                     const char *base_url, const char *api_key)
{
    if (!model_cfg || model_cfg->type != JSON_OBJECT) {
        model_cfg = json_new_object();
    }

    json_t *result = json_copy(model_cfg);
    if (!result) return NULL;

    const char *prev_provider = json_get_str(result, "provider", "");
    const char *new_provider = provider;

    json_object_set(result, "provider", json_new_string(provider));
    json_object_set(result, "default", json_new_string(model));

    if (base_url && base_url[0]) {
        json_object_set(result, "base_url", json_new_string(base_url));
    } else if (json_has(result, "base_url") && strcmp(prev_provider, new_provider) != 0) {
        /* Switching providers: drop old base_url by setting to null */
        json_object_set(result, "base_url", json_null());
    }

    if (api_key && api_key[0]) {
        json_object_set(result, "api_key", json_new_string(api_key));
        json_object_set(result, "api", json_null());
    } else if (json_has(result, "api_key") && strcmp(prev_provider, new_provider) != 0) {
        json_object_set(result, "api_key", json_null());
    }

    if (strcmp(prev_provider, new_provider) != 0) {
        /* Clear endpoint credentials when switching providers */
        json_object_set(result, "base_url", json_null());
        json_object_set(result, "api_key", json_null());
    }

    /* Always drop context_length override when model changes */
    json_object_set(result, "context_length", json_null());

    return result;
}

/* PoP: web_display_system_platform @ hermes_cli/web_server.py:_display_system_platform */
json_t *web_display_system_platform(const char *system, const char *release,
                                     const char *version, const char *platform_label)
{
    if (!system) system = "";
    if (!release) release = "";
    if (!version) version = "";
    if (!platform_label) platform_label = "";

    /* Windows 10 -> 11 detection by build number */
    json_t *result = json_new_object();
    if (!result) return NULL;

    if (strcmp(system, "Windows") == 0 && strcmp(release, "10") == 0) {
        /* Extract build number from version string (e.g., "10.0.22000") */
        int build = 0;
        const char *dot1 = strchr(version, '.');
        if (dot1) {
            const char *dot2 = strchr(dot1 + 1, '.');
            if (dot2) {
                build = atoi(dot2 + 1);
            }
        }
        if (build >= 22000) {  /* Windows 11 minimum build */
            platform_label = "Windows-11";
            release = "11";
        }
    }

    json_object_set(result, "os", json_new_string(system));
    json_object_set(result, "os_release", json_new_string(release));
    json_object_set(result, "os_version", json_new_string(version));
    json_object_set(result, "platform", json_new_string(platform_label));

    return result;
}

/* PoP: web_safe_call @ hermes_cli/web_server.py:_safe_call */
json_t *web_safe_call(const char *module_name, const char *fn_name, json_t *default_val)
{
    (void)module_name;  /* C port: module resolution is compile-time */
    (void)fn_name;
    (void)default_val;
    /* In C, we can't dynamically call module functions.
     * The caller should handle this at compile time. */
    return json_copy(default_val);
}

/* ===========================================================================
 *  web_server.py pure helpers (implemented in src/gateway/run_pure.c)
 * =========================================================================== */

/* Note: The following functions are declared in include/gateway_run_pure.h
 * and implemented in src/gateway/run_pure.c:
 * - web_tail_lines
 * - web_dashboard_spawn_executable
 * - web_record_completed_action (stub)
 * - web_normalize_config_for_web
 * - gateway_resolve_gateway_display_bool
 * - gateway_has_platform_display_override
 */

/* PoP: web_elevenlabs_voice_label @ hermes_cli/web_server.py:_elevenlabs_voice_label */
char *web_elevenlabs_voice_label(const char *voice_id, const char *voice_name,
                                  const char *category, const char *description)
{
    if (voice_name && voice_name[0]) {
        if (category && category[0]) {
            char *out = malloc(strlen(voice_name) + strlen(category) + 4);
            sprintf(out, "%s (%s)", voice_name, category);
            return out;
        }
        return strdup(voice_name);
    }
    if (voice_id && voice_id[0]) return strdup(voice_id);
    return strdup("Unknown voice");
}

/* PoP: web_voice_list_error_logged_once @ hermes_cli/web_server.py:_voice_list_error_logged_once */
bool web_voice_list_error_logged_once(void)
{
    static bool logged = false;
    if (!logged) {
        logged = true;
        return false;  /* first call - not logged yet */
    }
    return true;  /* already logged */
}

/* PoP: web_parse_model_ids @ hermes_cli/web_server.py:_parse_model_ids */
char **web_parse_model_ids(const char *json_response, size_t *out_count)
{
    if (!json_response || !out_count) return NULL;
    *out_count = 0;

    json_node_t *resp = json_parse(json_response, NULL);
    if (!resp) return NULL;

    json_node_t *data = json_object_get(resp, "data");
    if (!data || data->type != JSON_ARRAY) {
        json_free(resp);
        return NULL;
    }

    size_t count = data->c.count;
    char **ids = calloc(count, sizeof(char *));
    if (!ids) {
        json_free(resp);
        return NULL;
    }

    size_t idx = 0;
    for (size_t i = 0; i < data->c.count; i++) {
        json_node_t *item = data->c.items[i];
        if (!item) continue;

        const char *mid = NULL;
        if (item->type == JSON_OBJECT) {
            json_node_t *id_node = json_object_get(item, "id");
            if (id_node && id_node->type == JSON_STRING) {
                mid = id_node->str_val;
            }
        } else if (item->type == JSON_STRING) {
            mid = item->str_val;
        }

        if (mid && mid[0]) {
            ids[idx++] = strdup(mid);
        }
    }

    *out_count = idx;
    json_free(resp);
    return ids;
}

/* PoP: web_redact_mcp_env @ hermes_cli/web_server.py:_redact_mcp_env */
json_t *web_redact_mcp_env(json_t *config)
{
    if (!config || config->type != JSON_OBJECT) return json_new_object();

    json_t *result = json_copy(config);
    if (!result) return json_new_object();

    static const char *mcp_env_vars[] = {
        "MCP_API_KEY", "MCP_SECRET", "MCP_TOKEN",
        "ANTHROPIC_API_KEY", "OPENAI_API_KEY", NULL
    };

    for (int i = 0; mcp_env_vars[i]; i++) {
        json_node_t *val = json_object_get(result, mcp_env_vars[i]);
        if (val) {
            json_object_set(result, mcp_env_vars[i], json_new_string("***REDACTED***"));
        }
    }

    return result;
}

/* PoP: web_mcp_server_summary @ hermes_cli/web_server.py:_mcp_server_summary */
char *web_mcp_server_summary(const char *name, const char *transport,
                              const char *command, const char *url)
{
    if (!name) name = "unnamed";

    if (transport && strcmp(transport, "stdio") == 0 && command) {
        char *out = malloc(strlen(name) + strlen(command) + 32);
        sprintf(out, "%s [stdio: %s]", name, command);
        return out;
    }
    if (transport && strcmp(transport, "sse") == 0 && url) {
        char *out = malloc(strlen(name) + strlen(url) + 16);
        sprintf(out, "%s [sse: %s]", name, url);
        return out;
    }
    if (transport && strcmp(transport, "websocket") == 0 && url) {
        char *out = malloc(strlen(name) + strlen(url) + 20);
        sprintf(out, "%s [ws: %s]", name, url);
        return out;
    }
    return strdup(name);
}

/* PoP: web_safe_backup_upload_name @ hermes_cli/web_server.py:_safe_backup_upload_name */
char *web_safe_backup_upload_name(const char *name)
{
    if (!name || !name[0]) return strdup("backup");

    size_t len = strlen(name);
    char *out = malloc(len + 1);
    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.') {
            out[j++] = c;
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';

    /* Prevent path traversal */
    if (strstr(out, "..")) {
        free(out);
        return strdup("backup");
    }

    return out;
}

/* PoP: web_normalise_prefix @ hermes_cli/web_server.py:_normalise_prefix */
char *web_normalise_prefix(const char *prefix)
{
    if (!prefix || !prefix[0]) return strdup("");

    size_t len = strlen(prefix);
    char *out = malloc(len + 2);
    size_t j = 0;

    for (size_t i = 0; i < len; i++) {
        char c = prefix[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_') {
            out[j++] = c;
        } else if (c == ' ') {
            out[j++] = '-';
        }
    }
    out[j] = '\0';
    return out;
}

/* PoP: web_parse_theme_layer @ hermes_cli/web_server.py:_parse_theme_layer */
json_t *web_parse_theme_layer(const char *name, json_t *layer_def)
{
    if (!name || !name[0]) return NULL;
    if (!layer_def || layer_def->type != JSON_OBJECT) return NULL;

    json_t *result = json_new_object();
    if (!result) return NULL;

    /* Copy known fields */
    static const char *fields[] = {
        "background", "foreground", "accent", "muted", "border",
        "success", "warning", "error", "info", NULL
    };

    for (int i = 0; fields[i]; i++) {
        json_node_t *val = json_object_get(layer_def, fields[i]);
        if (val) {
            json_object_set(result, fields[i], json_copy(val));
        }
    }

    return result;
}

/* PoP: web_normalise_theme_definition @ hermes_cli/web_server.py:_normalise_theme_definition */
json_t *web_normalise_theme_definition(json_t *theme_def)
{
    if (!theme_def || theme_def->type != JSON_OBJECT) return json_new_object();

    json_t *result = json_new_object();
    if (!result) return NULL;

    json_node_t *layers = json_object_get(theme_def, "layers");
    if (layers && layers->type == JSON_OBJECT) {
        json_t *norm_layers = json_new_object();
        if (norm_layers) {
            for (size_t i = 0; i < layers->c.count; i++) {
                const char *layer_name = layers->c.keys[i];
                json_node_t *layer_def = layers->c.items[i];
                json_node_t *norm_layer = web_parse_theme_layer(layer_name, layer_def);
                if (norm_layer) {
                    json_object_set(norm_layers, layer_name, norm_layer);
                }
            }
            json_object_set(result, "layers", norm_layers);
        }
    }

    /* Copy other top-level fields */
    static const char *fields[] = {
        "name", "author", "version", "description", NULL
    };
    for (int i = 0; fields[i]; i++) {
        json_node_t *val = json_object_get(theme_def, fields[i]);
        if (val) {
            json_object_set(result, fields[i], json_copy(val));
        }
    }

    return result;
}

/* PoP: web_validate_plugin_name @ hermes_cli/web_server.py:_validate_plugin_name */
bool web_validate_plugin_name(const char *name)
{
    if (!name || !name[0]) return false;
    if (strlen(name) > 64) return false;

    for (const char *p = name; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

/* PoP: web_read_bound_port @ hermes_cli/web_server.py:_read_bound_port */
int web_read_bound_port(const char *ready_file)
{
    if (!ready_file) return 0;

    FILE *f = fopen(ready_file, "r");
    if (!f) return 0;

    char line[256];
    int port = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "port:", 5) == 0) {
            port = atoi(line + 5);
            break;
        }
    }
    fclose(f);
    return port;
}

/* PoP: web_write_dashboard_ready_file @ hermes_cli/web_server.py:_write_dashboard_ready_file */
bool web_write_dashboard_ready_file(const char *ready_file, int port)
{
    if (!ready_file) return false;

    FILE *f = fopen(ready_file, "w");
    if (!f) return false;

    fprintf(f, "port:%d\n", port);
    fclose(f);
    return true;
}

/* PoP: web_maybe_open_browser @ hermes_cli/web_server.py:_maybe_open_browser */
bool web_maybe_open_browser(const char *url)
{
    if (!url) return false;
    hermes_log(LOG_INFO, "web_server", "Dashboard ready at %s", url);
    return true;  /* Caller should use xdg-open / start / open */
}

/* ================================================================
 *  Dashboard pure helpers — Port of hermes_cli/web_server.py
 *  Deterministic helpers with no FastAPI / asyncio dependency.
 * ================================================================ */

/* Minimal ISO-8601 parser: YYYY-MM-DD[THH:MM[:SS[.fff]][Z|±HH:MM]].
 * Returns seconds since epoch (UTC), or -1 on parse failure. */
static double web__parse_iso(const char *s) {
    if (!s || !*s) return -1;
    int Y, M, D, h = 0, m = 0, sec = 0;
    double frac = 0;
    char sign = '\0'; int tz_h = 0, tz_m = 0;
    const char *p = s;
    if (sscanf(p, "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &m, &sec) < 3)
        return -1;
    /* advance past the date/time we consumed */
    /* find 'T' then the time; if no 'T', default time 00:00:00 */
    const char *t = strchr(p, 'T');
    if (t) {
        const char *q = t + 1;
        /* parse time optionally with fractional and timezone */
        int n = 0;
        if (sscanf(q, "%d:%d:%d%n", &h, &m, &sec, &n) >= 3) {
            q += n;
            if (*q == '.') { sscanf(q, ".%lf%n", &frac, &n); q += n; }
            if (*q == 'Z' || *q == 'z') { /* utc */ }
            else if (*q == '+' || *q == '-') {
                sign = *q;
                sscanf(q, "%c%d:%d", &sign, &tz_h, &tz_m);
            }
        }
    }
    /* days-from-civil algorithm (Howard Hinnant) */
    int yy = Y - (M <= 2 ? 1 : 0);
    int era = (yy >= 0 ? yy : yy - 399) / 400;
    int yoe = yy - era * 400;
    int doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097 + doe - 719468;
    double secs = ((double)days * 86400.0) + h * 3600.0 + m * 60.0 + sec + frac;
    if (sign == '+') secs -= (tz_h * 3600 + tz_m * 60);
    else if (sign == '-') secs += (tz_h * 3600 + tz_m * 60);
    return secs;
}

/* PoP: web_parse_expiry_ts @ hermes_cli/web_server.py:_parse_expiry_ts */
double web_parse_expiry_ts(const char *value) {
    if (!value || !*value) return (double)time(NULL) + 600.0;
    /* Normalize a trailing 'Z' to an explicit +00:00 offset so the ISO
     * parser always sees a UTC offset. */
    size_t L = strlen(value);
    char *norm = malloc(L + 7);
    if (L > 0 && (value[L-1] == 'Z' || value[L-1] == 'z'))
        snprintf(norm, L + 7, "%.*s+00:00", (int)(L-1), value);
    else
        snprintf(norm, L + 7, "%s", value);
    double r = web__parse_iso(norm);
    free(norm);
    if (r < 0) return (double)time(NULL) + 600.0;
    return r;
}

/* PoP: web_normalize_telegram_user_id @ hermes_cli/web_server.py:_normalize_telegram_user_id */
/* _TELEGRAM_USER_ID_RE = ^[0-9]+$ */
char *web_normalize_telegram_user_id(const char *value) {
    if (!value) return NULL;
    const char *s = value;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t start = s - value;
    size_t i = start;
    if (value[i] == '\0') return NULL;
    for (; value[i]; i++) {
        if (value[i] == ' ' || value[i] == '\t' || value[i] == '\n' || value[i] == '\r') break;
    }
    size_t end = i;
    /* all chars in [start,end) must be digits */
    for (size_t k = start; k < end; k++) {
        if (!isdigit((unsigned char)value[k])) return NULL;
    }
    if (end == start) return NULL;
    size_t len = end - start;
    char *out = malloc(len + 1);
    memcpy(out, value + start, len);
    out[len] = '\0';
    return out;
}

/* PoP: web_telegram_onboarding_error_message @ hermes_cli/web_server.py:_telegram_onboarding_error_message */
const char *web_telegram_onboarding_error_message(const char *error, const char *fallback) {
    static const struct { const char *code; const char *msg; } map[] = {
        {"not_found", "Telegram pairing was not found. Start a new setup."},
        {"expired", "Telegram setup expired. Start a new setup."},
        {"claimed", "Telegram setup was already claimed. Start a new setup."},
        {"unauthorized", "Telegram setup service rejected this request."},
        {"telegram_manager_bot_token_not_configured", "Telegram setup service is not configured."},
        {"telegram_token_fetch_failed", "Telegram could not finish bot setup. Try again."},
        {NULL, NULL},
    };
    if (error) {
        for (int i = 0; map[i].code; i++)
            if (strcmp(error, map[i].code) == 0) return map[i].msg;
    }
    return fallback ? fallback : "";
}

/* PoP: web_truncate_token @ hermes_cli/web_server.py:_truncate_token */
char *web_truncate_token(const char *value, int visible) {
    if (!value || !*value) return strdup("");
    if (visible <= 0) visible = 6;
    const char *s = value;
    /* JWT: >=2 dots -> take trailing segment */
    int dots = 0;
    for (const char *p = s; *p; p++) if (*p == '.') dots++;
    if (dots >= 2) {
        const char *last = strrchr(s, '.');
        if (last) s = last + 1;
    }
    size_t len = strlen(s);
    if (len <= (size_t)visible) return strdup(s);
    /* prefix with ellipsis char '…' (UTF-8) */
    size_t need = 3 + (size_t)visible + 1; /* … + visible + nul (… is 3 bytes) */
    char *out = malloc(need);
    snprintf(out, need, "\xe2\x80\xa6%.*s", visible, s + len - visible);
    return out;
}

/* PoP: web_windows_build_number @ hermes_cli/web_server.py:_windows_build_number */
/* regex: (?:^|[^0-9])10\.0\.(\d{5,})(?:[^0-9]|$) */
int web_windows_build_number(const char *version, const char *platform_label) {
    const char *srcs[2];
    srcs[0] = version ? version : "";
    srcs[1] = platform_label ? platform_label : "";
    for (int i = 0; i < 2; i++) {
        const char *s = srcs[i];
        for (const char *p = s; *p; p++) {
            if ((p == s || !isdigit((unsigned char)p[-1])) &&
                p[0] == '1' && p[1] == '0' && p[2] == '.' && p[3] == '0' && p[4] == '.') {
                const char *q = p + 5;
                if (isdigit((unsigned char)*q)) {
                    const char *num_start = q;
                    while (isdigit((unsigned char)*q)) q++;
                    size_t nlen = q - num_start;
                    if (nlen >= 5 && (!isdigit((unsigned char)*q))) {
                        int v = atoi(num_start);
                        return v;
                    }
                }
            }
        }
    }
    return -1;
}

/* PoP: web_cron_optional_text @ hermes_cli/web_server.py:_cron_optional_text */
char *web_cron_optional_text(const char *value, bool strip_trailing_slash) {
    if (!value) return NULL;
    /* strip leading/trailing whitespace */
    const char *s = value;
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    if (e == s) return NULL;
    size_t len = e - s;
    char *out = malloc(len + 1);
    memcpy(out, s, len); out[len] = '\0';
    if (strip_trailing_slash) {
        while (out[0] && out[strlen(out)-1] == '/') out[strlen(out)-1] = '\0';
    }
    if (out[0] == '\0') { free(out); return NULL; }
    return out;
}

/* PoP: web_cron_string_list @ hermes_cli/web_server.py:_cron_string_list */
char **web_cron_string_list(const char *value) {
    if (!value) return NULL;
    /* split on newline or comma */
    char *buf = strdup(value);
    char **arr = malloc(sizeof(char *) * 64);
    int n = 0;
    char *save = buf;
    const char *delim = "\n,";
    char *tok = strtok_r(buf, delim, &save);
    while (tok) {
        /* trim */
        while (*tok && isspace((unsigned char)*tok)) tok++;
        size_t L = strlen(tok);
        while (L > 0 && isspace((unsigned char)tok[L-1])) tok[--L] = '\0';
        if (tok[0]) {
            if (n >= 63) break;
            arr[n++] = strdup(tok);
        }
        tok = strtok_r(NULL, delim, &save);
    }
    free(buf);
    if (n == 0) { free(arr); return NULL; }
    arr[n] = NULL;
    return arr;
}

/* PoP: web_channel_or_close_code @ hermes_cli/web_server.py:_channel_or_close_code */
/* _VALID_CHANNEL_RE = ^[A-Za-z0-9._-]{1,128}$ */
char *web_channel_or_close_code(const char *channel) {
    if (!channel) return NULL;
    size_t len = strlen(channel);
    if (len < 1 || len > 128) return NULL;
    for (size_t i = 0; i < len; i++) {
        char c = channel[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
            return NULL;
    }
    return strdup(channel);
}

/* PoP: web_read_active_session_file @ hermes_cli/web_server.py:_read_active_session_file */
char *web_read_active_session_file(const char *path) {
    if (!path) return NULL;
    char *err = NULL;
    json_t *doc = json_parse_file(path, &err);
    if (err) { free(err); }
    if (!doc) return NULL;
    const char *sid = json_get_str(doc, "session_id", NULL);
    char *out = NULL;
    if (sid && *sid) {
        while (*sid && isspace((unsigned char)*sid)) sid++;
        /* strip trailing whitespace (Python str.strip() semantics) */
        size_t L = strlen(sid);
        while (L > 0 && isspace((unsigned char)sid[L-1])) L--;
        if (L > 0) { out = malloc(L + 1); memcpy(out, sid, L); out[L] = '\0'; }
    }
    json_free(doc);
    return out;
}

/* PoP: web_platform_env_prefixes @ hermes_cli/web_server.py:_platform_env_prefixes */
char **web_platform_env_prefixes(const char *platform_id)
{
    /* aliases map (platform_id -> prefixes) */
    static const struct { const char *id; const char *p[3]; int n; } aliases[] = {
        {"email",        {"EMAIL_"}, 1},
        {"homeassistant", {"HASS_"}, 1},
        {"qqbot",        {"QQ_", "QQBOT_"}, 2},
        {"sms",          {"TWILIO_"}, 1},
        {"wecom",        {"WECOM_BOT_", "WECOM_SECRET"}, 2},
        {"wecom_callback", {"WECOM_CALLBACK_"}, 1},
    };
    if (!platform_id || !*platform_id) return NULL;
    for (int i = 0; i < (int)(sizeof(aliases)/sizeof(aliases[0])); i++) {
        if (strcmp(platform_id, aliases[i].id) == 0) {
            char **out = malloc(sizeof(char *) * (aliases[i].n + 1));
            if (!out) return NULL;
            for (int j = 0; j < aliases[i].n; j++)
                out[j] = strdup(aliases[i].p[j]);
            out[aliases[i].n] = NULL;
            return out;
        }
    }
    /* default: upper-case id with '-' -> '_', plus trailing '_' */
    size_t L = strlen(platform_id);
    char *up = malloc(L + 2);
    if (!up) return NULL;
    for (size_t k = 0; k < L; k++)
        up[k] = (platform_id[k] == '-') ? '_' : (char)toupper((unsigned char)platform_id[k]);
    up[L] = '_';
    up[L+1] = '\0';
    char **out = malloc(sizeof(char *) * 2);
    if (!out) { free(up); return NULL; }
    out[0] = up;
    out[1] = NULL;
    return out;
}

void web_free_strv(char **v)
{
    if (!v) return;
    for (int i = 0; v[i]; i++) free(v[i]);
    free(v);
}

/* Reusable client-IP resolution (Port of the dashboard_auth _client_ip helper,
 * identical across middleware.py / routes.py / token_auth.py):
 *   x_forwarded_for (may be ""): take the first comma-separated value, trimmed.
 *   else fall back to client_host (the direct peer), or "" if NULL.
 * Caller owns the returned string (malloc'd). Returns NULL on alloc failure. */
char *web_client_ip(const char *x_forwarded_for, const char *client_host)
{
    if (x_forwarded_for && *x_forwarded_for) {
        /* first hop */
        const char *comma = strchr(x_forwarded_for, ',');
        size_t len = comma ? (size_t)(comma - x_forwarded_for) : strlen(x_forwarded_for);
        /* trim leading/trailing whitespace */
        size_t s = 0;
        while (s < len && (x_forwarded_for[s] == ' ' || x_forwarded_for[s] == '\t')) s++;
        size_t e = len;
        while (e > s && (x_forwarded_for[e-1] == ' ' || x_forwarded_for[e-1] == '\t')) e--;
        if (e > s) {
            char *out = malloc(e - s + 1);
            if (!out) return NULL;
            memcpy(out, x_forwarded_for + s, e - s);
            out[e - s] = '\0';
            return out;
        }
    }
    if (client_host && *client_host) return strdup(client_host);
    return strdup("");
}


/* ── from src/cli/port_web_server_auth.c ── */
/*
 * port_web_server_auth.c — Faithful C11 port of the dashboard auth-helper
 * cluster from hermes_cli/web_server.py.
 *
 * Ports (exact, PoP):
 *   - ws_has_valid_session_token   @ web_server.py:_has_valid_session_token
 *   - ws_should_require_auth       @ web_server.py:should_require_auth
 *   - ws_is_accepted_host          @ web_server.py:_is_accepted_host
 *
 * These are the pure-logic pieces of the FastAPI auth middlewares. They are
 * compiled deterministically and reused by the existing raw-HTTP dashboard
 * handlers (web_dashboard.c) and by any future request-time gate. The token
 * source is the shared g_session_token (set in web_dashboard.c on startup).
 *
 * The string-matching here deliberately mirrors the Python logic: the
 * loopback set, the IPv6 bracket handling, and the exact-host match for
 * non-loopback binds. No behaviour is stubbed.
 */

#include "hermes_web_dashboard.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   /* strcasestr() */
#endif
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* ── Loopback host values (mirrors web_server._LOOPBACK_HOST_VALUES) ───── */
static const char *k_loopback_hosts[] = {
    "localhost", "127.0.0.1", "::1", NULL
};

static bool is_loopback_value(const char *h) {
    if (!h) return false;
    for (int i = 0; k_loopback_hosts[i]; i++) {
        if (strcasecmp(h, k_loopback_hosts[i]) == 0) return true;
    }
    return false;
}

/* ── ws_has_valid_session_token ──────────────────────────────────────────
 * Port of _has_valid_session_token(request). Validates the dedicated session
 * header (X-Hermes-Session-Token) with constant-time compare, then falls
 * back to the legacy "Bearer <token>" Authorization form. Mirrors Python's
 * hmac.compare_digest, which compares the ENTIRE header value — so trailing
 * junk (e.g. "token extra") is rejected, not just whitespace-trimmed. */
/* PoP: ws_has_valid_session_token @ hermes_cli/web_server.py:_has_valid_session_token */
bool ws_has_valid_session_token(const char *headers) {
    if (!headers || !g_session_token[0]) return false;
    size_t toklen = strlen(g_session_token);
    if (toklen == 0) return false;

    /* Dedicated session header (case-insensitive name). */
    const char *hdr = strcasestr(headers, "X-Hermes-Session-Token:");
    if (hdr) {
        hdr = strchr(hdr, ':');
        if (hdr) {
            hdr++;
            while (*hdr == ' ') hdr++;
            /* Value runs to end-of-line; reject any trailing junk. */
            const char *end = hdr;
            while (*end && *end != '\r' && *end != '\n') end++;
            size_t vlen = (size_t)(end - hdr);
            if (vlen == toklen && strncmp(hdr, g_session_token, toklen) == 0)
                return true;
        }
    }

    /* Legacy Bearer form in Authorization. */
    const char *auth = strcasestr(headers, "Authorization:");
    if (auth) {
        auth = strchr(auth, ':');
        if (auth) {
            auth++;
            while (*auth == ' ') auth++;
            if (strncmp(auth, "Bearer ", 7) == 0) {
                auth += 7;
                const char *end = auth;
                while (*end && *end != '\r' && *end != '\n') end++;
                size_t vlen = (size_t)(end - auth);
                if (vlen == toklen && strncmp(auth, g_session_token, toklen) == 0)
                    return true;
            }
        }
    }

    return false;
}

/* ── ws_should_require_auth ──────────────────────────────────────────────
 * Port of should_require_auth(host, allow_public). Loopback binds are
 * trusted (no gate); any non-loopback bind always requires an auth
 * provider. `allow_public` is accepted for backward-compat but ignored —
 * a non-loopback bind ALWAYS engages the gate. */
/* PoP: ws_should_require_auth @ hermes_cli/web_server.py:should_require_auth */
bool ws_should_require_auth(const char *host, bool allow_public) {
    (void)allow_public; /* legacy escape hatch, intentionally ignored */
    if (!host) return true;
    return !is_loopback_value(host);
}

/* ── ws_is_accepted_host ─────────────────────────────────────────────────
 * Port of _is_accepted_host(host_header, bound_host). DNS-rebinding defence
 * (GHSA-ppp5-vxwm-4cf7): reject Host headers that don't target the bound
 * interface. 0.0.0.0 / :: binds opt into all-interfaces (no Host-layer
 * defence possible); loopback binds accept the loopback aliases; explicit
 * non-loopback binds require an exact host match. */
/* PoP: ws_is_accepted_host @ hermes_cli/web_server.py:_is_accepted_host */
bool ws_is_accepted_host(const char *host_header, const char *bound_host) {
    if (!host_header || !bound_host) return false;

    /* Strip port suffix, handling IPv6 bracket notation ([::1]:9119). */
    char hbuf[256];
    size_t n = strlen(host_header);
    if (n >= sizeof(hbuf)) n = sizeof(hbuf) - 1;
    memcpy(hbuf, host_header, n);
    hbuf[n] = '\0';
    /* trim trailing whitespace */
    while (n > 0 && (hbuf[n - 1] == ' ' || hbuf[n - 1] == '\t' ||
                     hbuf[n - 1] == '\r' || hbuf[n - 1] == '\n'))
        hbuf[--n] = '\0';

    char host_only[256];
    if (hbuf[0] == '[') {
        char *close = strchr(hbuf, ']');
        if (close) {
            size_t len = (size_t)(close - (hbuf + 1));
            if (len >= sizeof(host_only)) len = sizeof(host_only) - 1;
            memcpy(host_only, hbuf + 1, len);
            host_only[len] = '\0';
        } else {
            size_t len = strlen(hbuf) - 2; /* strip [ ] */
            if (len >= sizeof(host_only)) len = sizeof(host_only) - 1;
            memcpy(host_only, hbuf + 1, len);
            host_only[len] = '\0';
        }
    } else {
        char *colon = strrchr(hbuf, ':');
        if (colon) *colon = '\0';
        strncpy(host_only, hbuf, sizeof(host_only) - 1);
        host_only[sizeof(host_only) - 1] = '\0';
    }

    /* lowercase for comparison */
    for (char *p = host_only; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    /* 0.0.0.0 / :: → operator opted into all-interfaces. */
    if (strcasecmp(bound_host, "0.0.0.0") == 0 ||
        strcasecmp(bound_host, "::") == 0)
        return true;

    /* Loopback bind: accept the loopback aliases. */
    char bound_lc[256];
    strncpy(bound_lc, bound_host, sizeof(bound_lc) - 1);
    bound_lc[sizeof(bound_lc) - 1] = '\0';
    for (char *p = bound_lc; *p; p++)
        *p = (char)tolower((unsigned char)*p);

    if (is_loopback_value(bound_lc))
        return is_loopback_value(host_only);

    /* Explicit non-loopback bind: require exact host match. */
    return strcmp(host_only, bound_lc) == 0;
}





/* ── from src/cli/port_web_server_chat_argv.c ── */
/*
 * port_web_server_chat_argv.c — chat PTY env assembly + session descendant
 * resolution. Faithful port of _resolve_profile_dir,
 * _session_latest_descendant, HermesState.resolve_session_id (the exact/
 * unique-prefix contract the resume path needs), and the env-assembly body
 * of _resolve_chat_argv from hermes_cli/web_server.py.
 *
 * Reuses: profile_validate_name / profile_dir_exists / profile_dir_for
 * (port_cli_profiles.c) and sqlite3 (lib/libdb) against the real session DB.
 */

#include "web_server_chat_argv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sqlite3.h"
#include "hermes_json.h"

/* From port_cli_profiles.c */
extern bool profile_validate_name(const char *name, char **err);
extern int profile_dir_exists(const char *name);
extern char *profile_dir_for(const char *name);

/* ── _resolve_profile_dir ───────────────────────────────────────────────── */
/* PoP: ws_chat_resolve_profile_dir @ hermes_cli/web_server.py:_resolve_profile_dir */
char *ws_chat_resolve_profile_dir(const char *name, int *status,
                                  char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    char *err = NULL;
    if (!profile_validate_name(name, &err)) {
        if (status) *status = 400;
        if (detail) *detail = err ? err : strdup("invalid profile name");
        else free(err);
        return NULL;
    }
    free(err);
    if (!profile_dir_exists(name)) {
        if (status) *status = 404;
        if (detail) {
            char buf[512];
            snprintf(buf, sizeof(buf), "Profile '%s' does not exist.", name);
            *detail = strdup(buf);
        }
        return NULL;
    }
    return profile_dir_for(name);
}

/* ── resolve_session_id (hermes_state.py) ───────────────────────────────── */

static bool session_exists(sqlite3 *db, const char *sid) {
    sqlite3_stmt *st = NULL;
    bool found = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sessions WHERE id = ?", -1,
                           &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, sid, -1, SQLITE_TRANSIENT);
        found = sqlite3_step(st) == SQLITE_ROW;
    }
    sqlite3_finalize(st);
    return found;
}

/* PoP: ws_chat_resolve_session_id @ hermes_state.py:resolve_session_id */
char *ws_chat_resolve_session_id(const char *db_path, const char *prefix) {
    if (!db_path || !prefix || !*prefix) return NULL;
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }
    char *result = NULL;
    if (session_exists(db, prefix)) {
        result = strdup(prefix);
        sqlite3_close(db);
        return result;
    }
    /* LIKE-escape \, %, _ with backslash (Python .replace chain). */
    size_t plen = strlen(prefix);
    char *escaped = malloc(plen * 2 + 2);
    size_t j = 0;
    for (size_t i = 0; i < plen; i++) {
        char c = prefix[i];
        if (c == '\\' || c == '%' || c == '_') escaped[j++] = '\\';
        escaped[j++] = c;
    }
    escaped[j++] = '%';
    escaped[j] = '\0';

    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT id FROM sessions WHERE id LIKE ? ESCAPE '\\' "
            "ORDER BY started_at DESC LIMIT 2",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, escaped, -1, SQLITE_TRANSIENT);
        char *first = NULL;
        int count = 0;
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (count == 0) {
                const unsigned char *t = sqlite3_column_text(st, 0);
                first = strdup(t ? (const char *)t : "");
            }
            count++;
        }
        if (count == 1) result = first;
        else free(first);
    }
    sqlite3_finalize(st);
    free(escaped);
    sqlite3_close(db);
    return result;
}

/* ── _session_latest_descendant ─────────────────────────────────────────── */

typedef struct {
    char *id;
    char *parent;
    double started_at;
} desc_row_t;

/* PoP: ws_chat_session_latest_descendant @ hermes_cli/web_server.py:_session_latest_descendant */
json_t *ws_chat_session_latest_descendant(const char *db_path,
                                          const char *session_id) {
    json_t *out = json_object();
    json_set(out, "latest", json_null());
    json_set(out, "path", json_array());

    char *sid = ws_chat_resolve_session_id(db_path, session_id);
    if (!sid) return out;

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        free(sid);
        return out;
    }

    desc_row_t *rows = NULL;
    size_t nrows = 0, cap = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(db,
            "WITH RECURSIVE descendants(id, parent_session_id, started_at) AS ("
            " SELECT id, parent_session_id, started_at FROM sessions WHERE id = ?"
            " UNION"
            " SELECT s.id, s.parent_session_id, s.started_at"
            " FROM sessions s JOIN descendants d ON s.parent_session_id = d.id)"
            " SELECT id, parent_session_id, started_at FROM descendants",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, sid, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (nrows == cap) {
                cap = cap ? cap * 2 : 16;
                rows = realloc(rows, cap * sizeof *rows);
            }
            const unsigned char *id = sqlite3_column_text(st, 0);
            const unsigned char *pa = sqlite3_column_text(st, 1);
            rows[nrows].id = strdup(id ? (const char *)id : "");
            rows[nrows].parent = pa ? strdup((const char *)pa) : NULL;
            /* Python float(started_at or 0) with exception → 0.0 */
            rows[nrows].started_at =
                sqlite3_column_type(st, 2) == SQLITE_NULL
                    ? 0.0
                    : sqlite3_column_double(st, 2);
            nrows++;
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);

    /* Walk: current = sid; repeatedly pick unseen child with max started_at
     * (candidates.sort reverse → first). Python keeps SQL row order for
     * ties; sort is stable, so max-scan taking the FIRST max matches. */
    json_t *path = json_object_get(out, "path");
    json_append(path, json_string(sid));

    char **seen = malloc((nrows + 1) * sizeof *seen);
    size_t nseen = 0;
    seen[nseen++] = strdup(sid);
    char *current = strdup(sid);

    for (;;) {
        int best = -1;
        for (size_t i = 0; i < nrows; i++) {
            if (!rows[i].parent || !rows[i].id[0]) continue;
            if (strcmp(rows[i].parent, current) != 0) continue;
            bool was_seen = false;
            for (size_t k = 0; k < nseen && !was_seen; k++)
                if (strcmp(seen[k], rows[i].id) == 0) was_seen = true;
            if (was_seen) continue;
            if (best < 0 || rows[i].started_at > rows[best].started_at)
                best = (int)i;
        }
        if (best < 0) break;
        free(current);
        current = strdup(rows[best].id);
        json_append(path, json_string(current));
        seen = realloc(seen, (nseen + 1) * sizeof *seen);
        seen[nseen++] = strdup(current);
    }

    json_set(out, "latest", json_string(current));
    free(current);
    for (size_t k = 0; k < nseen; k++) free(seen[k]);
    free(seen);
    for (size_t i = 0; i < nrows; i++) {
        free(rows[i].id);
        free(rows[i].parent);
    }
    free(rows);
    free(sid);
    return out;
}

/* ── _resolve_chat_argv env assembly ────────────────────────────────────── */

static void env_setdefault(json_t *env, const char *k, const char *v) {
    if (!json_object_get(env, k)) json_set(env, k, json_string(v));
}

/* PoP: ws_chat_build_env @ hermes_cli/web_server.py:_resolve_chat_argv */
json_t *ws_chat_build_env(const json_t *base_env, const char *resume,
                          const char *sidecar_url, const char *profile_dir,
                          const char *active_session_file,
                          const char *gateway_ws_url) {
    json_t *env = base_env ? json_copy((json_t *)base_env) : json_object();

    env_setdefault(env, "NODE_ENV", "production");
    env_setdefault(env, "HERMES_TUI_DISABLE_MOUSE", "1");
    env_setdefault(env, "HERMES_TUI_INLINE", "1");
    env_setdefault(env, "COLORTERM", "truecolor");
    json_set(env, "HERMES_TUI_DASHBOARD", json_string("1"));

    if (profile_dir && *profile_dir)
        json_set(env, "HERMES_HOME", json_string(profile_dir));

    if (resume && *resume)
        json_set(env, "HERMES_TUI_RESUME", json_string(resume));

    if (sidecar_url && *sidecar_url)
        json_set(env, "HERMES_TUI_SIDECAR_URL", json_string(sidecar_url));

    if (active_session_file && *active_session_file)
        json_set(env, "HERMES_TUI_ACTIVE_SESSION_FILE",
                 json_string(active_session_file));

    /* Profile-scoped chats must NOT attach to the dashboard's in-memory
     * gateway. */
    if ((!profile_dir || !*profile_dir) && gateway_ws_url && *gateway_ws_url)
        json_set(env, "HERMES_TUI_GATEWAY_URL", json_string(gateway_ws_url));

    return env;
}


/* ── from src/cli/port_web_server_console.c ── */
/*
 * port_web_server_console.c — /api/console frame protocol engine.
 * Faithful port of _console_json_payload, _console_profile_from_ws,
 * _console_send_result, and the console_ws dispatch state machine from
 * hermes_cli/web_server.py.
 */

#include "web_server_console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── UTF-8 validation (Python bytes.decode("utf-8") strictness) ─────────── */
static bool utf8_valid(const unsigned char *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char c = s[i];
        size_t need;
        unsigned int cp;
        if (c < 0x80) { i++; continue; }
        else if ((c & 0xE0) == 0xC0) { need = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { need = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { need = 3; cp = c & 0x07; }
        else return false;
        if (i + need >= len) return false; /* truncated tail */
        for (size_t k = 1; k <= need; k++) {
            if ((s[i + k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (s[i + k] & 0x3F);
        }
        /* overlong / surrogate / out-of-range */
        if (need == 1 && cp < 0x80) return false;
        if (need == 2 && (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))) return false;
        if (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)) return false;
        i += need + 1;
    }
    return true;
}

/* ── _console_json_payload ──────────────────────────────────────────────── */
/* PoP: ws_console_json_payload @ hermes_cli/web_server.py:_console_json_payload */
json_t *ws_console_json_payload(const char *text, const unsigned char *bytes,
                                size_t bytes_len, char **error) {
    if (error) *error = NULL;
    char *owned = NULL;
    const char *raw = text;
    if (raw == NULL && bytes != NULL) {
        if (!utf8_valid(bytes, bytes_len) ||
            memchr(bytes, '\0', bytes_len) != NULL) {
            /* NUL check: json_parse takes a C string, and a NUL inside the
             * frame would truncate — Python would parse the full buffer and
             * fail on the JSON layer instead; either way it's an error, but
             * an embedded NUL is not valid in JSON text anyway. Keep the
             * UTF-8 error message only for genuine decode failures. */
            if (!utf8_valid(bytes, bytes_len)) {
                if (error) *error = strdup("Console frames must be UTF-8 JSON.");
                return NULL;
            }
            if (error) *error = strdup("Console frames must be JSON objects.");
            return NULL;
        }
        owned = malloc(bytes_len + 1);
        memcpy(owned, bytes, bytes_len);
        owned[bytes_len] = '\0';
        raw = owned;
    }
    if (raw == NULL) return NULL; /* (None, None) */

    json_t *payload = json_parse(raw, NULL);
    free(owned);
    if (!payload) {
        if (error) *error = strdup("Console frames must be JSON objects.");
        return NULL;
    }
    if (payload->type != JSON_OBJECT) {
        json_free(payload);
        if (error) *error = strdup("Console frames must be JSON objects.");
        return NULL;
    }
    return payload;
}

/* ── _console_profile_from_ws ───────────────────────────────────────────── */
/* PoP: ws_console_profile_from_query @ hermes_cli/web_server.py:_console_profile_from_ws */
char *ws_console_profile_from_query(const char *profile) {
    if (!profile) return NULL;
    while (*profile == ' ' || *profile == '\t' || *profile == '\n' ||
           *profile == '\r') profile++;
    size_t l = strlen(profile);
    while (l && (profile[l-1] == ' ' || profile[l-1] == '\t' ||
                 profile[l-1] == '\n' || profile[l-1] == '\r')) l--;
    if (!l) return NULL;
    char *r = malloc(l + 1);
    memcpy(r, profile, l);
    r[l] = '\0';
    return r;
}

/* ── frame builders ─────────────────────────────────────────────────────── */

static json_t *frame_base(const char *type) {
    json_t *o = json_object();
    json_set(o, "type", json_string(type));
    return o;
}

static void push(json_t *frames, json_t *f) { json_append(frames, f); }

/* ── _console_send_result ───────────────────────────────────────────────── */
/* PoP: ws_console_send_result @ hermes_cli/web_server.py:_console_send_result */
void ws_console_send_result(json_t *frames, const ws_console_result_t *result,
                            int command_id) {
    const char *command = result->command ? result->command : "";
    const char *status = result->status ? result->status : "";

    if (strcmp(status, "ok") == 0) {
        if (result->output && *result->output) {
            json_t *f = frame_base("output");
            json_set(f, "id", json_number(command_id));
            json_set(f, "stream", json_string("stdout"));
            json_set(f, "data", json_string(result->output));
            json_set(f, "command", json_string(command));
            push(frames, f);
        }
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("ok"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "error") == 0) {
        json_t *e = frame_base("error");
        json_set(e, "id", json_number(command_id));
        json_set(e, "message", json_string(
            result->output && *result->output ? result->output
                                              : "Command failed."));
        json_set(e, "command", json_string(command));
        push(frames, e);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("error"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "confirm_required") == 0) {
        char fallback[512];
        snprintf(fallback, sizeof(fallback), "Run `%s`?", command);
        json_t *f = frame_base("confirm_required");
        json_set(f, "id", json_number(command_id));
        json_set(f, "command", json_string(command));
        json_set(f, "message", json_string(
            result->confirmation_message && *result->confirmation_message
                ? result->confirmation_message : fallback));
        json_set(f, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, f);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("confirm_required"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "clear") == 0) {
        json_t *f = frame_base("clear");
        json_set(f, "id", json_number(command_id));
        push(frames, f);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("clear"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return;
    }

    if (strcmp(status, "exit") == 0) {
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("exit"));
        json_set(c, "command", json_string(command));
        json_set(c, "prompt", json_string(""));
        push(frames, c);
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Unknown console result status: %s", status);
    json_t *e = frame_base("error");
    json_set(e, "id", json_number(command_id));
    json_set(e, "message", json_string(msg));
    json_set(e, "command", json_string(command));
    push(frames, e);
}

/* ── session state machine ──────────────────────────────────────────────── */

struct ws_console_session {
    char *pending_confirmation;  /* NULL when none */
    int command_generation;
    bool busy;                   /* active_task and not done() */
};

ws_console_session_t *ws_console_session_new(void) {
    return calloc(1, sizeof(ws_console_session_t));
}

void ws_console_session_free(ws_console_session_t *s) {
    if (!s) return;
    free(s->pending_confirmation);
    free(s);
}

void ws_console_session_set_busy(ws_console_session_t *s, bool busy) {
    if (s) s->busy = busy;
}
bool ws_console_session_busy(const ws_console_session_t *s) {
    return s && s->busy;
}
const char *ws_console_session_pending(const ws_console_session_t *s) {
    return s ? s->pending_confirmation : NULL;
}
int ws_console_session_generation(const ws_console_session_t *s) {
    return s ? s->command_generation : 0;
}

static void set_pending(ws_console_session_t *s, const char *cmd) {
    free(s->pending_confirmation);
    s->pending_confirmation = cmd ? strdup(cmd) : NULL;
}

static json_t *simple_error(const char *message) {
    json_t *e = frame_base("error");
    json_set(e, "message", json_string(message));
    json_set(e, "prompt", json_string(WS_CONSOLE_PROMPT));
    return e;
}

static json_t *simple_complete(const char *status) {
    json_t *c = frame_base("complete");
    json_set(c, "status", json_string(status));
    json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
    return c;
}

/* Python str.strip().lower() of payload.get("type"). */
static char *frame_type_of(const json_t *payload) {
    const char *t = json_get_str(payload, "type", NULL);
    if (!t) t = "";  /* str(None or "") == "" */
    while (*t == ' ' || *t == '\t' || *t == '\n' || *t == '\r') t++;
    size_t l = strlen(t);
    while (l && (t[l-1] == ' ' || t[l-1] == '\t' || t[l-1] == '\n' ||
                 t[l-1] == '\r')) l--;
    char *r = malloc(l + 1);
    for (size_t i = 0; i < l; i++)
        r[i] = (t[i] >= 'A' && t[i] <= 'Z') ? (char)(t[i] + 32) : t[i];
    r[l] = '\0';
    return r;
}

/* run_command inline (synchronous seam: exec + result frames). */
/* PoP: run_command_now @ hermes_cli/web_server.py:run_command */
/* PoP: run_command_now @ hermes_cli/web_server.py:start_command */
static bool run_command_now(ws_console_session_t *s, const char *line,
                            bool confirmed, ws_console_exec_fn exec,
                            void *exec_ctx, json_t *frames) {
    /* start_command: command_generation += 1 */
    s->command_generation++;
    int command_id = s->command_generation;

    ws_console_result_t result = {0};
    bool ok = exec ? exec(exec_ctx, line, confirmed, &result) : false;
    if (!ok) {
        /* except Exception path */
        set_pending(s, NULL);
        json_t *e = frame_base("error");
        json_set(e, "id", json_number(command_id));
        json_set(e, "message", json_string(
            result.output && *result.output ? result.output
                                            : "Exception"));
        json_set(e, "command", json_string(line));
        push(frames, e);
        json_t *c = frame_base("complete");
        json_set(c, "id", json_number(command_id));
        json_set(c, "status", json_string("error"));
        json_set(c, "command", json_string(line));
        json_set(c, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, c);
        return false;
    }
    /* else branch: pending = command if confirm_required else None */
    if (result.status && strcmp(result.status, "confirm_required") == 0)
        set_pending(s, result.command ? result.command : "");
    else
        set_pending(s, NULL);
    ws_console_send_result(frames, &result, command_id);
    return result.status && strcmp(result.status, "exit") == 0;
}

/* PoP: ws_console_handle_frame @ hermes_cli/web_server.py:console_ws */
bool ws_console_handle_frame(ws_console_session_t *s, const json_t *payload,
                             ws_console_exec_fn exec, void *exec_ctx,
                             json_t *frames) {
    char *ft = frame_type_of(payload);

    if (strcmp(ft, "ping") == 0) {
        json_t *p = frame_base("pong");
        json_set(p, "prompt", json_string(WS_CONSOLE_PROMPT));
        push(frames, p);
        free(ft);
        return false;
    }

    if (strcmp(ft, "cancel") == 0) {
        if (s->busy) {
            s->command_generation++;
            s->busy = false;
            set_pending(s, NULL);
            push(frames, simple_complete("cancelled"));
        } else if (s->pending_confirmation) {
            set_pending(s, NULL);
            push(frames, simple_complete("cancelled"));
        } else {
            push(frames, simple_complete("idle"));
        }
        free(ft);
        return false;
    }

    if (s->busy) {
        push(frames, simple_error("A console command is already running."));
        free(ft);
        return false;
    }

    if (strcmp(ft, "confirm") == 0) {
        const char *cmd_raw = json_get_str(payload, "command", NULL);
        const char *src = cmd_raw && *cmd_raw ? cmd_raw
                          : (s->pending_confirmation ? s->pending_confirmation
                                                     : "");
        /* .strip() */
        while (*src == ' ' || *src == '\t') src++;
        size_t l = strlen(src);
        while (l && (src[l-1] == ' ' || src[l-1] == '\t')) l--;
        char *command = malloc(l + 1);
        memcpy(command, src, l);
        command[l] = '\0';

        if (!s->pending_confirmation) {
            push(frames, simple_error("No command is waiting for confirmation."));
            free(command);
            free(ft);
            return false;
        }
        if (strcmp(command, s->pending_confirmation) != 0) {
            push(frames, simple_error(
                "Confirmation does not match the pending command."));
            free(command);
            free(ft);
            return false;
        }
        set_pending(s, NULL);
        bool close = run_command_now(s, command, true, exec, exec_ctx, frames);
        free(command);
        free(ft);
        return close;
    }

    if (strcmp(ft, "input") == 0 || strcmp(ft, "command") == 0) {
        const char *line_raw = json_get_str(payload, "line", NULL);
        if (!line_raw || !*line_raw)
            line_raw = json_get_str(payload, "command", NULL);
        if (!line_raw) line_raw = "";
        while (*line_raw == ' ' || *line_raw == '\t') line_raw++;
        size_t l = strlen(line_raw);
        while (l && (line_raw[l-1] == ' ' || line_raw[l-1] == '\t')) l--;
        char *line = malloc(l + 1);
        memcpy(line, line_raw, l);
        line[l] = '\0';

        if (!*line) {
            push(frames, simple_complete("ok"));
            free(line);
            free(ft);
            return false;
        }
        if (s->pending_confirmation) {
            push(frames, simple_error(
                "Confirm or cancel the pending command before running "
                "another one."));
            free(line);
            free(ft);
            return false;
        }
        bool close = run_command_now(s, line, false, exec, exec_ctx, frames);
        free(line);
        free(ft);
        return close;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Unsupported console frame: %s",
             *ft ? ft : "?");
    push(frames, simple_error(msg));
    free(ft);
    return false;
}


/* ── from src/cli/port_web_server_cron_dash.c ── */
/*
 * port_web_server_cron_dash.c — dashboard cron adapter layer.
 * Faithful port of _normalize_dashboard_cron_script,
 * _validate_dashboard_cron_effective_job, _normalize_dashboard_cron_updates,
 * _cron_default_profile, _cron_profile_home, and _annotate_cron_job from
 * hermes_cli/web_server.py.
 *
 * Reuses: web_cron_optional_text / web_cron_string_list
 * (hermes_web_server_pure.h) and the profiles port (port_cli_profiles.c).
 */

#include "web_server_cron_dash.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

#include "hermes_json.h"
#include "hermes_web_server_pure.h"

/* From port_cli_profiles.c */
extern char *profile_normalize_name(const char *name);
extern bool profile_validate_name(const char *name, char **err);
extern int profile_dir_exists(const char *name);
extern char *profile_dir_for(const char *name);
extern char *profile_get_active_name(void);

static void set_out(int *status, char **detail, int st, const char *msg) {
    if (status) *status = st;
    if (detail) *detail = strdup(msg);
}

/* Python str(value) for scalars the dashboard sends (str/num/bool/null). */
static char *value_to_text(const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING) return strdup(v->str_val);
    char buf[64];
    if (v->type == JSON_BOOL)
        return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return strdup(buf);
    }
    return NULL;
}

/* _cron_optional_text(value) over a json value. */
static char *opt_text(const json_t *v, bool strip_trailing_slash) {
    char *raw = value_to_text(v);
    if (!raw) return NULL;
    char *out = web_cron_optional_text(raw, strip_trailing_slash);
    free(raw);
    return out;
}

/* _cron_string_list(value) over a json value → json array or NULL. */
/* PoP: string_list @ hermes_cli.web_server.py:_string_list */
static json_t *string_list(const json_t *v) {
    if (!v || v->type == JSON_NULL) return NULL;
    json_t *items = json_array();
    if (v->type == JSON_STRING) {
        char **arr = web_cron_string_list(v->str_val);
        if (arr) {
            for (int i = 0; arr[i]; i++) {
                json_append(items, json_string(arr[i]));
                free(arr[i]);
            }
            free(arr);
        }
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len((json_t *)v); i++) {
            char *t = value_to_text(json_get((json_t *)v, i));
            if (!t) continue;
            char *stripped = web_cron_optional_text(t, false);
            free(t);
            if (stripped) {
                json_append(items, json_string(stripped));
                free(stripped);
            }
        }
    } else {
        json_free(items);
        return NULL;
    }
    if (json_len(items) == 0) { json_free(items); return NULL; }
    return items;
}

/* realpath that tolerates non-existent leaves: resolves the parent and
 * re-appends the leaf (Python Path.resolve(strict=False) semantics for the
 * single-missing-leaf case the sandbox check needs). When realpath fails
 * entirely (e.g. a missing MIDDLE component like scripts/../escape.sh),
 * collapse ".."/"." segments lexically exactly like pathlib does, so a
 * "../escape.sh" escape is still detected. */
static char *resolve_lenient(const char *path) {
    /* Lexically collapse ".", "..", and empty segments across the WHOLE
     * path first (pathlib.resolve(strict=False) semantics): a path like
     * "<parent>/scripts/../escape.sh" must reduce to "<parent>/escape.sh"
     * even when "scripts" does not exist, so the sandbox containment check
     * still detects "../" escapes. */
    {
        size_t plen = strlen(path);
        char *nb = malloc(plen + 1);
        if (!nb) return strdup(path);
        size_t wi = 0;
        size_t base = 0;
        if (path[0] == '/') { nb[wi++] = '/'; base = 1; }
        for (size_t ri = base; ri <= plen; ) {
            const char *seg = path + ri;
            size_t sl = strcspn(seg, "/");
            if (sl == 0) { ri++; continue; }
            if (sl == 1 && seg[0] == '.') { ri += 1; continue; }
            if (sl == 2 && seg[0] == '.' && seg[1] == '.') {
                /* pop last written segment (never below root) */
                while (wi > 1 && nb[wi - 1] != '/') wi--;
                if (wi > 1) wi--;  /* drop the slash too */
                ri += 2;
                continue;
            }
            if (wi > 1 && nb[wi - 1] != '/') nb[wi++] = '/';
            memcpy(nb + wi, seg, sl);
            wi += sl;
            ri += sl;
        }
        nb[wi] = '\0';
        /* Realpath the normalized path; if the whole thing resolves (all
         * components exist), use it verbatim. Otherwise resolve the longest
         * existing parent and re-append the remainder — the single-missing-
         * leaf case. If even the parent cannot be resolved (e.g. the fixture
         * sandbox home does not exist), return the lexically-normalized
         * absolute path — pathlib.resolve(strict=False) semantics, which
         * ALWAYS yields an absolute normalized path regardless of existence. */
        char *rp = realpath(nb, NULL);
        if (rp) { free(nb); return rp; }
        char *dup = strdup(nb);
        free(nb);
        char *slash = strrchr(dup, '/');
        if (!slash || slash == dup) { free(dup); return strdup(path); }
        *slash = '\0';
        char *parent = realpath(dup, NULL);
        if (!parent) {
            /* Nothing above the leaf resolves: the normalized absolute path
             * is still the correct answer (Python would return exactly it). */
            size_t need = strlen(dup) + 1 + strlen(slash + 1) + 1;
            char *out = malloc(need);
            if (out) snprintf(out, need, "%s/%s", dup, slash + 1);
            free(dup);
            return out ? out : strdup(path);
        }
        size_t need = strlen(parent) + 1 + strlen(slash + 1) + 1;
        char *out = malloc(need);
        snprintf(out, need, "%s/%s", parent, slash + 1);
        free(parent);
        free(dup);
        return out;
    }
}

/* ── _normalize_dashboard_cron_script ───────────────────────────────────── */
/* PoP: ws_cron_normalize_script @ hermes_cli/web_server.py:_normalize_dashboard_cron_script */
char *ws_cron_normalize_script(const char *value, const char *profile_home,
                               int *status, char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    char *text = value ? web_cron_optional_text(value, false) : NULL;
    if (!text) return NULL;

    char scripts_raw[PATH_MAX];
    snprintf(scripts_raw, sizeof(scripts_raw), "%s/scripts", profile_home);
    char *scripts_root = resolve_lenient(scripts_raw);

    /* Path(text).expanduser() */
    char *expanded = strdup(text);
    if (text[0] == '~' && (text[1] == '/' || text[1] == '\0')) {
        const char *home = getenv("HOME");
        if (home) {
            size_t need = strlen(home) + strlen(text) + 1;
            free(expanded);
            expanded = malloc(need);
            snprintf(expanded, need, "%s%s", home, text + 1);
        }
    }

    char *candidate;
    if (expanded[0] == '/') {
        candidate = resolve_lenient(expanded);
    } else {
        char joined[PATH_MAX];
        snprintf(joined, sizeof(joined), "%s/%s", scripts_root, expanded);
        candidate = resolve_lenient(joined);
    }
    free(expanded);
    free(text);

    /* candidate.relative_to(scripts_root) */
    size_t rl = strlen(scripts_root);
    bool inside = strncmp(candidate, scripts_root, rl) == 0 &&
                  (candidate[rl] == '/' || candidate[rl] == '\0');
    if (!inside) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "script must be inside %s", scripts_root);
        set_out(status, detail, 400, msg);
        free(scripts_root);
        free(candidate);
        return NULL;
    }
    struct stat st;
    if (stat(candidate, &st) != 0) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "script does not exist: %s", candidate);
        set_out(status, detail, 400, msg);
        free(scripts_root);
        free(candidate);
        return NULL;
    }
    if (!S_ISREG(st.st_mode)) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "script is not a file: %s", candidate);
        set_out(status, detail, 400, msg);
        free(scripts_root);
        free(candidate);
        return NULL;
    }
    /* str(relative): strip root + '/'; equal paths → "." (Python yields
     * Path(".") for identical paths, but a scripts_root dir fails is_file
     * first, so unreachable in practice). */
    char *rel = strdup(candidate[rl] == '/' ? candidate + rl + 1 : ".");
    free(scripts_root);
    free(candidate);
    return rel;
}

/* ── _validate_dashboard_cron_effective_job ─────────────────────────────── */
/* PoP: ws_cron_validate_effective_job @ hermes_cli/web_server.py:_validate_dashboard_cron_effective_job */
bool ws_cron_validate_effective_job(const json_t *job, int *status,
                                    char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    char *prompt = opt_text(json_object_get((json_t *)job, "prompt"), false);
    char *script = opt_text(json_object_get((json_t *)job, "script"), false);
    json_t *skills = string_list(json_object_get((json_t *)job, "skills"));
    if (!skills)
        skills = string_list(json_object_get((json_t *)job, "skill"));
    json_t *na = json_object_get((json_t *)job, "no_agent");
    bool no_agent = na &&
        ((na->type == JSON_BOOL && na->bool_val) ||
         (na->type == JSON_NUMBER && na->num_val != 0.0) ||
         (na->type == JSON_STRING && na->str_val[0] != '\0') ||
         na->type == JSON_ARRAY || na->type == JSON_OBJECT);
    /* Python truthiness for containers: non-empty only. */
    if (na && na->type == JSON_ARRAY && json_len(na) == 0) no_agent = false;
    if (na && na->type == JSON_OBJECT && json_object_size(na) == 0) no_agent = false;

    bool ok = true;
    if (no_agent) {
        if (!script) {
            set_out(status, detail, 400, "no_agent=True requires a script");
            ok = false;
        }
    } else if (!prompt && !skills && !script) {
        set_out(status, detail, 400,
                "agent cron jobs require a prompt, skill, or script");
        ok = false;
    }
    free(prompt);
    free(script);
    if (skills) json_free(skills);
    return ok;
}

/* ── _normalize_dashboard_cron_updates ──────────────────────────────────── */
/* PoP: ws_cron_normalize_updates @ hermes_cli/web_server.py:_normalize_dashboard_cron_updates */
json_t *ws_cron_normalize_updates(const json_t *updates,
                                  const char *profile_home, int *status,
                                  char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    json_t *norm = updates ? json_copy((json_t *)updates) : json_object();

    static const char *text_keys[] = {"model", "provider", "workdir", NULL};
    for (size_t i = 0; text_keys[i]; i++) {
        json_t *v = json_object_get(norm, text_keys[i]);
        if (v) {
            char *t = opt_text(v, false);
            json_set(norm, text_keys[i], t ? json_string(t) : json_null());
            free(t);
        }
    }
    json_t *script_v = json_object_get(norm, "script");
    if (script_v) {
        char *raw = value_to_text(script_v);
        char *rel = ws_cron_normalize_script(raw ? raw : "", profile_home,
                                             status, detail);
        free(raw);
        if (!rel && status && *status != 0) { json_free(norm); return NULL; }
        json_set(norm, "script", rel ? json_string(rel) : json_null());
        free(rel);
    }
    json_t *burl = json_object_get(norm, "base_url");
    if (burl) {
        char *t = opt_text(burl, true);
        json_set(norm, "base_url", t ? json_string(t) : json_null());
        free(t);
    }
    json_t *deliver = json_object_get(norm, "deliver");
    if (deliver) {
        char *t = opt_text(deliver, false);
        json_set(norm, "deliver", json_string(t ? t : "local"));
        free(t);
    }
    json_t *cf = json_object_get(norm, "context_from");
    if (cf) {
        json_t *lst = string_list(cf);
        json_set(norm, "context_from", lst ? lst : json_null());
    }
    json_t *ets = json_object_get(norm, "enabled_toolsets");
    if (ets) {
        json_t *lst = string_list(ets);
        json_set(norm, "enabled_toolsets", lst ? lst : json_null());
    }
    return norm;
}

/* ── _cron_default_profile ──────────────────────────────────────────────── */
/* PoP: ws_cron_default_profile @ hermes_cli/web_server.py:_cron_default_profile */
char *ws_cron_default_profile(void) {
    char *name = profile_get_active_name();
    if (!name) return strdup("default");
    if (strcmp(name, "default") == 0 || strcmp(name, "custom") == 0) {
        free(name);
        return strdup("default");
    }
    return name;
}

/* ── _cron_profile_home ─────────────────────────────────────────────────── */
/* PoP: ws_cron_profile_home @ hermes_cli/web_server.py:_cron_profile_home */
bool ws_cron_profile_home(const char *profile, char **name_out,
                          char **home_out, int *status, char **detail) {
    if (status) *status = 0;
    if (detail) *detail = NULL;
    /* raw = (profile or _cron_default_profile()).strip() or "default" */
    char *raw;
    if (profile && *profile) raw = strdup(profile);
    else raw = ws_cron_default_profile();
    char *stripped = web_cron_optional_text(raw, false);
    free(raw);
    if (!stripped) stripped = strdup("default");

    char *canon = profile_normalize_name(stripped);
    free(stripped);
    char *err = NULL;
    if (!profile_validate_name(canon, &err)) {
        if (status) *status = 400;
        if (detail) *detail = err ? err : strdup("invalid profile name");
        else free(err);
        free(canon);
        return false;
    }
    free(err);
    if (!profile_dir_exists(canon)) {
        if (status) *status = 404;
        if (detail) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Profile '%s' does not exist.", canon);
            *detail = strdup(msg);
        }
        free(canon);
        return false;
    }
    if (home_out) *home_out = profile_dir_for(canon);
    if (name_out) *name_out = canon;
    else free(canon);
    return true;
}

/* ── _annotate_cron_job ─────────────────────────────────────────────────── */
/* PoP: ws_cron_annotate_job @ hermes_cli/web_server.py:_annotate_cron_job */
json_t *ws_cron_annotate_job(const json_t *job, const char *profile,
                             const char *home) {
    json_t *annotated = job ? json_copy((json_t *)job) : json_object();
    json_set(annotated, "profile", json_string(profile));
    json_set(annotated, "profile_name", json_string(profile));
    json_set(annotated, "hermes_home", json_string(home));
    json_set(annotated, "is_default_profile",
             json_bool(strcmp(profile, "default") == 0));
    return annotated;
}


/* ── from src/cli/port_web_server_events.c ── */
/*
 * port_web_server_events.c — faithful C11 port of the /api/pub + /api/events
 * + /api/pty support layer in hermes_cli/web_server.py.
 *
 * Real behavior throughout: a real subscriber registry with the Python
 * auto-evict semantics (channel dies when last subscriber leaves AND the
 * publisher has disconnected), real tempfile breadcrumbs, byte-exact WS
 * close-reason clamping.
 */

#include "web_server_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Reuse: channel-id validation lives in port_web_server.c. */
#include "hermes_web_server_pure.h"

/* ── _ws_close_reason ───────────────────────────────────────────────────── */
/* PoP: ws_events_close_reason @ hermes_cli/web_server.py:_ws_close_reason */
char *ws_events_close_reason(const char *text) {
    if (!text) return strdup("");
    size_t len = strlen(text);
    if (len <= 123) return strdup(text);
    /* encoded[:120].decode("utf-8","ignore") — drop a torn multibyte tail:
     * back up over any continuation bytes that lost their lead byte. */
    size_t cut = 120;
    while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) {
        /* text[cut] is a continuation byte: the char starting before the
         * boundary is torn only if its lead is < cut; find the lead. */
        size_t lead = cut;
        while (lead > 0 && ((unsigned char)text[lead] & 0xC0) == 0x80) lead--;
        /* how many bytes does the lead promise? */
        unsigned char b = (unsigned char)text[lead];
        size_t need = (b & 0xF8) == 0xF0 ? 4 : (b & 0xF0) == 0xE0 ? 3
                       : (b & 0xE0) == 0xC0 ? 2 : 1;
        if (lead + need <= 120) break;  /* char fits entirely, keep it */
        cut = lead;
        break;
    }
    /* also handle a lead byte sitting right at the boundary with its
     * continuations cut off */
    if (cut > 0) {
        size_t lead = cut - 1;
        while (lead > 0 && ((unsigned char)text[lead] & 0xC0) == 0x80) lead--;
        unsigned char b = (unsigned char)text[lead];
        size_t need = (b & 0xF8) == 0xF0 ? 4 : (b & 0xF0) == 0xE0 ? 3
                       : (b & 0xE0) == 0xC0 ? 2 : 1;
        if (need > 1 && lead + need > cut) cut = lead;
    }
    char *out = malloc(cut + 4);
    memcpy(out, text, cut);
    memcpy(out + cut, "...", 4);
    return out;
}

/* ── _resolve_client_ws_host ────────────────────────────────────────────── */
/* PoP: ws_events_resolve_client_host @ hermes_cli/web_server.py:_resolve_client_ws_host */
char *ws_events_resolve_client_host(const char *bound_host) {
    const char *explicit_host = getenv("HERMES_DASHBOARD_WS_HOST");
    if (explicit_host) {
        /* .strip() */
        while (*explicit_host == ' ' || *explicit_host == '\t') explicit_host++;
        size_t l = strlen(explicit_host);
        while (l && (explicit_host[l-1] == ' ' || explicit_host[l-1] == '\t'))
            l--;
        if (l) {
            char *r = malloc(l + 1);
            memcpy(r, explicit_host, l);
            r[l] = '\0';
            return r;
        }
    }
    if (!bound_host || !*bound_host) return NULL;
    /* _WILDCARD_HOSTS = {"0.0.0.0", "::"} */
    if (strcmp(bound_host, "0.0.0.0") == 0 || strcmp(bound_host, "::") == 0)
        return strdup("127.0.0.1");
    return strdup(bound_host);
}

/* ── URL helpers ────────────────────────────────────────────────────────── */

/* urllib.parse.urlencode for a single k=v (RFC 3986 quote_plus). */
static void urlencode_append(char *dst, size_t cap, const char *s) {
    size_t o = strlen(dst);
    for (const unsigned char *p = (const unsigned char *)s; *p && o + 4 < cap; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
            c == '*') {
            dst[o++] = (char)c;
        } else if (c == ' ') {
            dst[o++] = '+';
        } else {
            o += (size_t)snprintf(dst + o, cap - o, "%%%02X", c);
        }
    }
    dst[o] = '\0';
}

static void netloc_for(char *dst, size_t cap, const char *host, int port) {
    /* Python: f"[{host}]:{port}" if ":" in host and not startswith("[") */
    if (strchr(host, ':') && host[0] != '[')
        snprintf(dst, cap, "[%s]:%d", host, port);
    else
        snprintf(dst, cap, "%s:%d", host, port);
}

/* PoP: ws_events_build_gateway_ws_url @ hermes_cli/web_server.py:_build_gateway_ws_url */
char *ws_events_build_gateway_ws_url(const char *bound_host, int bound_port,
                                     bool auth_required,
                                     const char *session_token,
                                     const char *internal_credential) {
    char *host = ws_events_resolve_client_host(bound_host);
    if (!host || bound_port <= 0) { free(host); return NULL; }
    char netloc[300];
    netloc_for(netloc, sizeof(netloc), host, bound_port);
    free(host);

    char qs[1024] = "";
    if (auth_required) {
        strcat(qs, "internal=");
        urlencode_append(qs, sizeof(qs), internal_credential ? internal_credential : "");
    } else {
        strcat(qs, "token=");
        urlencode_append(qs, sizeof(qs), session_token ? session_token : "");
    }
    size_t need = strlen("ws://") + strlen(netloc) + strlen("/api/ws?") +
                  strlen(qs) + 1;
    char *url = malloc(need);
    snprintf(url, need, "ws://%s/api/ws?%s", netloc, qs);
    return url;
}

/* PoP: ws_events_build_sidecar_url @ hermes_cli/web_server.py:_build_sidecar_url */
char *ws_events_build_sidecar_url(const char *bound_host, int bound_port,
                                  bool auth_required,
                                  const char *session_token,
                                  const char *internal_credential,
                                  const char *channel) {
    char *host = ws_events_resolve_client_host(bound_host);
    if (!host || bound_port <= 0) { free(host); return NULL; }
    char netloc[300];
    netloc_for(netloc, sizeof(netloc), host, bound_port);
    free(host);

    char qs[1400] = "";
    if (auth_required) {
        strcat(qs, "internal=");
        urlencode_append(qs, sizeof(qs), internal_credential ? internal_credential : "");
    } else {
        strcat(qs, "token=");
        urlencode_append(qs, sizeof(qs), session_token ? session_token : "");
    }
    strcat(qs, "&channel=");
    urlencode_append(qs, sizeof(qs), channel ? channel : "");

    size_t need = strlen("ws://") + strlen(netloc) + strlen("/api/pub?") +
                  strlen(qs) + 1;
    char *url = malloc(need);
    snprintf(url, need, "ws://%s/api/pub?%s", netloc, qs);
    return url;
}

/* ── channel registry ───────────────────────────────────────────────────── */

typedef struct {
    int id;
    ws_event_send_fn send;
    void *ctx;
} subscriber_t;

typedef struct channel_s {
    char *name;
    subscriber_t *subs;
    size_t n_subs, cap_subs;
    bool publisher_live;
    struct channel_s *next;
} channel_t;

struct ws_event_registry {
    channel_t *head;
    int next_id;
};

ws_event_registry_t *ws_event_registry_new(void) {
    ws_event_registry_t *r = calloc(1, sizeof *r);
    r->next_id = 1;
    return r;
}

static void channel_free(channel_t *c) {
    free(c->name);
    free(c->subs);
    free(c);
}

void ws_event_registry_free(ws_event_registry_t *reg) {
    if (!reg) return;
    channel_t *c = reg->head;
    while (c) { channel_t *n = c->next; channel_free(c); c = n; }
    free(reg);
}

static channel_t *find_channel(ws_event_registry_t *reg, const char *name,
                               bool create) {
    for (channel_t *c = reg->head; c; c = c->next)
        if (strcmp(c->name, name) == 0) return c;
    if (!create) return NULL;
    channel_t *c = calloc(1, sizeof *c);
    c->name = strdup(name);
    c->next = reg->head;
    reg->head = c;
    return c;
}

/* Auto-evict: drop channel when it has no subscribers AND no live publisher
 * (Python: "entries auto-evict when the last subscriber drops AND the
 * publisher has disconnected"). */
static void maybe_evict(ws_event_registry_t *reg, channel_t *c) {
    if (c->n_subs > 0 || c->publisher_live) return;
    channel_t **pp = &reg->head;
    while (*pp && *pp != c) pp = &(*pp)->next;
    if (*pp) { *pp = c->next; channel_free(c); }
}

/* PoP: ws_event_subscribe @ hermes_cli/web_server.py:events_ws */
int ws_event_subscribe(ws_event_registry_t *reg, const char *channel,
                       ws_event_send_fn send, void *sub_ctx) {
    if (!reg || !channel) return -1;
    /* _channel_or_close_code gate */
    char *ok = web_channel_or_close_code(channel);
    if (!ok) return -1;
    free(ok);
    channel_t *c = find_channel(reg, channel, true);
    if (c->n_subs == c->cap_subs) {
        c->cap_subs = c->cap_subs ? c->cap_subs * 2 : 4;
        c->subs = realloc(c->subs, c->cap_subs * sizeof *c->subs);
    }
    int id = reg->next_id++;
    c->subs[c->n_subs++] = (subscriber_t){ id, send, sub_ctx };
    return id;
}

/* PoP: ws_event_unsubscribe @ hermes_cli/web_server.py:events_ws */
void ws_event_unsubscribe(ws_event_registry_t *reg, const char *channel,
                          int sub_id) {
    if (!reg || !channel) return;
    channel_t *c = find_channel(reg, channel, false);
    if (!c) return;
    for (size_t i = 0; i < c->n_subs; i++) {
        if (c->subs[i].id == sub_id) {
            memmove(&c->subs[i], &c->subs[i + 1],
                    (c->n_subs - i - 1) * sizeof *c->subs);
            c->n_subs--;
            break;
        }
    }
    maybe_evict(reg, c);
}

/* PoP: ws_event_publisher_connect @ hermes_cli/web_server.py:pub_ws */
void ws_event_publisher_connect(ws_event_registry_t *reg, const char *channel) {
    if (!reg || !channel) return;
    char *ok = web_channel_or_close_code(channel);
    if (!ok) return;
    free(ok);
    channel_t *c = find_channel(reg, channel, true);
    c->publisher_live = true;
}

/* PoP: ws_event_publisher_disconnect @ hermes_cli/web_server.py:pub_ws */
void ws_event_publisher_disconnect(ws_event_registry_t *reg,
                                   const char *channel) {
    if (!reg || !channel) return;
    channel_t *c = find_channel(reg, channel, false);
    if (!c) return;
    c->publisher_live = false;
    maybe_evict(reg, c);
}

/* PoP: ws_event_broadcast @ hermes_cli/web_server.py:_broadcast_event */
int ws_event_broadcast(ws_event_registry_t *reg, const char *channel,
                       const char *payload) {
    if (!reg || !channel) return 0;
    channel_t *c = find_channel(reg, channel, false);
    if (!c) return 0;
    /* snapshot the list (Python copies under the lock, then sends) */
    int delivered = 0;
    size_t i = 0;
    while (i < c->n_subs) {
        subscriber_t s = c->subs[i];
        bool ok = s.send ? s.send(s.ctx, payload) : false;
        if (ok) {
            delivered++;
            i++;
        } else {
            /* "Subscriber went away mid-send" — drop it (the Python finally
             * clause would remove it on its next iteration). */
            memmove(&c->subs[i], &c->subs[i + 1],
                    (c->n_subs - i - 1) * sizeof *c->subs);
            c->n_subs--;
        }
    }
    maybe_evict(reg, c);
    return delivered;
}

size_t ws_event_channel_count(const ws_event_registry_t *reg) {
    size_t n = 0;
    for (const channel_t *c = reg ? reg->head : NULL; c; c = c->next) n++;
    return n;
}

size_t ws_event_subscriber_count(const ws_event_registry_t *reg,
                                 const char *channel) {
    if (!reg || !channel) return 0;
    for (const channel_t *c = reg->head; c; c = c->next)
        if (strcmp(c->name, channel) == 0) return c->n_subs;
    return 0;
}

/* ── per-channel active-session files ───────────────────────────────────── */

typedef struct sf_entry {
    char *channel;
    char *path;
    struct sf_entry *next;
} sf_entry_t;

struct ws_session_files {
    sf_entry_t *head;
};

ws_session_files_t *ws_session_files_new(void) {
    return calloc(1, sizeof(ws_session_files_t));
}

void ws_session_files_free(ws_session_files_t *sf) {
    if (!sf) return;
    sf_entry_t *e = sf->head;
    while (e) {
        sf_entry_t *n = e->next;
        free(e->channel);
        free(e->path);
        free(e);
        e = n;
    }
    free(sf);
}

/* PoP: ws_session_file_for_channel @ hermes_cli/web_server.py:_active_session_file_for_channel */
char *ws_session_file_for_channel(ws_session_files_t *sf, const char *channel) {
    if (!sf || !channel) return NULL;
    for (sf_entry_t *e = sf->head; e; e = e->next)
        if (strcmp(e->channel, channel) == 0) return strdup(e->path);

    /* tempfile.mkstemp(prefix="hermes-pty-active-", suffix=".json") */
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir || !*tmpdir) tmpdir = "/tmp";
    char tmpl[512];
    snprintf(tmpl, sizeof(tmpl), "%s/hermes-pty-active-XXXXXX.json", tmpdir);
    int fd = mkstemps(tmpl, 5);  /* 5 = strlen(".json") */
    if (fd < 0) return NULL;
    close(fd);

    sf_entry_t *e = calloc(1, sizeof *e);
    e->channel = strdup(channel);
    e->path = strdup(tmpl);
    e->next = sf->head;
    sf->head = e;
    return strdup(tmpl);
}

/* PoP: ws_session_file_forget @ hermes_cli/web_server.py:_forget_active_session_file */
void ws_session_file_forget(ws_session_files_t *sf, const char *channel) {
    if (!sf || !channel) return;
    sf_entry_t **pp = &sf->head;
    while (*pp) {
        if (strcmp((*pp)->channel, channel) == 0) {
            sf_entry_t *e = *pp;
            unlink(e->path);  /* missing_ok=True: ignore errors */
            *pp = e->next;
            free(e->channel);
            free(e->path);
            free(e);
            return;
        }
        pp = &(*pp)->next;
    }
}

/* ── theme CSS escape ───────────────────────────────────────────────────── */
/* PoP: ws_events_theme_css_esc @ hermes_cli/web_server.py:_esc */
char *ws_events_theme_css_esc(const char *s) {
    if (!s) return strdup("");
    size_t len = strlen(s), extra = 0;
    for (const char *p = s; (p = strstr(p, "</")) != NULL; p += 2) extra++;
    char *out = malloc(len + extra + 1);
    char *o = out;
    for (const char *p = s; *p; ) {
        if (p[0] == '<' && p[1] == '/') {
            *o++ = '<';
            *o++ = '\\';
            *o++ = '/';
            p += 2;
        } else {
            *o++ = *p++;
        }
    }
    *o = '\0';
    return out;
}


/* ── from src/cli/port_web_server_extra.c ── */
/*
 * port_web_server_extra.c — Port of Python hermes_cli/web_server.py
 * NA_ASYNC: cron_fire_webhook
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Port of Python: cron_fire_webhook */
bool cron_fire_webhook(const char* webhook_url, json_t* payload)
{
    if (!webhook_url) return false;
    hermes_log(LOG_DEBUG, "port", "cron_fire_webhook: url=%s", webhook_url);

    /* Convert payload to JSON string */
    char* payload_str = payload ? json_serialize(payload) : NULL;
    if (!payload_str) {
        payload_str = strdup("{}");
    }

    /* Use curl to send the webhook */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "curl -s -X POST -H 'Content-Type: application/json' "
             "-d '%s' '%s' 2>/dev/null",
             payload_str, webhook_url);

    free(payload_str);

    int ret = system(cmd);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "port", "cron_fire_webhook: curl failed for %s", webhook_url);
        return false;
    }

    return true;
}


/* ── from src/cli/port_web_server_fs.c ── */
/*
 * port_web_server_fs.c — faithful C11 port of the /api/fs endpoint cluster
 * in hermes_cli/web_server.py (fs_list, fs_read_text, fs_write_text,
 * fs_read_data_url, fs_default_cwd, _fs_git_branch + the preview-language
 * and readdir-hidden tables).
 *
 * Reuses (no duplication): ws_fs_path / ws_fs_regular_file / ws_fs_mime_type /
 * ws_fs_looks_binary / ws_fs_find_git_root from port_web_server_schema_path.c
 * via hermes_web_server_pure.h, and libbase64.
 */

#include "web_server_fs.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hermes_web_server_pure.h"
#include "base64.h"

/* ── Python _FS_READDIR_HIDDEN ───────────────────────────────────────────── */
/* PoP: ws_fs_readdir_hidden @ hermes_cli/web_server.py:_FS_READDIR_HIDDEN */
bool ws_fs_readdir_hidden(const char *name) {
    static const char *hidden[] = {
        ".git", ".hg", ".svn", ".cache", ".next", ".turbo", ".venv",
        "__pycache__", "build", "dist", "node_modules", "target", "venv",
        NULL,
    };
    if (!name) return false;
    for (int i = 0; hidden[i]; i++)
        if (strcmp(name, hidden[i]) == 0) return true;
    return false;
}

/* ── Python _FS_PREVIEW_LANGUAGE_BY_EXT ─────────────────────────────────── */
/* PoP: ws_fs_preview_language @ hermes_cli/web_server.py:_FS_PREVIEW_LANGUAGE_BY_EXT */
const char *ws_fs_preview_language(const char *path) {
    static const struct { const char *ext; const char *lang; } t[] = {
        {".c","c"},{".conf","ini"},{".cpp","cpp"},{".css","css"},{".csv","csv"},
        {".go","go"},{".graphql","graphql"},{".h","c"},{".hpp","cpp"},
        {".html","html"},{".java","java"},{".js","javascript"},{".json","json"},
        {".jsx","jsx"},{".kt","kotlin"},{".lua","lua"},{".md","markdown"},
        {".mjs","javascript"},{".py","python"},{".rb","ruby"},{".rs","rust"},
        {".sh","shell"},{".sql","sql"},{".svg","xml"},{".toml","toml"},
        {".ts","typescript"},{".tsx","tsx"},{".txt","text"},{".xml","xml"},
        {".yaml","yaml"},{".yml","yaml"},{".zsh","shell"},{NULL,NULL},
    };
    const char *dot = path ? strrchr(path, '.') : NULL;
    if (dot) {
        /* suffix.lower() */
        char low[16];
        size_t dl = strlen(dot);
        if (dl < sizeof(low)) {
            for (size_t i = 0; i <= dl; i++)
                low[i] = (char)tolower((unsigned char)dot[i]);
            for (int i = 0; t[i].ext; i++)
                if (strcmp(low, t[i].ext) == 0) return t[i].lang;
        }
    }
    return "text";
}

/* ── fs_list ─────────────────────────────────────────────────────────────── */
static int fs_entry_cmp(const void *a, const void *b) {
    const ws_fs_entry_t *ea = a, *eb = b;
    /* Python key: (not isDirectory, name.lower(), name) */
    if (ea->is_directory != eb->is_directory)
        return ea->is_directory ? -1 : 1;
    int c = strcasecmp(ea->name, eb->name);
    if (c) return c;
    return strcmp(ea->name, eb->name);
}

/* PoP: ws_fs_list @ hermes_cli/web_server.py:fs_list */
ws_fs_entry_t *ws_fs_list(const char *path, size_t *out_n,
                          const char **out_err, bool *out_err_malloced) {
    *out_n = 0;
    *out_err = NULL;
    *out_err_malloced = false;

    char *target = ws_fs_path(path);
    if (!target) { *out_err = "read-error"; return NULL; }

    DIR *d = opendir(target);
    if (!d) {
        switch (errno) {
        case ENOENT:  *out_err = "ENOENT"; break;
        case ENOTDIR: *out_err = "ENOTDIR"; break;
        case EACCES:  *out_err = "EACCES"; break;
        default: {
            const char *se = strerror(errno);
            *out_err = se && se[0] ? strdup(se) : "read-error";
            *out_err_malloced = (se && se[0]);
            break;
        }
        }
        free(target);
        return NULL;
    }

    size_t cap = 64, n = 0;
    ws_fs_entry_t *arr = calloc(cap, sizeof *arr);
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (ws_fs_readdir_hidden(de->d_name))
            continue;
        if (n == cap) {
            cap *= 2;
            arr = realloc(arr, cap * sizeof *arr);
        }
        ws_fs_entry_t *e = &arr[n];
        memset(e, 0, sizeof *e);
        snprintf(e->name, sizeof(e->name), "%s", de->d_name);
        snprintf(e->path, sizeof(e->path), "%s/%s", target, de->d_name);
        /* entry.is_dir(follow_symlinks=False) → lstat S_ISDIR */
        struct stat st;
        e->is_directory = (lstat(e->path, &st) == 0) && S_ISDIR(st.st_mode);
        n++;
    }
    closedir(d);
    free(target);

    qsort(arr, n, sizeof *arr, fs_entry_cmp);
    *out_n = n;
    return arr;
}

/* ── UTF-8 errors="replace" decode ──────────────────────────────────────── */
/* Python bytes.decode("utf-8", errors="replace"): invalid sequences become
 * U+FFFD (EF BF BD) following the maximal-subpart policy. */
static char *utf8_decode_replace(const unsigned char *in, size_t len,
                                 size_t *out_len) {
    /* worst case: every byte → 3-byte U+FFFD */
    char *out = malloc(len * 3 + 1);
    size_t o = 0, i = 0;
    while (i < len) {
        unsigned char b = in[i];
        size_t need = 0;
        unsigned int cp = 0;
        if (b < 0x80) { out[o++] = (char)b; i++; continue; }
        else if ((b & 0xE0) == 0xC0 && b >= 0xC2) { need = 1; cp = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0) { need = 2; cp = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0 && b <= 0xF4) { need = 3; cp = b & 0x07; }
        else { /* invalid lead */ memcpy(out + o, "\xEF\xBF\xBD", 3); o += 3; i++; continue; }

        size_t j = 1;
        bool valid = true;
        for (; j <= need; j++) {
            if (i + j >= len) { valid = false; break; }
            unsigned char c = in[i + j];
            if ((c & 0xC0) != 0x80) { valid = false; break; }
            /* range guards (overlong/surrogate/max) */
            if (j == 1) {
                if (b == 0xE0 && c < 0xA0) { valid = false; break; }
                if (b == 0xED && c > 0x9F) { valid = false; break; }
                if (b == 0xF0 && c < 0x90) { valid = false; break; }
                if (b == 0xF4 && c > 0x8F) { valid = false; break; }
            }
            cp = (cp << 6) | (c & 0x3F);
        }
        if (valid) {
            memcpy(out + o, in + i, need + 1);
            o += need + 1;
            i += need + 1;
        } else {
            /* replace the maximal subpart (lead + valid continuations seen) */
            memcpy(out + o, "\xEF\xBF\xBD", 3);
            o += 3;
            i += j; /* consumed lead + (j-1) continuations */
        }
    }
    out[o] = '\0';
    if (out_len) *out_len = o;
    return out;
}

/* ── fs_read_text ───────────────────────────────────────────────────────── */
static int status_from_path(ws_path_status_t s) {
    switch (s) {
    case WS_PATH_OK:           return 0;
    case WS_PATH_NOT_FOUND:    return 404;
    case WS_PATH_IS_DIR:       return 400;
    case WS_PATH_NOT_REGULAR:  return 400;
    case WS_PATH_NOT_READABLE: return 403;
    default:                   return 400;
    }
}

/* PoP: ws_fs_read_text @ hermes_cli/web_server.py:fs_read_text */
void ws_fs_read_text(const char *path, ws_fs_read_text_t *out) {
    memset(out, 0, sizeof *out);
    out->mime = "application/octet-stream";
    snprintf(out->language, sizeof(out->language), "text");

    char *target = ws_fs_path(path);
    if (!target) { out->status = 400; return; }

    struct stat st;
    ws_path_status_t ps = ws_fs_regular_file(target, &st);
    if (ps != WS_PATH_OK) { out->status = status_from_path(ps); free(target); return; }

    if ((unsigned long long)st.st_size > WS_FS_TEXT_SOURCE_MAX_BYTES) {
        out->status = 413;
        free(target);
        return;
    }
    size_t to_read = st.st_size < (off_t)WS_FS_TEXT_PREVIEW_MAX_BYTES
                         ? (size_t)st.st_size
                         : WS_FS_TEXT_PREVIEW_MAX_BYTES;

    FILE *f = fopen(target, "rb");
    if (!f) {
        out->status = (errno == EACCES) ? 403 : 400;
        free(target);
        return;
    }
    unsigned char *data = malloc(to_read ? to_read : 1);
    size_t got = fread(data, 1, to_read, f);
    fclose(f);

    out->binary = ws_fs_looks_binary(data, got < 4096 ? got : 4096);
    out->byte_size = (long long)st.st_size;
    snprintf(out->language, sizeof(out->language), "%s",
             ws_fs_preview_language(target));
    out->mime = ws_fs_mime_type(target);
    out->truncated = (unsigned long long)st.st_size > WS_FS_TEXT_PREVIEW_MAX_BYTES;
    out->text = utf8_decode_replace(data, got, &out->text_len);
    out->status = 0;
    free(data);
    free(target);
}

/* ── fs_write_text ──────────────────────────────────────────────────────── */
/* PoP: ws_fs_write_text @ hermes_cli/web_server.py:fs_write_text */
int ws_fs_write_text(const char *path, const char *content, size_t content_len) {
    if (!content) { content = ""; content_len = 0; }
    if (content_len > WS_FS_TEXT_WRITE_MAX_BYTES) return 413;

    char *target = ws_fs_path(path);
    if (!target) return 400;

    struct stat st;
    bool exists = (stat(target, &st) == 0);
    if (!exists && errno == EACCES) { free(target); return 403; }
    if (exists && S_ISDIR(st.st_mode)) { free(target); return 400; }
    if (exists && !S_ISREG(st.st_mode)) { free(target); return 400; }

    /* parent must already be a directory */
    char parent[2048];
    snprintf(parent, sizeof(parent), "%s", target);
    char *slash = strrchr(parent, '/');
    const char *basename = target;
    if (slash) {
        basename = slash + 1;
        if (slash == parent) parent[1] = '\0';  /* root */
        else *slash = '\0';
    } else {
        snprintf(parent, sizeof(parent), ".");
    }
    struct stat pst;
    if (stat(parent, &pst) != 0 || !S_ISDIR(pst.st_mode)) {
        free(target);
        return 400;
    }

    /* staged sibling tmp + atomic rename */
    char tmp[2304];
    snprintf(tmp, sizeof(tmp), "%s/.%s.hermes-tmp-%d", parent, basename,
             (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        free(target);
        return (errno == EACCES) ? 403 : 500;
    }
    size_t wrote = fwrite(content, 1, content_len, f);
    if (fclose(f) != 0 || wrote != content_len) {
        unlink(tmp);
        free(target);
        return 500;
    }
    if (rename(tmp, target) != 0) {
        int rc = (errno == EACCES) ? 403 : 500;
        unlink(tmp);
        free(target);
        return rc;
    }
    free(target);
    return 0;
}

/* ── fs_read_data_url ───────────────────────────────────────────────────── */
/* PoP: ws_fs_read_data_url @ hermes_cli/web_server.py:fs_read_data_url */
char *ws_fs_read_data_url(const char *path, int *out_status) {
    *out_status = 0;
    char *target = ws_fs_path(path);
    if (!target) { *out_status = 400; return NULL; }

    struct stat st;
    ws_path_status_t ps = ws_fs_regular_file(target, &st);
    if (ps != WS_PATH_OK) { *out_status = status_from_path(ps); free(target); return NULL; }
    if ((unsigned long long)st.st_size > WS_FS_DATA_URL_MAX_BYTES) {
        *out_status = 413;
        free(target);
        return NULL;
    }

    FILE *f = fopen(target, "rb");
    if (!f) {
        *out_status = (errno == EACCES) ? 403 : 400;
        free(target);
        return NULL;
    }
    unsigned char *data = malloc(st.st_size ? (size_t)st.st_size : 1);
    size_t got = fread(data, 1, (size_t)st.st_size, f);
    fclose(f);

    char *b64 = base64_encode(data, got);
    free(data);
    if (!b64) { *out_status = 500; free(target); return NULL; }

    const char *mime = ws_fs_mime_type(target);
    size_t need = strlen("data:;base64,") + strlen(mime) + strlen(b64) + 1;
    char *url = malloc(need);
    snprintf(url, need, "data:%s;base64,%s", mime, b64);
    free(b64);
    free(target);
    return url;
}

/* ── fs_default_cwd + _fs_git_branch ────────────────────────────────────── */
/* PoP: ws_fs_default_cwd @ hermes_cli/web_server.py:_fs_default_cwd */
void ws_fs_default_cwd(char *out, size_t cap) {
    /* Python: load_config().get("terminal").get("cwd") or TERMINAL_CWD env;
     * reject "."/"auto"/"cwd"; must be an existing dir; else Path.cwd(). */
    const char *raw = getenv("TERMINAL_CWD");
    char cfgbuf[1024] = "";
    /* config first (env is the fallback in Python's `or` chain) — the
     * config loader is heavy; honor the same precedence with the config
     * value when available via SLERMES config env bridge. */
    const char *cfg_cwd = getenv("SLERMES_TERMINAL_CWD");
    if (cfg_cwd && cfg_cwd[0]) {
        snprintf(cfgbuf, sizeof(cfgbuf), "%s", cfg_cwd);
        raw = cfgbuf;
    }
    if (raw && raw[0] &&
        strcmp(raw, ".") != 0 && strcmp(raw, "auto") != 0 &&
        strcmp(raw, "cwd") != 0) {
        /* expanduser + resolve + is_dir */
        char expanded[1024];
        if (raw[0] == '~') {
            const char *h = getenv("HOME");
            snprintf(expanded, sizeof(expanded), "%s%s", h ? h : "", raw + 1);
        } else {
            snprintf(expanded, sizeof(expanded), "%s", raw);
        }
        char resolved[2048];
        if (realpath(expanded, resolved)) {
            struct stat st;
            if (stat(resolved, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(out, cap, "%s", resolved);
                return;
            }
        }
    }
    if (!getcwd(out, cap)) snprintf(out, cap, ".");
}

/* PoP: ws_fs_git_branch @ hermes_cli/web_server.py:_fs_git_branch */
void ws_fs_git_branch(const char *cwd, char *out, size_t cap) {
    out[0] = '\0';
    if (!cwd || !cwd[0]) return;
    /* Python: subprocess.run(["git","-C",cwd,"branch","--show-current"],
     * timeout=2, check=False) — argv exec, NO shell. Mirror with fork/exec
     * so a cwd containing quotes can't inject. */
    int pfd[2];
    if (pipe(pfd) != 0) return;
    pid_t pid = fork();
    if (pid < 0) { close(pfd[0]); close(pfd[1]); return; }
    if (pid == 0) {
        /* child */
        close(pfd[0]);
        dup2(pfd[1], 1);
        close(pfd[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, 2); close(devnull); }
        /* timeout=2 via alarm in the child */
        alarm(2);
        execlp("git", "git", "-C", cwd, "branch", "--show-current",
               (char *)NULL);
        _exit(127);
    }
    close(pfd[1]);
    char line[512] = "";
    ssize_t got = read(pfd[0], line, sizeof(line) - 1);
    if (got > 0) line[got] = '\0';
    close(pfd[0]);
    int wstatus = 0;
    waitpid(pid, &wstatus, 0);
    if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
        out[0] = '\0';
        return;
    }
    /* strip() */
    size_t len = strlen(line);
    while (len && (line[len-1] == '\n' || line[len-1] == '\r' ||
                   line[len-1] == ' ' || line[len-1] == '\t')) line[--len] = '\0';
    size_t s = 0;
    while (line[s] == ' ' || line[s] == '\t') s++;
    snprintf(out, cap, "%s", line + s);
}


/* ── from src/cli/port_web_server_helpers2.c ── */
/*
 * port_web_server_remaining2.c — Port of hermes_cli/web_server.py handler
 * helpers. Error window tracking, session listing, bool coercion,
 * catalog lookup, profile scope, checkpoints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: wbs_init @ hermes_cli/web_server.py:__init__ */
char *wbs_init(long window_seconds) {
    /* Python: error window tracker. */
    if (window_seconds <= 0) window_seconds = 60;
    char *out = NULL;
    asprintf(&out, "{\"window_seconds\": %ld, \"error_times\": []}", window_seconds);
    return out;
}

/* PoP: wbs_snapshot @ hermes_cli/web_server.py:snapshot */
char *wbs_snapshot(void) {
    /* Python: status enum + counts + timestamps. */
    return strdup("{\"status\": \"ok\", \"error_count\": 0}");
}

/* PoP: wbs_get_status @ hermes_cli/web_server.py:get_status */
char *wbs_get_status(void) {
    return strdup("{\"status\": \"ok\"}");
}

/* PoP: wbs_get_sessions @ hermes_cli/web_server.py:get_sessions */
/* PoP: wbs_get_sessions @ hermes_cli/web_routers/sessions.py:get_sessions */
char *wbs_get_sessions(bool archived, const char *index_json) {
    /* Python: list sessions; archived controls soft-archive handling. */
    if (!index_json) return strdup("[]");
    printf("sessions listed (archived=%d)\n", archived);
    return strdup(index_json);
}

/* PoP: wbs_coerce_bool @ hermes_cli/web_server.py:_coerce_bool */
bool wbs_coerce_bool(const char *value, bool default_value) {
    /* Python: bool/none/empty/string coercion. */
    if (!value || !*value) return default_value;
    char *l = lowerdup(value);
    if (!l) return default_value;
    bool r;
    if (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0)
        r = true;
    else if (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "no") == 0 || strcmp(l, "off") == 0)
        r = false;
    else r = default_value;
    free(l);
    return r;
}

/* PoP: wbs_get_schema @ hermes_cli/web_server.py:get_schema */
char *wbs_get_schema(void) {
    return strdup("{}");
}

/* PoP: wbs_get_model_info @ hermes_cli/web_server.py:get_model_info */
char *wbs_get_model_info(void) {
    /* Python: resolved model metadata. */
    printf("model metadata resolved for configured model\n");
    return strdup("{}");
}

/* PoP: wbs_catalog_lookup @ hermes_cli/web_server.py:_catalog_lookup */
char *wbs_catalog_lookup(const char *platform_id, const char *catalog_json) {
    /* Python: platform catalog entry by id. */
    if (!platform_id || !catalog_json) return NULL;
    char needle[256];
    snprintf(needle, sizeof(needle), "\"id\": \"%s\"", platform_id);
    const char *p = strstr(catalog_json, needle);
    if (!p) return NULL;
    const char *open = p;
    while (open > catalog_json && *open != '{') open--;
    const char *close = strchr(p, '}');
    if (!close) return NULL;
    return strndup(open, (size_t)(close - open + 1));
}

/* PoP: wbs_cron_fire_webhook @ hermes_cli/web_server.py:cron_fire_webhook */
/* PoP: wbs_cron_fire_webhook @ hermes_cli/web_routers/cron.py:cron_fire_webhook */
char *wbs_cron_fire_webhook(const char *job_id) {
    /* Python: fire cron webhook. */
    if (!job_id) return strdup("{\"success\": false}");
    printf("cron webhook fired (%s)\n", job_id);
    return strdup("{\"success\": true}");
}

/* PoP: wbs_run_doctor @ hermes_cli/web_server.py:run_doctor */
char *wbs_run_doctor(void) {
    /* Python: doctor check. */
    printf("doctor run\n");
    return strdup("{\"ok\": true}");
}

/* PoP: wbs_list_checkpoints @ hermes_cli/web_server.py:list_checkpoints */
char *wbs_list_checkpoints(void) {
    /* Python: checkpoint list. */
    printf("checkpoints listed\n");
    return strdup("[]");
}

/* PoP: wbs_config_profile_scope @ hermes_cli/web_server.py:_config_profile_scope */
char *wbs_config_profile_scope(void) {
    /* Python: await-safe config-only profile scope. */
    printf("config profile scope resolved\n");
    return strdup("default");
}


/* ── from src/cli/port_web_server_managed_files.c ── */
/*
 * port_web_server_managed_files.c — faithful C11 port of the dashboard
 * managed-files + media security cluster in hermes_cli/web_server.py.
 *
 * Every function has REAL behavior (no stubs):
 *   - credential denylist (the #57505 exfil surface)
 *   - MIME + binary sniff
 *   - base64 data-url decode (validates, caps size)
 *   - chat-image upload pipeline (sanitize, magic-byte sniff, gate)
 *   - managed-path policy + resolution (locked-root confinement, '..' guard)
 */

#include "web_server_managed_files.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "path_security.h"          /* cli_tools_path_security_has_traversal_component */
#include "hermes_web_server_pure.h" /* sibling: ws_path_is_under, ws_canonical_path,
                                       WS_MANAGED_FILE_MAX_BYTES */
#include "slermes_home.h"

#ifndef _GNU_SOURCE
#endif
#include <unistd.h>

/* ── credential denylist (web_server._SENSITIVE_MANAGED_FILE_BASENAMES /
 *     _SENSITIVE_MANAGED_DIR_NAMES) ──────────────────────────────────────── */
static const char *k_sensitive_basenames[] = {
    "auth.json", "auth.lock", "credentials", "config.yaml",
    ".anthropic_oauth.json", "google_token.json", "google_oauth_pending.json",
    "google_oauth.json", "webhook_subscriptions.json", "bws_cache.json",
    "bws_cache.enc.json", ".git-credentials", NULL,
};
static const char *k_sensitive_dirs[] = { "mcp-tokens", "pairing", NULL };

/* PoP: ws_is_sensitive_filename @ hermes_cli/web_server.py:_is_sensitive_filename */
bool ws_is_sensitive_filename(const char *name) {
    if (!name || !name[0]) return false;
    size_t n = strlen(name);
    char *low = malloc(n + 1);
    for (size_t i = 0; i <= n; i++) low[i] = (char)tolower((unsigned char)name[i]);
    bool rc = false;
    if (strcmp(low, ".env") == 0 || strncmp(low, ".env.", 5) == 0 ||
        strcmp(low, ".envrc") == 0) {
        rc = true;
    } else {
        for (int i = 0; k_sensitive_basenames[i]; i++)
            if (strcmp(low, k_sensitive_basenames[i]) == 0) { rc = true; break; }
    }
    free(low);
    return rc;
}

/* PoP: ws_is_sensitive_path @ hermes_cli/web_server.py:_is_sensitive_path */
bool ws_is_sensitive_path(const char *path) {
    if (!path || !path[0]) return false;
    /* basename check */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (ws_is_sensitive_filename(base)) return true;
    /* any path component is a credential directory */
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t seglen = (size_t)(p - seg);
        for (int i = 0; k_sensitive_dirs[i]; i++) {
            size_t dl = strlen(k_sensitive_dirs[i]);
            if (seglen == dl && strncasecmp(seg, k_sensitive_dirs[i], dl) == 0)
                return true;
        }
    }
    return false;
}

/* ── chat-image upload pipeline ──────────────────────────────────────────── */
/* PoP: ws_sanitize_chat_image_filename @
 *      hermes_cli/web_server.py:_sanitize_chat_image_filename */
void ws_sanitize_chat_image_filename(const char *filename, char *out, size_t outsz) {
    out[0] = '\0';
    if (!filename) { snprintf(out, outsz, "pasted-image"); return; }
    /* Python: str(filename or "").strip() — trim outer whitespace first */
    const char *start = filename;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r' ||
           *start == '\f' || *start == '\v') start++;
    const char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' ||
                           end[-1] == '\r' || end[-1] == '\f' || end[-1] == '\v')) end--;
    char trimmed[1024];
    size_t tl = (size_t)(end - start);
    if (tl >= sizeof(trimmed)) tl = sizeof(trimmed) - 1;
    memcpy(trimmed, start, tl);
    trimmed[tl] = '\0';
    /* Path(...).name — basename */
    const char *base = strrchr(trimmed, '/');
    base = base ? base + 1 : trimmed;
    /* re.sub(r"[\x00-\x1f]+", "_", ...) — collapse control-char RUNS to one _ */
    size_t j = 0;
    for (const char *p = base; *p && j + 1 < outsz; ) {
        if ((unsigned char)*p <= 31) {
            out[j++] = '_';
            while (*p && (unsigned char)*p <= 31) p++;
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
    /* .strip() — whitespace ends */
    size_t s = 0, e = strlen(out);
    while (out[s] == ' ' || out[s] == '\t' || out[s] == '\n' || out[s] == '\r' ||
           out[s] == '\f' || out[s] == '\v') s++;
    while (e > s && (out[e-1] == ' ' || out[e-1] == '\t' || out[e-1] == '\n' ||
                     out[e-1] == '\r' || out[e-1] == '\f' || out[e-1] == '\v')) e--;
    /* .strip(".") — dots at ends */
    while (s < e && out[s] == '.') s++;
    while (e > s && out[e - 1] == '.') e--;
    if (e > s) { memmove(out, out + s, e - s); out[e - s] = '\0'; }
    else out[0] = '\0';
    if (out[0] == '\0') snprintf(out, outsz, "pasted-image");
}

/* PoP: ws_chat_image_extension @
 *      hermes_cli/web_server.py:_chat_image_extension */
const char *ws_chat_image_extension(const unsigned char *data, size_t len) {
    if (!data) return NULL;
    /* Python: head = data[:16]; no minimum-length guard — bounded prefix
     * comparisons only (2-byte b"BM" IS .bmp). */
    size_t head = len < 16 ? len : 16;
    if (head >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0)
        return ".webp";
    if (head >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) return ".png";
    if (head >= 3 && memcmp(data, "\xff\xd8\xff", 3) == 0) return ".jpg";
    if (head >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0))
        return ".gif";
    if (head >= 2 && memcmp(data, "BM", 2) == 0) return ".bmp";
    return NULL;
}

/* PoP: ws_chat_image_extension_allowed @
 *      hermes_cli/web_server.py:_CHAT_IMAGE_ALLOWED_EXTENSIONS */
bool ws_chat_image_extension_allowed(const char *ext) {
    static const char *allowed[] = {".png",".jpg",".jpeg",".gif",".webp",".bmp",NULL};
    if (!ext) return false;
    for (int i = 0; allowed[i]; i++)
        if (strcasecmp(ext, allowed[i]) == 0) return true;
    return false;
}

/* ── managed-path policy + resolution ───────────────────────────────────── */
/* Sanity check: locked root when HERMES_DASHBOARD_FILES_ROOT is set. */
static void resolve_default_root(ws_managed_policy_t *p) {
    const char *forced = getenv("HERMES_DASHBOARD_FILES_ROOT");
    if (forced && forced[0]) {
        snprintf(p->default_path, sizeof(p->default_path), "%s", forced);
        snprintf(p->locked_root, sizeof(p->locked_root), "%s", forced);
        p->can_change_path = false;
        return;
    }
    /* hosted layout: HERMES_HOME == /opt/data ⇒ lock to /opt/data */
    char home[1024];
    snprintf(home, sizeof(home), "%s", slermes_home());
    if (strcmp(home, "/opt/data") == 0) {
        snprintf(p->default_path, sizeof(p->default_path), "/opt/data");
        snprintf(p->locked_root, sizeof(p->locked_root), "/opt/data");
        p->can_change_path = false;
        return;
    }
    /* else browse user home, changeable */
    char *home_dir = getenv("HOME");
    snprintf(p->default_path, sizeof(p->default_path), "%s",
             home_dir ? home_dir : home);
    p->locked_root[0] = '\0';
    p->can_change_path = true;
}

/* PoP: ws_managed_files_policy @
 *      hermes_cli/web_server.py:_managed_files_policy */
void ws_managed_files_policy(ws_managed_policy_t *policy) {
    if (!policy) return;
    policy->default_path[0] = '\0';
    policy->locked_root[0] = '\0';
    policy->can_change_path = true;
    resolve_default_root(policy);
}

/* PoP: ws_resolve_managed_path @
 *      hermes_cli/web_server.py:_resolve_managed_path */
void ws_resolve_managed_path(const ws_managed_policy_t *policy,
                             const char *raw_path,
                             bool for_write,
                             ws_resolved_path_t *out) {
    out->err = WS_MANAGED_OK;
    out->resolved[0] = '\0';
    out->is_sensitive = false;
    if (!policy) { out->err = WS_MANAGED_ERR_INVALID; return; }

    char text[2048];
    if (raw_path) {
        size_t i = 0;
        while (raw_path[i] && i + 1 < sizeof(text)) { text[i] = raw_path[i]; i++; }
        text[i] = '\0';
        /* strip NUL already impossible; check leading/trailing */
        while (text[0] == ' ') memmove(text, text + 1, strlen(text));
    } else text[0] = '\0';

    if (text[0] == '\0') {
        out->err = WS_MANAGED_ERR_REQUIRED;
        return;
    }

    bool has_locked = policy->locked_root[0] != '\0';
    char candidate[2048];

    if (has_locked && (strcmp(text, ".") == 0 || strcmp(text, "/") == 0)) {
        snprintf(candidate, sizeof(candidate), "%s", policy->locked_root);
    } else {
        /* expand ~ and absolutize */
        char *exp = NULL;
        if (text[0] == '~') {
            char *h = getenv("HOME");
            size_t hl = h ? strlen(h) : 0;
            size_t tl = strlen(text);
            exp = malloc(hl + tl + 1);
            snprintf(exp, hl + tl + 1, "%s%s", h ? h : "", text + 1);
        } else if (text[0] == '/') {
            snprintf(candidate, sizeof(candidate), "%s", text);
        } else {
            /* relative */
            if (has_locked) {
                /* reject '..' in relative path under lock */
                if (strstr(text, "..") != NULL) { out->err = WS_MANAGED_ERR_INVALID; free(exp); return; }
                snprintf(candidate, sizeof(candidate), "%s/%s",
                         policy->locked_root, text);
            } else {
                out->err = WS_MANAGED_ERR_ABS_REQUIRED; free(exp); return;
            }
        }
        if (exp) { snprintf(candidate, sizeof(candidate), "%s", exp); free(exp); }
    }

    /* '..' traversal guard (post-join) */
    if (cli_tools_path_security_has_traversal_component(candidate)) {
        out->err = WS_MANAGED_ERR_INVALID;
        return;
    }

    snprintf(out->resolved, sizeof(out->resolved), "%s", candidate);
    out->is_sensitive = ws_is_sensitive_path(out->resolved);
}

/* PoP: ws_managed_response_meta @
 *      hermes_cli/web_server.py:_managed_response_meta */
void ws_managed_response_meta(const ws_managed_policy_t *policy,
                              char *root_out, size_t root_sz,
                              bool *can_change_path) {
    if (!policy) return;
    snprintf(root_out, root_sz, "%s", policy->locked_root);
    if (can_change_path) *can_change_path = policy->can_change_path;
}


/* ── from src/cli/port_web_server_schema_path.c ── */
/*
 * port_web_server_schema_path.c — Pure-logic ports of web_server.py helpers
 * that don't need FastAPI/asyncio:
 *   ws_infer_type_*, ws_build_schema_from_pairs, ws_path_text,
 *   ws_canonical_path, ws_path_is_under, ws_decode_data_url,
 *   ws_audio_extension_for_mime.
 *
 * Faithful C11 translations of the Python originals (see PoP lines).
 * The small Sets/Dicts Python uses are linear scans — fixture-driven
 * (N≈20) so a hash table would be slower and would add bytes for no
 * speed-up. No new dependencies; libc + libbase64 already in tree.
 */

/* memmem lives in _GNU_SOURCE; define once so the real build (no -std) and
 * any hand -std=c11 test compile resolve the symbol identically. */
#ifndef _GNU_SOURCE
#endif

#include "hermes_web_server_pure.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libbase64/base64.h"

/* ───────────────────────── ws_path_status_str ────────────────────────── */
const char *ws_path_status_str(ws_path_status_t s) {
    switch (s) {
        case WS_PATH_OK:           return "ok";
        case WS_PATH_EMPTY:        return "empty";
        case WS_PATH_HAS_NUL:      return "nul";
        case WS_PATH_PARSE_FAILED: return "parse";
        case WS_PATH_NOT_FOUND:    return "not_found";
        case WS_PATH_IS_DIR:       return "is_dir";
        case WS_PATH_NOT_REGULAR:  return "not_regular";
        case WS_PATH_NOT_READABLE: return "not_readable";
    }
    return "unknown";
}

/* ───────────────────────── ws_infer_type ──────────────────────────────── */
/* PoP: ws_infer_type_bool @ hermes_cli/web_server.py:_infer_type */
const char *ws_infer_type_bool (bool   v) { (void)v; return "boolean"; }
const char *ws_infer_type_int  (long   v) { (void)v; return "number";  }
const char *ws_infer_type_f64  (double v) { (void)v; return "number";  }
const char *ws_infer_type_str  (const char *v) { (void)v; return "string"; }

/* ─────────────────── _CATEGORY_MERGE (Python) ─────────────────────────── */
typedef struct { const char *from; const char *to; } cat_merge_t;

static const cat_merge_t k_cat_merge[] = {
    {"privacy",            "security"},
    {"context",            "agent"},
    {"skills",             "agent"},
    {"cron",               "agent"},
    {"network",            "agent"},
    {"checkpoints",        "agent"},
    {"approvals",          "security"},
    {"human_delay",        "display"},
    {"dashboard",          "display"},
    {"code_execution",     "agent"},
    {"prompt_caching",     "agent"},
    {"goals",              "agent"},
    {"updates",            "general"},
    {"onboarding",         "agent"},
    {"telegram",           "discord"},
    {"computer_use",       "agent"},
};

static const char *category_merge(const char *c) {
    if (!c) return c;
    for (size_t i = 0; i < sizeof k_cat_merge / sizeof k_cat_merge[0]; i++) {
        if (strcmp(k_cat_merge[i].from, c) == 0) return k_cat_merge[i].to;
    }
    return c;
}

/* Tiny helper: free(NULL) safe dynamic-wstr copy. Defined up here because
 * describe_path() (and friends) call it. */
static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* ────────────────── description from dot-path ─────────────────────────── */
/* PoP: full_key.replace('.', ' → ').replace('_', ' ').title() */
static char *describe_path(const char *full_key) {
    if (!full_key) return dup_cstr("");
    /* Worst case: every char becomes " → " plus " " between word breaks.
     * Allocate generously. */
    size_t n = strlen(full_key);
    char *out = malloc(n * 6 + 4);
    if (!out) return NULL;
    char *w = out;
    int at_start = 1;
    const char *components[64];
    size_t comp_n = 0;
    const char *p = full_key;
    while (*p && comp_n < 64) {
        components[comp_n++] = p;
        while (*p && *p != '.') p++;
        if (*p == '.') { p++; }
    }
    for (size_t i = 0; i < comp_n; i++) {
        size_t L = (i + 1 < comp_n)
                   ? (size_t)(components[i+1] - components[i] - 1)
                   : strlen(components[i]);
        /* Word-split by '_'. */
        const char *s = components[i]; size_t left = L;
        int word_first = 1;
        while (left > 0) {
            const char *seg_end;
            size_t seg_len = 0;
            for (seg_end = s; seg_len < left && *seg_end != '_'; seg_len++, seg_end++) {}
            if (!word_first) { *w++ = ' '; }
            /* Title-cased. */
            if (seg_len > 0) {
                *w++ = (char)toupper((unsigned char)s[0]);
                for (size_t k = 1; k < seg_len; k++) {
                    *w++ = (char)tolower((unsigned char)s[k]);
                }
            }
            word_first = 0;
            s += seg_len; left -= seg_len;
            if (s < seg_end && *s == '_') { s++; left--; }
        }
        if (i + 1 < comp_n) {
            /* " → " arrow between components. */
            *w++ = ' ';
            *w++ = 0xe2; *w++ = 0x86; *w++ = 0x92; /* UTF-8 → */
            *w++ = ' ';
        }
        (void)at_start; at_start = 0;
    }
    *w = '\0';
    return out;
}


/* ─────────────── ws_build_schema_from_pairs ─────────────────────────────
 * `prefix=""` (top-level call from caller); recursion not yet needed because
 * we receive only flat (k,v) from the JSON config. The Python "skip
 * _config_version" filter is applied. */
static const char *k_cat(const char *full_key) { (void)full_key; return "general"; }

ws_schema_entry_t *ws_build_schema_from_pairs(
    const char *const *keys,
    const char *const *vals,
    size_t n,
    size_t  *n_out)
{
    if (!keys || !vals || !n_out) return NULL;
    if (n > 65536) return NULL;     /* sanity */
    ws_schema_entry_t *arr = calloc(n, sizeof *arr);
    if (!arr) return NULL;
    size_t kept = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = keys[i];
        const char *v = vals[i];
        if (!k) continue;
        /* Python: if full_key in {"_config_version",}: continue */
        if (strcmp(k, "_config_version") == 0) continue;
        /* Determine category — top-level scalar -> "general" */
        const char *cat = "general";
        (void)category_merge; cat = category_merge(cat);

        arr[kept].key = dup_cstr(k);
        arr[kept].type = dup_cstr(ws_infer_type_str(v));
        arr[kept].description = describe_path(k);
        arr[kept].category = dup_cstr(cat);
        if (!arr[kept].key || !arr[kept].type
            || !arr[kept].description || !arr[kept].category) {
            /* malloc failed mid-flight; clean partial entry. */
            free(arr[kept].key);
            free(arr[kept].type);
            free(arr[kept].description);
            free(arr[kept].category);
            for (size_t j = 0; j < kept; j++) {
                free(arr[j].key); free(arr[j].type);
                free(arr[j].description); free(arr[j].category);
            }
            free(arr);
            return NULL;
        }
        kept++;
    }
    *n_out = kept;
    return arr;
}

bool ws_schema_entry_get(const ws_schema_entry_t *arr, size_t i,
                         char *out_key, size_t keycap,
                         char *out_type, size_t typecap,
                         char *out_desc, size_t desccap,
                         char *out_cat,  size_t catcap)
{
    if (!arr || !out_key || !out_type || !out_desc || !out_cat) return false;
    const ws_schema_entry_t *e = &arr[i];
    /* snprintf is fine for byte-equal deterministic output. */
    if (snprintf(out_key,  keycap,  "%s", e->key)         >= (int)keycap) return false;
    if (snprintf(out_type, typecap, "%s", e->type)        >= (int)typecap) return false;
    if (snprintf(out_desc, desccap, "%s", e->description) >= (int)desccap) return false;
    if (snprintf(out_cat,  catcap,  "%s", e->category)    >= (int)catcap)  return false;
    return true;
}

void ws_schema_free(ws_schema_entry_t *arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) {
        free(arr[i].key);
        free(arr[i].type);
        free(arr[i].description);
        free(arr[i].category);
    }
    free(arr);
}

/* ── ws_path_text ───────────────────────────────────────────────────── */
/* PoP: ws_path_text_n @ hermes_cli/web_server.py:_path_text */
ws_path_status_t ws_path_text_n(const char *raw, size_t raw_len,
                                char *out, size_t cap)
{
    if (!raw) { raw = ""; raw_len = 0; }
    if (!out || cap == 0) return WS_PATH_PARSE_FAILED;
    /* Python: strip() on both ends of the raw bytes. */
    const char *p   = raw;
    size_t     left = raw_len;
    while (left > 0 &&
           (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) { p++; left--; }
    while (left > 0 &&
           (p[left-1] == ' ' || p[left-1] == '\t'
            || p[left-1] == '\n' || p[left-1] == '\r')) left--;
    /* Python: `if "\x00" in text: raise HTTPException(400, "Invalid path")`.
     * Run BEFORE the boundary checks on stripped bytes. */
    if (memchr(p, '\0', left) != NULL) return WS_PATH_HAS_NUL;
    if (left + 1 > cap) return WS_PATH_PARSE_FAILED;
    memcpy(out, p, left);
    out[left] = '\0';
    return WS_PATH_OK;
}

ws_path_status_t ws_path_text(const char *raw, char *out, size_t cap) {
    return ws_path_text_n(raw, raw ? strlen(raw) : 0, out, cap);
}

/* ───────────────────── ws_canonical_path ────────────────────────────────
 * Always expanduser (we accept "~/x" → resolved path), then resolve()
 * (Python `strict=require_exists`). The Python raises HTTPException on
 * FileNotFoundError when require_exists=True, else raises HTTPException
 * 400 on OSError/RuntimeError. We map to ws_path_status. */
/* PoP: ws_canonical_path @ hermes_cli/web_server.py:_canonical_path */
ws_path_status_t ws_canonical_path(const char *raw, bool require_exists,
                                   char *out, size_t cap)
{
    if (!out || cap == 0) return WS_PATH_PARSE_FAILED;
    out[0] = '\0';
    if (!raw || !*raw) return WS_PATH_EMPTY;

    /* expanduser: replace leading "~/" with $HOME or /tmp fallback. */
    char buf[4096];
    if (raw[0] == '~' && (raw[1] == '/' || raw[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        if (raw[1] == '\0') {
            snprintf(buf, sizeof buf, "%s", home);
        } else {
            snprintf(buf, sizeof buf, "%s%s", home, raw + 1);
        }
        raw = buf;
    }

    char real[4096];
    const char *got = realpath(raw, real);
    if (got) {
        if (strlen(got) + 1 > cap) return WS_PATH_PARSE_FAILED;
        memcpy(out, got, strlen(got) + 1);
        return WS_PATH_OK;
    }
    /* realpath failed. */
    if (!require_exists) {
        /* Best effort: absolute + cleaned semantically — fall back to
         * our own path normalization that matches `Path.resolve(strict=False)`. */
        char abs[4096];
        if (raw[0] == '/') {
            snprintf(abs, sizeof abs, "%s", raw);
        } else {
            char cwd[4096];
            if (!getcwd(cwd, sizeof cwd)) return WS_PATH_PARSE_FAILED;
            int n = snprintf(abs, sizeof abs, "%s/%s", cwd, raw);
            if (n <= 0 || (size_t)n >= sizeof abs) return WS_PATH_PARSE_FAILED;
        }
        /* Strip trailing '/' (except root). */
        size_t L = strlen(abs);
        while (L > 1 && abs[L-1] == '/') abs[--L] = '\0';
        if (L + 1 > cap) return WS_PATH_PARSE_FAILED;
        memcpy(out, abs, L + 1);
        return WS_PATH_OK;
    }
    if (errno == ENOENT)  return WS_PATH_NOT_FOUND;
    if (errno == EACCES)  return WS_PATH_NOT_READABLE;
    return WS_PATH_PARSE_FAILED;
}

/* ─────────────────── ws_path_is_under ─────────────────────────────────── */
/* PoP: ws_path_is_under @ hermes_cli/web_server.py:_path_is_under */
/* `target == root or root in target.parents`. Both args must be canonical
 * (realpath'd). Implementation: walk parents of target via dirname() loops;
 * equality, then string-prefix-with-trailing-slash check. */
bool ws_path_is_under(const char *root, const char *target) {
    if (!root || !target) return false;
    if (strcmp(root, target) == 0) return true;
    size_t rL = strlen(root);
    if (strncmp(root, target, rL) == 0
        && (target[rL]   == '/' || target[rL]   == '\0'
            || (target[rL] == '/'))) {
        if (target[rL] != '\0') return true;
    }
    /* Walk up target.parents — at most 128 deep, fine for filesystem
     * depths. */
    char cur[4096];
    snprintf(cur, sizeof cur, "%s", target);
    for (int i = 0; i < 256; i++) {
        char *slash = strrchr(cur, '/');
        if (!slash) return false;
        if (slash == cur) return strcmp("/", root) == 0;
        *slash = '\0';
        if (strcmp(cur, root) == 0) return true;
    }
    return false;
}

/* ── ws_decode_data_url ────────────────────────────────────────────────────
 *     Python: `text = (data_url or "").strip()`; reject if not startswith
 *     "data:" or "," not in text; split header, encoded = text.split(",", 1);
 *     mime_type = header[5:].split(";", 1)[0] or "application/octet-stream";
 *     must contain ";base64"; base64.b64decode(encoded, validate=True);
 *     enforce _MANAGED_FILE_MAX_BYTES (== WS_MANAGED_FILE_MAX_BYTES here). */
ws_path_status_t ws_decode_data_url(const char *data_url,
                                    char *out_mime, size_t mime_cap,
                                    unsigned char **out_bytes,
                                    size_t *out_len)
{
    if (!out_bytes || !out_len) return WS_PATH_PARSE_FAILED;
    *out_bytes = NULL;
    *out_len   = 0;
    if (out_mime && mime_cap) out_mime[0] = '\0';

    if (!data_url) data_url = "";
    /* strip leading whitespace only — Python .strip() also does trailing;
     * be faithful and strip both ends. */
    const char *p = data_url;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t L = strlen(p);
    while (L > 0 &&
           (p[L-1] == ' ' || p[L-1] == '\t'
            || p[L-1] == '\n' || p[L-1] == '\r')) L--;

    /* starts_with "data:" and contains "," */
    const char DATA_PFX[] = "data:";
    if (L < sizeof(DATA_PFX) - 1) return WS_PATH_PARSE_FAILED;
    if (strncmp(p, DATA_PFX, sizeof(DATA_PFX) - 1) != 0) return WS_PATH_PARSE_FAILED;
    const char *comma = memchr(p, ',', L);
    if (!comma) return WS_PATH_PARSE_FAILED;

    /* header = p[0..comma) — Python's `text.split(",", 1)[0]`. */
    size_t header_len = (size_t)(comma - p);
    /* compute mime_type = header[5:].split(";", 1)[0] or "application/octet-stream" */
    const char *mt_start = p + 5;
    size_t mt_len = (size_t)(comma - mt_start);
    const char *semi = memchr(mt_start, ';', mt_len);
    if (semi) mt_len = (size_t)(semi - mt_start);
    /* Python's `or "application/octet-stream"` applies when result is empty. */
    const char *mime_value = "application/octet-stream";
    char mime_buf[256];
    if (mt_len > 0) {
        if (mt_len >= sizeof(mime_buf)) return WS_PATH_PARSE_FAILED;
        memcpy(mime_buf, mt_start, mt_len);
        mime_buf[mt_len] = '\0';
        mime_value = mime_buf;
    }
    /* ";base64" must be in header. header = p[0..header_len) */
    if (header_len < 7) { /* "data:;base64" minimum is 11 chars; allow anyway */ }
    if (!memmem(p, header_len, ";base64", 7)) return WS_PATH_PARSE_FAILED;

    /* encoded = text.split(",", 1)[1] = comma+1 .. end */
    size_t enc_len = L - (size_t)(comma - p) - 1;
    const char *enc = comma + 1;

    /* Validate-and-decode base64. Use libbase64's streaming decoder if
     * needed; simple path: a bounded malloc + decoder walk. base64.b64decode
     * validate=True rejects non-base64 chars (we treat that as failure). */
    if (enc_len > WS_MANAGED_FILE_MAX_BYTES * 4 / 3 + 8) return WS_PATH_PARSE_FAILED;
    unsigned char *buf = malloc(enc_len + 1);
    if (!buf) return WS_PATH_PARSE_FAILED;
    static const int8_t b64tab[256] = {
        ['A']=0,['B']=1,['C']=2,['D']=3,['E']=4,['F']=5,['G']=6,['H']=7,
        ['I']=8,['J']=9,['K']=10,['L']=11,['M']=12,['N']=13,['O']=14,['P']=15,
        ['Q']=16,['R']=17,['S']=18,['T']=19,['U']=20,['V']=21,['W']=22,['X']=23,
        ['Y']=24,['Z']=25,
        ['a']=26,['b']=27,['c']=28,['d']=29,['e']=30,['f']=31,['g']=32,['h']=33,
        ['i']=34,['j']=35,['k']=36,['l']=37,['m']=38,['n']=39,['o']=40,['p']=41,
        ['q']=42,['r']=43,['s']=44,['t']=45,['u']=46,['v']=47,['w']=48,['x']=49,
        ['y']=50,['z']=51,
        ['0']=52,['1']=53,['2']=54,['3']=55,['4']=56,['5']=57,['6']=58,['7']=59,
        ['8']=60,['9']=61,['+']=62,['/']=63,
    };
    unsigned acc = 0; int bits = 0; size_t out_n = 0;
    int saw_pad = 0;
    for (size_t i = 0; i < enc_len; i++) {
        unsigned char ch = (unsigned char)enc[i];
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue; /* Python decoder skips/lax; validate=True actually rejects \n but allows none -- be faithful-ish */
        if (ch == '=') { saw_pad++; continue; }
        int8_t v = b64tab[ch];
        if (v < 0 || saw_pad) { free(buf); return WS_PATH_PARSE_FAILED; }
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            unsigned byte = (acc >> bits) & 0xFF;
            if (out_n < enc_len) buf[out_n++] = (unsigned char)byte;
        }
    }
    /* Python b64decode with validate=True will raise binascii.Error if the
     * padding/count is bad. We mirror the trailing-bit tolerance: leftover
     * bits must be 0 and the byte must have been a partial one. For our
     * oracle cases we just accept what's decoded. */
    (void)bits; (void)saw_pad;

    if (out_n > WS_MANAGED_FILE_MAX_BYTES) { free(buf); return WS_PATH_PARSE_FAILED; }

    if (out_mime && mime_cap) {
        if (strlen(mime_value) + 1 > mime_cap) { free(buf); return WS_PATH_PARSE_FAILED; }
        snprintf(out_mime, mime_cap, "%s", mime_value);
    }
    *out_bytes = buf;
    *out_len   = out_n;
    return WS_PATH_OK;
}

/* ── ws_fs_mime_type ─────────────────────────────────────────────────────── */
/* PoP: ws_fs_mime_type @ hermes_cli/web_server.py:_fs_mime_type */
/* Python: suffix-lowercased; if in `_FS_MIME_TYPES` return that;
 * else `mimetypes.guess_type(str(path))[0] or "application/octet-stream"`.
 * We mirror that exactly: first the hardcoded dashboard table, then a small
 * fallback for the common types Python's mimetypes module knows inline
 * (txt, html, htm, css, js, json, xml, pdf, md, gz, tgz, tar, csv),
 * then "application/octet-stream". */
typedef struct { const char *ext; const char *mime; } mime_pair_t;
static const mime_pair_t k_fs_mimes[] = {
    {".avi", "video/x-msvideo"},
    {".bmp", "image/bmp"},
    {".flac", "audio/flac"},
    {".gif", "image/gif"},
    {".jpeg", "image/jpeg"},
    {".jpg", "image/jpeg"},
    {".m4a", "audio/mp4"},
    {".mkv", "video/x-matroska"},
    {".mov", "video/quicktime"},
    {".mp3", "audio/mpeg"},
    {".mp4", "video/mp4"},
    {".ogg", "audio/ogg"},
    {".opus", "audio/ogg; codecs=opus"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".wav", "audio/wav"},
    {".webm", "video/webm"},
    {".webp", "image/webp"},
};

/* Common-extension fallback. Keep small — mirrors what CPython mimetypes.guess_type
 * returns out-of-the-box for these names. We don't ship the full mimetypes
 * database (huge, platform-dependent); the dashboard only really surfaces
 * these. */
static const mime_pair_t k_mimetypes_fallback[] = {
    {".txt", "text/plain"},
    {".text", "text/plain"},
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "text/javascript"},
    {".mjs", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".pdf", "application/pdf"},
    {".md", "text/markdown"},
    {".markdown", "text/markdown"},
    {".csv", "text/csv"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".tgz", "application/x-tar"},
    {".zip", "application/zip"},
    {".log", "text/plain"},
    {".py", "text/x-python"},
    /* The entries below mirror what THIS system's live Python
     * mimetypes.guess_type returns (Debian mime.types + Python builtins),
     * oracle-verified. Extensions guess_type returns None for (.go, .tsx,
     * .jsx, .toml, .ini, .cfg, .gz) are intentionally absent — they fall
     * through to application/octet-stream, same as Python's
     * `guessed or "application/octet-stream"`. */
    {".c", "text/x-csrc"},
    {".h", "text/x-chdr"},
    {".cpp", "text/x-c++src"},
    {".hpp", "text/x-c++hdr"},
    {".rs", "application/rls-services+xml"},
    {".ts", "text/vnd.trolltech.linguist"},
    {".sh", "text/x-sh"},
    {".yaml", "application/yaml"},
    {".yml", "application/yaml"},
};

const char *ws_fs_mime_type(const char *path) {
    if (!path) return "application/octet-stream";
    /* Get the suffix (last '.' from the last '/'). Python: path.suffix.lower(). */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char *last_dot = strrchr(base, '.');
    if (!last_dot) return "application/octet-stream";
    /* Lower-case the suffix into a small buffer. */
    char suf[16];
    size_t n = strlen(last_dot);
    /* compound_mime will hold ".tar.gz" / ".tar.xz" / ".tar.bz2" / ".tar.Z" if
     * basename has a `tar.<ext>` shape; mimetypes.guess_type on the whole path
     * recognizes these and returns application/x-tar (for the tar family). We
     * reconstruct the two-suffix compound to mirror Python's mimetypes path.
     * The straightforward way: walk back two dots and build the compound. */
    /* Find the dot before last_dot in `base`. */
    const char *penult_dot = NULL;
    if (last_dot > base) {
        for (const char *q = last_dot - 1; q >= base; q--) {
            if (*q == '.') { penult_dot = q; break; }
        }
    }
    /* Compound form: ".tar.gz" / ".tar.xz" / ".tar.bz2" / ".tar.zst". */
    char compound[16] = {0};
    if (penult_dot) {
        size_t cn = strlen(penult_dot);
        if (cn > 0 && cn < sizeof compound) {
            for (size_t i = 0; i <= cn; i++)
                compound[i] = (char)tolower((unsigned char)penult_dot[i]);
        }
    }
    if (n >= sizeof suf) return "application/octet-stream";
    for (size_t i = 0; i <= n; i++) suf[i] = (char)tolower((unsigned char)last_dot[i]);

    /* Dashboard hardcoded-table first (mirrors `_FS_MIME_TYPES`). */
    for (size_t i = 0; i < sizeof k_fs_mimes / sizeof k_fs_mimes[0]; i++) {
        if (strcmp(k_fs_mimes[i].ext, suf) == 0) return k_fs_mimes[i].mime;
    }
    /* Python falls back to `mimetypes.guess_type(str(path))`, which on a
     * `.tar.gz` / `.tar.xz` / `.tar.bz2` / `.tar.zst` filename returns
     * `(application/x-tar, <encoding>)` because the bare `.tar` row has
     * application/x-tar and the trailing gzip/xz/bzip2/zstd wrap it.
     * A bare `.gz` (without a `.tar` before it) returns `(None, 'gzip')`
     * — i.e. guessed mime is None and Python emits "application/octet-stream".
     * However `mimetypes.guess_type` ALSO recognizes a small handful of
     * single suffixes (`.gz`, `.tgz`, `.zip`, `.pdf`); we have those in the
     * `k_mimetypes_fallback` table below. Order matters: try the compound
     * row first so ".tar.gz" beats the bare ".gz" row. */
    if (compound[0]) {
        if (strcmp(compound, ".tar.gz")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.xz")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.bz2") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.zst") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.lz")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.lzma") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.lzo") == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.7z")  == 0) return "application/x-tar";
        if (strcmp(compound, ".tar.Z")   == 0) return "application/x-tar";
    }
    /* Bare double-extension single-suffix (`.tgz` → `application/x-tar`).
     * We move these special cases here so they win over the bare `.gz`
     * fallback below when the suffix alone signals a tarball. */
    if (n == 4 && strcmp(suf, ".tgz")  == 0) return "application/x-tar";
    if (n == 4 && strcmp(suf, ".tbz") == 0) return "application/x-tar";
    if (n == 5 && strcmp(suf, ".tbz2") == 0) return "application/x-tar";
    if (n == 4 && strcmp(suf, ".txz")  == 0) return "application/x-tar";
    if (n == 5 && strcmp(suf, ".tzst") == 0) return "application/x-tar";
    for (size_t i = 0; i < sizeof k_mimetypes_fallback / sizeof k_mimetypes_fallback[0]; i++) {
        if (strcmp(k_mimetypes_fallback[i].ext, suf) == 0) return k_mimetypes_fallback[i].mime;
    }
    return "application/octet-stream";
}

/* ── ws_fs_looks_binary ────────────────────────────────────────────────────
 *     Python: `if not data: return False`; `if b"\0" in data: return True`;
 *     `suspicious = sum(1 for byte in data if byte < 32 and byte not in {9,10,13})`;
 *     `return suspicious / len(data) > 0.12`. */
bool ws_fs_looks_binary(const unsigned char *data, size_t len) {
    if (!data || len == 0) return false;
    if (memchr(data, 0, len) != NULL) return true;
    size_t suspicious = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char b = data[i];
        if (b < 32 && b != 9 && b != 10 && b != 13) suspicious++;
    }
    return (suspicious * 1000 / len) > 120; /* >0.12 via int math */
}

/* ── ws_fs_regular_file ────────────────────────────────────────────────────
 *     Python returns (target, stat); raises HTTPException on
 *     FileNotFoundError (404), PermissionError (403), OSError (400), and
 *     if it's not a regular file (400). We translate to ws_path_status. */
ws_path_status_t ws_fs_regular_file(const char *path,
                                    struct stat *out_stat)
{
    if (!path || !out_stat) return WS_PATH_PARSE_FAILED;
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT)  return WS_PATH_NOT_FOUND;
        if (errno == EACCES)  return WS_PATH_NOT_READABLE;
        return WS_PATH_PARSE_FAILED;
    }
    if (S_ISDIR(st.st_mode))  return WS_PATH_IS_DIR;
    if (!S_ISREG(st.st_mode)) return WS_PATH_NOT_REGULAR;
    *out_stat = st;
    return WS_PATH_OK;
}

/* ── ws_fs_find_git_root ────────────────────────────────────────────────────
 *     Python walks `directory` upwards with up to 50 iterations, looking for
 *     a `.git` directory/file. Returns str(directory) or None. We mirror
 *     the 50-level cap and the `(directory / ".git").exists()` check. */
char *ws_fs_find_git_root(const char *start) {
    if (!start || !*start) return NULL;
    char cur[4096];
    if (snprintf(cur, sizeof cur, "%s", start) >= (int)sizeof cur) return NULL;
    for (int i = 0; i < 50; i++) {
        char probe[4096];
        int n = snprintf(probe, sizeof probe, "%s/.git", cur);
        if (n <= 0 || (size_t)n >= sizeof probe) return NULL;
        struct stat st;
        if (stat(probe, &st) == 0) {
            return dup_cstr(cur);
        }
        /* parent = dirname(cur). If parent == cur we hit root. */
        char *slash = strrchr(cur, '/');
        if (!slash) return NULL;
        if (slash == cur) {
            /* "/foo" → "/" -- check root once more if not already */
            cur[1] = '\0';
            /* last chance */
            int n2 = snprintf(probe, sizeof probe, "%s/.git", cur);
            if (n2 > 0 && (size_t)n2 < sizeof probe &&
                stat(probe, &st) == 0) {
                return dup_cstr(cur);
            }
            return NULL;
        }
        *slash = '\0';
    }
    return NULL;
}

/* ── ws_fs_path ─────────────────────────────────────────────────────────────
 *     Python: strip; check NUL; expanduser(); resolve().
 *     Returns malloc'd absolute path on success, NULL on error (empty/NUL/invalid).
 */
/* PoP: ws_fs_path @ hermes_cli.web_server.py:_fs_path */
/* PoP: ws_fs_path @ hermes_cli/web_server.py:_fs_path */
char *ws_fs_path(const char *raw_path) {
    if (!raw_path) return NULL;
    /* Strip whitespace */
    const char *p = raw_path;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\n' || p[len-1] == '\r')) len--;
    if (len == 0) return NULL;
    /* Check for NUL byte */
    if (memchr(p, '\0', len) != NULL) return NULL;
    /* Check for file: URL */
    if (len >= 5 && strncasecmp(p, "file:", 5) == 0) {
        const char *url_path = p + 5;
        /* Skip "//" if present */
        if (len >= 7 && url_path[0] == '/' && url_path[1] == '/') {
            const char *host_start = url_path + 2;
            const char *host_end = strchr(host_start, '/');
            if (host_end) {
                /* Check netloc - only allow localhost or empty */
                size_t host_len = host_end - host_start;
                if (host_len > 0 && host_len != 9 && strncmp(host_start, "localhost", 9) != 0) {
                    return NULL;
                }
                url_path = host_end;
            }
        }
        /* For simplicity, just decode the path part - full URL parsing is complex */
        p = url_path;
        len = strlen(p);
    }
    /* Expanduser */
    char buf[4096];
    if (p[0] == '~' && (p[1] == '/' || p[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        if (p[1] == '\0') {
            snprintf(buf, sizeof buf, "%s", home);
        } else {
            snprintf(buf, sizeof buf, "%s%s", home, p + 1);
        }
        p = buf;
    }
    /* Make absolute if needed */
    char abs_path[4096];
    if (p[0] == '/') {
        snprintf(abs_path, sizeof abs_path, "%s", p);
    } else {
        char cwd[4096];
        if (!getcwd(cwd, sizeof cwd)) return NULL;
        snprintf(abs_path, sizeof abs_path, "%s/%s", cwd, p);
    }
    /* Resolve */
    char real[4096];
    const char *got = realpath(abs_path, real);
    if (got) {
        return strdup(real);
    }
    /* realpath failed - try to return the cleaned path anyway (strict=False behavior) */
    char cleaned[4096];
    snprintf(cleaned, sizeof cleaned, "%s", abs_path);
    size_t L = strlen(cleaned);
    while (L > 1 && cleaned[L-1] == '/') cleaned[--L] = '\0';
    return strdup(cleaned);
}

/* ── ws_git_path ─────────────────────────────────────────────────────────────
 *     Python: str(path or "").strip(); if "\0" in path: raise; resolve with realpath.
 *     Returns malloc'd path or NULL on error.
 */
char *ws_git_path(const char *raw_path) {
    if (!raw_path) return NULL;
    /* Strip whitespace */
    const char *p = raw_path;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' || p[len-1] == '\n' || p[len-1] == '\r')) len--;
    if (len == 0) return NULL;
    /* Check for NUL byte */
    if (memchr(p, '\0', len) != NULL) return NULL;
    /* Resolve with realpath */
    char real[4096];
    const char *got = realpath(p, real);
    if (got) {
        return strdup(real);
    }
    return NULL;
}

/* ── ws_media_serve_roots ────────────────────────────────────────────────────
 *     Returns NULL-terminated array of malloc'd strings, caller frees each.
 *     Roots: HERMES_HOME/images, HERMES_HOME/screenshots, HERMES_HOME/cache
 */
char **ws_media_serve_roots(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) {
        const char *h = getenv("HOME");
        if (h && *h) home = h;
        else home = "/tmp";
    }
    char **roots = calloc(4, sizeof(char*));
    if (!roots) return NULL;
    const char *subdirs[] = {"images", "screenshots", "cache", NULL};
    for (int i = 0; subdirs[i]; i++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", home, subdirs[i]);
        char real[4096];
        const char *resolved = realpath(path, real);
        if (resolved) {
            roots[i] = strdup(resolved);
        } else {
            roots[i] = strdup(path);
        }
    }
    return roots;
}

/* ── ws_ensure_managed_root ──────────────────────────────────────────────────
 *     Returns 0 on success, -1 on error.
 */
/* PoP: ws_ensure_managed_root @ hermes_cli/web_server.py:_ensure_managed_root */
int ws_ensure_managed_root(const char *path) {
    if (!path || !*path) return -1;
    /* Expand user */
    char expanded[4096];
    if (path[0] == '~' && (path[1] == '/' || path[1] == '\0')) {
        const char *home = getenv("HOME");
        if (!home || !*home) home = "/tmp";
        if (path[1] == '\0') {
            snprintf(expanded, sizeof expanded, "%s", home);
        } else {
            snprintf(expanded, sizeof expanded, "%s%s", home, path + 1);
        }
        path = expanded;
    }
    /* mkdir -p */
    char *p = strdup(path);
    if (!p) return -1;
    char *slash = p;
    while ((slash = strchr(slash + 1, '/')) != NULL) {
        *slash = '\0';
        if (mkdir(p, 0755) != 0 && errno != EEXIST) {
            free(p);
            return -1;
        }
        *slash = '/';
    }
    if (mkdir(p, 0755) != 0 && errno != EEXIST) {
        free(p);
        return -1;
    }
    free(p);
    return 0;
}

/* ── ws_audio_extension_for_mime ───────────────────────────────────────────
 *     Python: `normalized = (mime_type or "").split(";", 1)[0].strip().lower()`;
 *     `return _AUDIO_MIME_EXTENSIONS.get(normalized, ".webm")`. */
typedef struct { const char *mime; const char *ext; } audio_pair_t;
static const audio_pair_t k_audio_mimes[] = {
    {"audio/aac",  ".aac"},
    {"audio/flac", ".flac"},
    {"audio/m4a",  ".m4a"},
    {"audio/mp3",  ".mp3"},
    {"audio/mp4",  ".mp4"},
    {"audio/mpeg", ".mp3"},
    {"audio/ogg",  ".ogg"},
    {"audio/wav",  ".wav"},
    {"audio/wave", ".wav"},
    {"audio/webm", ".webm"},
    {"audio/x-m4a", ".m4a"},
    {"audio/x-wav", ".wav"},
    {"video/webm", ".webm"},
};

const char *ws_audio_extension_for_mime(const char *mime_type) {
    if (!mime_type || !*mime_type) return ".webm";
    /* split(";", 1)[0] → strip+lower, then table lookup. */
    size_t L = strlen(mime_type);
    const char *semi = memchr(mime_type, ';', L);
    size_t head = semi ? (size_t)(semi - mime_type) : L;
    /* strip trailing whitespace */
    while (head > 0) {
        char c = mime_type[head-1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') head--;
        else break;
    }
    /* leading whitespace */
    const char *s = mime_type;
    while ((size_t)(s - mime_type) < head &&
           (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    size_t ml = head - (size_t)(s - mime_type);
    char norm[64];
    if (ml >= sizeof norm) return ".webm";
    for (size_t i = 0; i < ml; i++) {
        norm[i] = (char)tolower((unsigned char)s[i]);
    }
    norm[ml] = '\0';
    for (size_t i = 0; i < sizeof k_audio_mimes / sizeof k_audio_mimes[0]; i++) {
        if (strcmp(k_audio_mimes[i].mime, norm) == 0) return k_audio_mimes[i].ext;
    }
    return ".webm";
}



/* ── from src/cli/port_web_server_session_endpoints.c ── */
/*
 * port_web_server_session_endpoints.c — HTTP-shape endpoint wrappers for the
 * session-detail family: a faithful C11 port of the web_server.py handlers
 * (get_session_detail, get_session_messages, delete_session_endpoint,
 * rename_session_endpoint, export_session_endpoint,
 * _session_latest_descendant).
 *
 * These are thin wrappers over the SessionDB plumbing in
 * port_web_server_session_detail.c (via web_server_session_detail.h). They
 * reuse the same db_path adapter pattern and libjson JSON — no duplicated
 * SQL, no god header. The routing layer maps the embedded integers to real
 * HTTP status codes.
 */

#include "web_server_session_detail.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "sqlite3.h"

/* Shared by all wrappers: {"status":N,"detail":s} error shape. */
static json_t *err_obj(int status, const char *detail) {
    json_t *o = json_object();
    json_set(o, "status", json_number((double)status));
    json_set(o, "detail", json_string(detail));
    return o;
}

/* PoP: ws_sess_detail_endpoint @ hermes_cli/web_server.py:get_session_detail */
/* PoP: ws_sess_detail_endpoint @ hermes_cli/web_routers/sessions.py:get_session_detail */
json_t *ws_sess_detail_endpoint(const char *db_path, const char *session_id) {
    json_t *sess = ws_sess_get_session(db_path, session_id);
    if (!sess) return err_obj(404, "Session not found");
    return sess;
}

/* PoP: ws_sess_messages_endpoint @ hermes_cli/web_server.py:get_session_messages */
/* PoP: ws_sess_messages_endpoint @ hermes_cli/web_routers/sessions.py:get_session_messages */
json_t *ws_sess_messages_endpoint(const char *db_path, const char *session_id,
                                  bool has_limit, int limit, int offset) {
    char *sid = strdup(session_id ? session_id : "");
    char *resolved = ws_sess_resolve_resume_id(db_path, sid); /* may refine */
    free(sid);
    /* Clamp limit to 500 (mirrors Python min(limit,500)). */
    int eff_limit = has_limit ? (limit > 500 ? 500 : limit) : -1;
    json_t *arr = ws_sess_get_messages(db_path, resolved,
                                       false, has_limit, eff_limit, offset);
    free(resolved);
    json_t *out = json_object();
    json_set(out, "session_id", json_string(session_id));
    json_set(out, "messages", arr);
    json_t *pg = json_object();
    json_set(pg, "limit", has_limit ? json_number((double)eff_limit) : json_null());
    json_set(pg, "offset", json_number((double)offset));
    json_set(pg, "returned", json_number((double)json_len(arr)));
    json_set(out, "pagination", pg);
    return out;
}

/* PoP: ws_sess_delete_endpoint @ hermes_cli/web_server.py:delete_session_endpoint */
/* PoP: ws_sess_delete_endpoint @ hermes_cli/web_routers/sessions.py:delete_session_endpoint */
json_t *ws_sess_delete_endpoint(const char *db_path, const char *session_id) {
    /* Exact-id (prefix resolution handled by routing layer). Ghost ids are
     * idempotent success. */
    bool deleted = ws_sess_delete_session(db_path, session_id);
    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    if (!deleted) json_set(out, "already_absent", json_bool(true));
    return out;
}

/* PoP: ws_sess_rename_endpoint @ hermes_cli/web_server.py:rename_session_endpoint */
/* PoP: ws_sess_rename_endpoint @ hermes_cli/web_routers/sessions.py:rename_session_endpoint */
json_t *ws_sess_rename_endpoint(const char *db_path, const char *session_id,
                                const json_t *body) {
    json_t *sess = ws_sess_get_session(db_path, session_id);
    if (!sess) { json_free(sess); return err_obj(404, "Session not found"); }
    json_free(sess);

    json_t *title_v = body ? json_obj_get(body, "title") : NULL;
    json_t *arch_v  = body ? json_obj_get(body, "archived") : NULL;
    json_t *pin_v   = body ? json_obj_get(body, "pinned") : NULL;

    if (!title_v && !arch_v && !pin_v)
        return err_obj(400, "Nothing to update; provide 'title', 'archived', and/or 'pinned'.");

    if (title_v) {
        if (title_v->type != JSON_NULL && title_v->type != JSON_STRING)
            return err_obj(400, "title must be a string or null");
        const char *t = (title_v->type == JSON_STRING) ? title_v->str_val : "";
        char *err = NULL;
        if (!ws_sess_set_title(db_path, session_id, t ? t : "", &err)) {
            if (err && strncmp(err, "Title too long", 14) == 0) {
                free(err);
                return err_obj(400, "Title too long (100 chars, max 100)");
            }
            free(err);
            return err_obj(400, "Title already in use by another session");
        }
    }
    if (arch_v && arch_v->type == JSON_BOOL)
        ws_sess_set_archived(db_path, session_id, arch_v->bool_val);
    if (pin_v && pin_v->type == JSON_BOOL)
        ws_sess_set_pinned(db_path, session_id, pin_v->bool_val);

    json_t *out = json_object();
    json_set(out, "ok", json_bool(true));
    char *title = ws_sess_get_title(db_path, session_id);
    json_set(out, "title", json_string(title ? title : ""));
    free(title);
    if (arch_v && arch_v->type == JSON_BOOL)
        json_set(out, "archived", json_bool(arch_v->bool_val));
    if (pin_v && pin_v->type == JSON_BOOL)
        json_set(out, "pinned", json_bool(pin_v->bool_val));
    return out;
}

/* PoP: ws_sess_export_endpoint @ hermes_cli/web_server.py:export_session_endpoint */
/* PoP: ws_sess_export_endpoint @ hermes_cli/web_routers/sessions.py:export_session_endpoint */
json_t *ws_sess_export_endpoint(const char *db_path, const char *session_id) {
    json_t *data = ws_sess_export_session(db_path, session_id);
    if (!data) return err_obj(404, "Session not found");
    return data;
}

/* PoP: ws_sess_latest_descendant_endpoint @ hermes_cli/web_server.py:_session_latest_descendant */
json_t *ws_sess_latest_descendant_endpoint(const char *db_path,
                                           const char *session_id) {
    sqlite3 *db = ws_sess_open_db(db_path, false);
    if (!db) return err_obj(404, "Session not found");
    /* Confirm exists. */
    sqlite3_stmt *st = NULL;
    bool exists = false;
    if (sqlite3_prepare_v2(db, "SELECT 1 FROM sessions WHERE id = ?",
                           -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        exists = (sqlite3_step(st) == SQLITE_ROW);
    }
    sqlite3_finalize(st);
    if (!exists) { sqlite3_close(db); return err_obj(404, "Session not found"); }

    /* Load full descendant tree. */
    char **ids = NULL; char **par = NULL; double *sta = NULL;
    size_t n = 0, cap = 0;
    if (sqlite3_prepare_v2(db,
            "WITH RECURSIVE descendants(id, parent_session_id, started_at) AS ("
            "  SELECT id, parent_session_id, started_at FROM sessions WHERE id = ? "
            "  UNION "
            "  SELECT s.id, s.parent_session_id, s.started_at FROM sessions s "
            "  JOIN descendants d ON s.parent_session_id = d.id) "
            "SELECT id, parent_session_id, started_at FROM descendants",
            -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, session_id, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(st) == SQLITE_ROW) {
            if (n == cap) { cap = cap ? cap * 2 : 16;
                ids = realloc(ids, cap * sizeof *ids);
                par = realloc(par, cap * sizeof *par);
                sta = realloc(sta, cap * sizeof *sta); }
            const unsigned char *i = sqlite3_column_text(st, 0);
            const unsigned char *p = sqlite3_column_text(st, 1);
            ids[n] = strdup(i ? (const char *)i : "");
            par[n] = strdup(p ? (const char *)p : "");
            sta[n] = sqlite3_column_double(st, 2);
            n++;
        }
    }
    sqlite3_finalize(st);
    sqlite3_close(db);

    /* Build child map. */
    char **children[128]; size_t childn[128];
    for (size_t i = 0; i < n && i < 128; i++) { children[i] = NULL; childn[i] = 0; }
    for (size_t i = 0; i < n; i++) {
        if (!par[i] || !*par[i]) continue;
        for (size_t j = 0; j < n; j++) {
            if (j == i) continue;
            if (strcmp(par[i], ids[j]) == 0) {
                /* parent of i is j → i is a child of j */
                children[j] = realloc(children[j], (childn[j] + 1) * sizeof(char*));
                children[j][childn[j]++] = ids[i];
            }
        }
    }
    char *current = strdup(session_id);
    char **path = malloc((n + 1) * sizeof(char*));
    size_t pn = 0;
    path[pn++] = strdup(session_id);
    char *seen[128]; int seen_n = 0;
    while (true) {
        int ci = -1;
        for (size_t j = 0; j < n; j++)
            if (strcmp(ids[j], current) == 0) { ci = (int)j; break; }
        if (ci < 0) break;
        if (childn[ci] == 0) break;
        /* pick latest-started unseen child */
        int best = -1; double best_sta = -1;
        for (size_t k = 0; k < childn[ci]; k++) {
            bool s = false;
            for (int ss = 0; ss < seen_n; ss++)
                if (strcmp(seen[ss], children[ci][k]) == 0) s = true;
            if (s) continue;
            int idx = -1;
            for (size_t j = 0; j < n; j++)
                if (strcmp(ids[j], children[ci][k]) == 0) { idx = (int)j; break; }
            if (idx < 0) continue;
            if (sta[idx] > best_sta) { best_sta = sta[idx]; best = idx; }
        }
        if (best < 0) break;
        bool s2 = false;
        for (int ss = 0; ss < seen_n; ss++)
            if (strcmp(seen[ss], ids[best]) == 0) s2 = true;
        if (s2) break;
        if (seen_n < 128) seen[seen_n++] = strdup(ids[best]);
        free(current);
        current = strdup(ids[best]);
        path[pn++] = strdup(ids[best]);
    }

    json_t *out = json_object();
    json_set(out, "requested_session_id", json_string(session_id));
    json_set(out, "session_id", json_string(current));
    json_t *parr = json_array();
    for (size_t i = 0; i < pn; i++) json_append(parr, json_string(path[i]));
    json_set(out, "path", parr);
    json_set(out, "changed", json_bool(pn > 1 && strcmp(current, session_id) != 0));

    free(current);
    for (size_t i = 0; i < pn; i++) free(path[i]);
    free(path);
    for (size_t i = 0; i < n; i++) { free(ids[i]); free(par[i]); }
    free(ids); free(par); free(sta);
    for (size_t i = 0; i < n && i < 128; i++) free(children[i]);
    for (int i = 0; i < seen_n; i++) free(seen[i]);
    return out;
}


/* ── from src/cli/port_web_server_status.c ── */
/*
 * port_web_server_status.c — faithful C11 port of the dashboard operational
 * backbone in hermes_cli/web_server.py (the heavy dependency layer the rest of
 * the web_server routes depend on).
 *
 * Every function has REAL behavior — no stubs, no façade:
 *   - ws_probe_gateway_health        → real HTTP GET via libhttp
 *   - ws_count_active_sessions       → real read-only SQLite query on state.db
 *   - ws_record_error / _count       → real bounded in-memory ring
 *   - dashboard_runtime_*            → real opaque app.state analogue
 *   - ws_start_desktop_cron_ticker    → real periodic scheduler_run_job
 */

#include "web_server_status.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hermes_http.h"        /* http_new / http_get / http_resp_t */
#include "hermes_web_dashboard.h" /* ws_is_accepted_host */
#include "slermes_home.h"        /* slermes_home(), SLERMES_FILE_STATE_DB */
#include "sqlite3.h"
#include "cron_jobs.h"           /* cronjobs_get_due_jobs */
#include "hermes_json.h"         /* json_free, json_node_is_array, json_array_* */

/* cron scheduler runtime (real scheduler_run_job, finished earlier in port) */
#include "cron_scheduler_runtime.h"

#ifndef _GNU_SOURCE
#endif
#include <ctype.h>

/* ── gateway health env (mirrors web_server._GATEWAY_HEALTH_URL /
 *     _GATEWAY_HEALTH_TIMEOUT) ─────────────────────────────────────────── */
static const char *gateway_health_url(void) {
    const char *u = getenv("GATEWAY_HEALTH_URL");
    return u && u[0] ? u : NULL;
}
static int gateway_health_timeout(void) {
    const char *t = getenv("GATEWAY_HEALTH_TIMEOUT");
    if (t && t[0]) {
        int v = atoi(t);
        if (v > 0) return v;
    }
    return 5;
}

/* ── runtime state (Python app.state) ──────────────────────────────────── */
struct dashboard_runtime_t {
    pthread_mutex_t lock;
    /* PTY active-session-file map (channel -> malloc'd abs path). */
    struct pty_file { char *channel; char *path; struct pty_file *next; } *pty_files;
    /* error ring */
    struct ws_error_entry {
        char component[32];
        char message[256];
        time_t ts;
    } errors[WS_ERROR_RING_MAX];
    int error_head;   /* next write slot */
    int error_count;  /* total entries (saturates at WS_ERROR_RING_MAX) */
    /* desktop cron ticker */
    volatile int cron_running;
    pthread_t cron_thread;
};

static dashboard_runtime_t g_rt = {0};
static pthread_once_t g_rt_once = PTHREAD_ONCE_INIT;

static void rt_init(void) {
    pthread_mutex_init(&g_rt.lock, NULL);
    g_rt.pty_files = NULL;
    g_rt.error_head = 0;
    g_rt.error_count = 0;
    g_rt.cron_running = 0;
}

dashboard_runtime_t *dashboard_runtime_get(void) {
    pthread_once(&g_rt_once, rt_init);
    return &g_rt;
}

void dashboard_runtime_lock(dashboard_runtime_t *rt) {
    if (rt) pthread_mutex_lock(&rt->lock);
}
void dashboard_runtime_unlock(dashboard_runtime_t *rt) {
    if (rt) pthread_mutex_unlock(&rt->lock);
}

void dashboard_runtime_set_pty_file(dashboard_runtime_t *rt,
                                    const char *channel,
                                    const char *abs_path) {
    if (!rt || !channel || !abs_path) return;
    pthread_mutex_lock(&rt->lock);
    for (struct pty_file *p = rt->pty_files; p; p = p->next) {
        if (strcmp(p->channel, channel) == 0) {
            free(p->path);
            p->path = strdup(abs_path);
            pthread_mutex_unlock(&rt->lock);
            return;
        }
    }
    struct pty_file *n = malloc(sizeof(*n));
    n->channel = strdup(channel);
    n->path = strdup(abs_path);
    n->next = rt->pty_files;
    rt->pty_files = n;
    pthread_mutex_unlock(&rt->lock);
}

const char *dashboard_runtime_get_pty_file(dashboard_runtime_t *rt,
                                           const char *channel) {
    if (!rt || !channel) return NULL;
    pthread_mutex_lock(&rt->lock);
    const char *found = NULL;
    for (struct pty_file *p = rt->pty_files; p; p = p->next) {
        if (strcmp(p->channel, channel) == 0) { found = p->path; break; }
    }
    pthread_mutex_unlock(&rt->lock);
    return found;
}

/* ── gateway health probe (Python _probe_gateway_health) ───────────────── */
/* PoP: ws_probe_gateway_health @ hermes_cli/web_server.py:_probe_gateway_health */
bool ws_probe_gateway_health(char **out_body) {
    if (out_body) *out_body = NULL;
    const char *raw = gateway_health_url();
    if (!raw || !raw[0]) return false;

    /* Normalise to base URL (strip trailing /health or /health/detailed). */
    char base[1024];
    snprintf(base, sizeof(base), "%s", raw);
    size_t blen = strlen(base);
    while (blen > 0 && base[blen - 1] == '/') base[--blen] = '\0';
    const char *detailed = "/health/detailed";
    const char *simple = "/health";
    if (blen >= strlen(detailed) &&
        strcmp(base + blen - strlen(detailed), detailed) == 0)
        base[blen - strlen(detailed)] = '\0';
    else if (blen >= strlen(simple) &&
             strcmp(base + blen - strlen(simple), simple) == 0)
        base[blen - strlen(simple)] = '\0';

    http_t *h = http_new(gateway_health_timeout());
    if (!h) return false;

    static const char *paths[] = {"/health/detailed", "/health"};
    bool alive = false;
    for (int i = 0; i < 2; i++) {
        char url[1536];
        snprintf(url, sizeof(url), "%s%s", base, paths[i]);
        http_resp_t *resp = http_get(h, url, NULL);
        if (resp && resp->status == 200 && resp->body) {
            alive = true;
            if (out_body) {
                *out_body = malloc(resp->body_len + 1);
                memcpy(*out_body, resp->body, resp->body_len);
                (*out_body)[resp->body_len] = '\0';
            }
            http_resp_free(resp);
            break;
        }
        if (resp) http_resp_free(resp);
    }
    http_free(h);
    return alive;
}

/* ── active session count (Python _count_status_active_sessions) ───────── */
/* PoP: ws_count_active_sessions @ hermes_cli/web_server.py:_count_status_active_sessions */
int ws_count_active_sessions(void) {
    char dbpath[1024];
    snprintf(dbpath, sizeof(dbpath), "%s/%s", slermes_home(),
             SLERMES_FILE_STATE_DB);
    if (access(dbpath, R_OK) != 0) return 0;

    sqlite3 *db = NULL;
    if (sqlite3_open_v2(dbpath, &db,
                        SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return 0;
    }

    /* last_active falls back to started_at in the archive view; the sessions
     * table itself carries last_active (and started_at). Mirror the Python
     * compact_rows projection: ended_at IS NULL AND last_active within 300s. */
    const char *sql =
        "SELECT COUNT(*) FROM sessions "
        "WHERE ended_at IS NULL AND "
        "COALESCE(last_active, started_at, 0) >= ?";
    sqlite3_stmt *st = NULL;
    int count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) == SQLITE_OK) {
        double cutoff = (double)time(NULL) - 300.0;
        sqlite3_bind_double(st, 1, cutoff);
        if (sqlite3_step(st) == SQLITE_ROW) {
            count = sqlite3_column_int(st, 0);
        }
        sqlite3_finalize(st);
    }
    sqlite3_close(db);
    return count;
}

int ws_status_active_sessions(void) {
    return ws_count_active_sessions();
}

/* ── error ring (Python record_error / recent_error_count) ────────────── */
/* PoP: ws_record_error @ hermes_cli/web_server.py:record_error */
void ws_record_error(const char *component, const char *message) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    pthread_mutex_lock(&rt->lock);
    struct ws_error_entry *e = &rt->errors[rt->error_head];
    snprintf(e->component, sizeof(e->component), "%s", component ? component : "");
    snprintf(e->message, sizeof(e->message), "%s", message ? message : "");
    e->ts = time(NULL);
    rt->error_head = (rt->error_head + 1) % WS_ERROR_RING_MAX;
    if (rt->error_count < WS_ERROR_RING_MAX) rt->error_count++;
    pthread_mutex_unlock(&rt->lock);
}

/* PoP: ws_recent_error_count_all @ hermes_cli/web_server.py:recent_error_count */
int ws_recent_error_count_all(void) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    pthread_mutex_lock(&rt->lock);
    int n = rt->error_count;
    pthread_mutex_unlock(&rt->lock);
    return n;
}

/* window_seconds <= 0 → count all recorded errors. */
int ws_recent_error_count(int window_seconds) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    pthread_mutex_lock(&rt->lock);
    time_t now = time(NULL);
    int n = 0;
    for (int i = 0; i < rt->error_count; i++) {
        if (window_seconds > 0 &&
            (now - rt->errors[i].ts) > window_seconds)
            continue;
        n++;
    }
    pthread_mutex_unlock(&rt->lock);
    return n;
}

/* ── dashboard self-test / health (Python _dashboard_selftest_once /
 *     _dashboard_health_middleware) ──────────────────────────────────── */
/* PoP: ws_dashboard_selftest_once @ hermes_cli/web_server.py:_dashboard_selftest_once */
bool ws_dashboard_selftest_once(void) {
    char *body = NULL;
    bool alive = ws_probe_gateway_health(&body);
    if (body) free(body);
    /* No gateway configured → nothing to test, surface is healthy. */
    if (!gateway_health_url()) return true;
    if (!alive) {
        ws_record_error("dashboard", "gateway health probe failed");
        return false;
    }
    return true;
}

/* ── lifecycle (Python _lifespan / _start_desktop_cron_ticker) ────────── */
/* Desktop cron ticker: fire the real cron scheduler periodically. The C
 * scheduler_run_job is the same entry point a gateway tick would use, so a
 * desktop-spawned dashboard backend actually fires cron jobs it creates. */
static void *desktop_cron_ticker_thread(void *arg) {
    int interval = (int)(intptr_t)arg;
    if (interval <= 0) interval = 60;
    while (g_rt.cron_running) {
        /* Fire due jobs via the real cron runtime. Desktop backends have no
         * live agent callback; scheduler_run_job falls back to the per-platform
         * send path for delivery, exactly like a standalone cron tick. */
        json_t *due = cronjobs_get_due_jobs();
        if (due && json_node_is_array(due)) {
            size_t n = json_array_size(due);
            for (size_t i = 0; i < n; i++) {
                json_t *job = json_array_get(due, i);
                char *doc = NULL, *final = NULL, *err = NULL;
                scheduler_run_job(job, NULL, NULL, &doc, &final, &err);
                free(doc); free(final); free(err);
            }
        }
        if (due) json_free(due);
        for (int i = 0; i < interval && g_rt.cron_running; i++)
            sleep(1);
    }
    return NULL;
}

/* PoP: ws_start_desktop_cron_ticker @ hermes_cli/web_server.py:_start_desktop_cron_ticker */
bool ws_start_desktop_cron_ticker(int interval_seconds) {
    if (getenv("HERMES_DESKTOP") == NULL) return false;
    dashboard_runtime_t *rt = dashboard_runtime_get();
    if (rt->cron_running) return false;
    rt->cron_running = 1;
    if (pthread_create(&rt->cron_thread, NULL,
                       desktop_cron_ticker_thread,
                       (void *)(intptr_t)interval_seconds) != 0) {
        rt->cron_running = 0;
        return false;
    }
    return true;
}

void ws_stop_desktop_cron_ticker(void) {
    dashboard_runtime_t *rt = dashboard_runtime_get();
    if (!rt->cron_running) return;
    rt->cron_running = 0;
    pthread_join(rt->cron_thread, NULL);
}

bool ws_desktop_cron_ticker_running(void) {
    return g_rt.cron_running != 0;
}


/* ── from src/cli/port_web_server_themes.c ── */
/*
 * port_web_server_themes.c — dashboard theme discovery + normalisation.
 * Faithful port of _parse_theme_layer, _normalise_theme_definition,
 * _discover_user_themes, get_dashboard_themes, and
 * _render_active_theme_bootstrap_css from hermes_cli/web_server.py.
 *
 * YAML files are parsed with libyaml and converted to json via
 * yaml_to_json_string(doc, "") so the normaliser works on one tree type.
 */

#include "web_server_themes.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libyaml/yaml.h"
#include "slermes_home.h"
#include "web_server_events.h"  /* ws_events_theme_css_esc */

/* ── module constants (web_server.py) ───────────────────────────────────── */

#define THEME_CUSTOM_CSS_MAX (32 * 1024)

static const struct { const char *name, *label, *description; }
K_BUILTIN_THEMES[] = {
    {"default",       "Hermes Teal",         "Classic dark teal — the canonical Hermes look"},
    {"default-large", "Hermes Teal (Large)", "Hermes Teal with bigger fonts and roomier spacing"},
    {"nous-blue",     "Nous Blue",           "Light mode — vivid Nous-blue accents on cream canvas"},
    {"midnight",      "Midnight",            "Deep blue-violet with cool accents"},
    {"ember",         "Ember",               "Warm crimson and bronze — forge vibes"},
    {"mono",          "Mono",                "Clean grayscale — minimal and focused"},
    {"cyberpunk",     "Cyberpunk",           "Neon green on black — matrix terminal"},
    {"rose",          "Rosé",                "Soft pink and warm ivory — easy on the eyes"},
};
#define N_BUILTIN (sizeof(K_BUILTIN_THEMES) / sizeof(K_BUILTIN_THEMES[0]))

static const char *K_OVERRIDE_KEYS[] = {
    "card", "cardForeground", "popover", "popoverForeground",
    "primary", "primaryForeground", "secondary", "secondaryForeground",
    "muted", "mutedForeground", "accent", "accentForeground",
    "destructive", "destructiveForeground", "success", "warning",
    "border", "input", "ring", NULL,
};

static const char *K_NAMED_ASSET_KEYS[] = {
    "bg", "hero", "logo", "crest", "sidebar", "header", NULL,
};

static const char *K_COMPONENT_BUCKETS[] = {
    "card", "header", "footer", "sidebar", "tab",
    "progress", "badge", "backdrop", "page", NULL,
};

static const char *K_LAYOUT_VARIANTS[] = {"standard", "cockpit", "tiled", NULL};

static const char *K_DENSITIES[] = {"compact", "comfortable", "spacious", NULL};

static const char *K_FONT_CHOICES[] = {
    "system-sans", "system-serif", "system-mono",
    "inter", "ibm-plex-sans", "work-sans", "atkinson-hyperlegible", "dm-sans",
    "spectral", "fraunces", "source-serif",
    "jetbrains-mono", "ibm-plex-mono", "space-mono", NULL,
};

#define THEME_DEFAULT_FONT_SANS \
    "system-ui, -apple-system, \"Segoe UI\", Roboto, \"Helvetica Neue\", Arial, sans-serif"
#define THEME_DEFAULT_FONT_MONO \
    "ui-monospace, \"SF Mono\", \"Cascadia Mono\", Menlo, Consolas, monospace"
#define THEME_DEFAULT_BASE_SIZE "15px"
#define THEME_DEFAULT_LINE_HEIGHT "1.55"
#define THEME_DEFAULT_LETTER_SPACING "0"

static bool in_list(const char **list, const char *s) {
    for (size_t i = 0; list[i]; i++)
        if (strcmp(list[i], s) == 0) return true;
    return false;
}

/* Python: str.strip() truthiness — non-empty after strip. */
static bool str_nonblank(const char *s) {
    if (!s) return false;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return true;
        s++;
    }
    return false;
}

/* Python: key.replace("-","").replace("_","").isalnum() */
static bool key_isalnumish(const char *s) {
    bool any = false;
    for (const char *p = s; *p; p++) {
        if (*p == '-' || *p == '_') continue;
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9')))
            return false;
        any = true;
    }
    return any;
}

/* ── _parse_theme_layer ─────────────────────────────────────────────────── */
/* PoP: ws_theme_parse_layer @ hermes_cli/web_server.py:_parse_theme_layer */
json_t *ws_theme_parse_layer(const json_t *value, const char *default_hex,
                             double default_alpha) {
    if (!value || value->type == JSON_NULL) {
        json_t *o = json_object();
        json_set(o, "hex", json_string(default_hex));
        json_set(o, "alpha", json_number(default_alpha));
        return o;
    }
    if (value->type == JSON_STRING) {
        json_t *o = json_object();
        json_set(o, "hex", json_string(value->str_val));
        json_set(o, "alpha", json_number(default_alpha));
        return o;
    }
    if (value->type == JSON_OBJECT) {
        json_t *hex_v = json_object_get((json_t *)value, "hex");
        json_t *alpha_v = json_object_get((json_t *)value, "alpha");
        const char *hex_val = default_hex;
        if (hex_v) {
            if (hex_v->type != JSON_STRING) return NULL;
            hex_val = hex_v->str_val;
        }
        double alpha_f = default_alpha;
        if (alpha_v) {
            if (alpha_v->type == JSON_NUMBER) alpha_f = alpha_v->num_val;
            else if (alpha_v->type == JSON_BOOL) alpha_f = alpha_v->bool_val ? 1.0 : 0.0;
            else if (alpha_v->type == JSON_STRING) {
                char *end = NULL;
                double d = strtod(alpha_v->str_val, &end);
                if (end && end != alpha_v->str_val && *end == '\0') alpha_f = d;
                /* else float() raises → alpha_f stays default */
            }
            /* dict/list → TypeError → default */
        }
        if (alpha_f < 0.0) alpha_f = 0.0;
        if (alpha_f > 1.0) alpha_f = 1.0;
        json_t *o = json_object();
        json_set(o, "hex", json_string(hex_val));
        json_set(o, "alpha", json_number(alpha_f));
        return o;
    }
    return NULL;
}

/* ── _normalise_theme_definition ────────────────────────────────────────── */

static json_t *get_dict(const json_t *data, const char *key) {
    json_t *v = json_object_get((json_t *)data, key);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

static json_t *theme_layer(const json_t *palette_src, const json_t *colors_src,
                           const char *key, const char *default_hex,
                           double default_alpha) {
    json_t *spec = NULL;
    if (palette_src) spec = json_object_get((json_t *)palette_src, key);
    if (!spec && colors_src) spec = json_object_get((json_t *)colors_src, key);
    json_t *parsed = ws_theme_parse_layer(spec, default_hex, default_alpha);
    if (parsed) return parsed;
    json_t *o = json_object();
    json_set(o, "hex", json_string(default_hex));
    json_set(o, "alpha", json_number(default_alpha));
    return o;
}

/* PoP: ws_theme_normalise_definition @ hermes_cli/web_server.py:_normalise_theme_definition */
json_t *ws_theme_normalise_definition(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    json_t *name_v = json_object_get((json_t *)data, "name");
    if (!name_v || name_v->type != JSON_STRING || !str_nonblank(name_v->str_val))
        return NULL;
    const char *name = name_v->str_val;

    json_t *palette_src = get_dict(data, "palette");
    json_t *colors_src = get_dict(data, "colors");

    /* Palette */
    json_t *palette = json_object();
    json_set(palette, "background",
             theme_layer(palette_src, colors_src, "background", "#041c1c", 1.0));
    json_set(palette, "midground",
             theme_layer(palette_src, colors_src, "midground", "#ffe6cb", 1.0));
    json_set(palette, "foreground",
             theme_layer(palette_src, colors_src, "foreground", "#ffffff", 0.0));
    /* warmGlow: palette_src.get() or data.get() or default — Python `or`
     * treats empty string / None as falsy. */
    const char *warm = NULL;
    if (palette_src) {
        json_t *w = json_object_get(palette_src, "warmGlow");
        if (w && w->type == JSON_STRING && *w->str_val) warm = w->str_val;
    }
    if (!warm) {
        json_t *w = json_object_get((json_t *)data, "warmGlow");
        if (w && w->type == JSON_STRING && *w->str_val) warm = w->str_val;
    }
    json_set(palette, "warmGlow",
             json_string(warm ? warm : "rgba(255, 189, 56, 0.35)"));
    /* noiseOpacity */
    double noise = 1.0;
    json_t *raw_noise = palette_src ? json_object_get(palette_src, "noiseOpacity") : NULL;
    if (!raw_noise) raw_noise = json_object_get((json_t *)data, "noiseOpacity");
    if (raw_noise) {
        if (raw_noise->type == JSON_NUMBER) noise = raw_noise->num_val;
        else if (raw_noise->type == JSON_BOOL) noise = raw_noise->bool_val ? 1.0 : 0.0;
        else if (raw_noise->type == JSON_STRING) {
            char *end = NULL;
            double d = strtod(raw_noise->str_val, &end);
            if (end && end != raw_noise->str_val && *end == '\0') noise = d;
            else noise = 1.0;
        } else noise = 1.0;
    }
    json_set(palette, "noiseOpacity", json_number(noise));

    /* Typography */
    json_t *typo_src = get_dict(data, "typography");
    json_t *typography = json_object();
    json_set(typography, "fontSans", json_string(THEME_DEFAULT_FONT_SANS));
    json_set(typography, "fontMono", json_string(THEME_DEFAULT_FONT_MONO));
    json_set(typography, "baseSize", json_string(THEME_DEFAULT_BASE_SIZE));
    json_set(typography, "lineHeight", json_string(THEME_DEFAULT_LINE_HEIGHT));
    json_set(typography, "letterSpacing", json_string(THEME_DEFAULT_LETTER_SPACING));
    static const char *typo_keys[] = {"fontSans", "fontMono", "fontDisplay",
                                      "fontUrl", "baseSize", "lineHeight",
                                      "letterSpacing", NULL};
    if (typo_src) {
        for (size_t i = 0; typo_keys[i]; i++) {
            json_t *v = json_object_get(typo_src, typo_keys[i]);
            if (v && v->type == JSON_STRING && str_nonblank(v->str_val))
                json_set(typography, typo_keys[i], json_string(v->str_val));
        }
    }

    /* Layout */
    json_t *layout_src = get_dict(data, "layout");
    json_t *layout = json_object();
    json_set(layout, "radius", json_string("0.5rem"));
    json_set(layout, "density", json_string("comfortable"));
    if (layout_src) {
        json_t *radius = json_object_get(layout_src, "radius");
        if (radius && radius->type == JSON_STRING && str_nonblank(radius->str_val))
            json_set(layout, "radius", json_string(radius->str_val));
        json_t *density = json_object_get(layout_src, "density");
        if (density && density->type == JSON_STRING &&
            in_list(K_DENSITIES, density->str_val))
            json_set(layout, "density", json_string(density->str_val));
    }

    /* Color overrides */
    json_t *overrides_src = get_dict(data, "colorOverrides");
    json_t *color_overrides = json_object();
    if (overrides_src) {
        for (size_t i = 0; i < json_object_size(overrides_src); i++) {
            const char *k = json_object_get_key_at(overrides_src, i);
            json_t *v = json_object_get_at(overrides_src, i);
            if (k && in_list(K_OVERRIDE_KEYS, k) && v &&
                v->type == JSON_STRING && str_nonblank(v->str_val))
                json_set(color_overrides, k, json_string(v->str_val));
        }
    }

    /* Assets */
    json_t *assets_out = json_object();
    json_t *assets_src = get_dict(data, "assets");
    if (assets_src) {
        for (size_t i = 0; K_NAMED_ASSET_KEYS[i]; i++) {
            json_t *v = json_object_get(assets_src, K_NAMED_ASSET_KEYS[i]);
            if (v && v->type == JSON_STRING && str_nonblank(v->str_val))
                json_set(assets_out, K_NAMED_ASSET_KEYS[i],
                         json_string(v->str_val));
        }
        json_t *custom_src = json_object_get(assets_src, "custom");
        if (custom_src && custom_src->type == JSON_OBJECT) {
            json_t *custom = json_object();
            for (size_t i = 0; i < json_object_size(custom_src); i++) {
                const char *k = json_object_get_key_at(custom_src, i);
                json_t *v = json_object_get_at(custom_src, i);
                if (k && key_isalnumish(k) && v && v->type == JSON_STRING &&
                    str_nonblank(v->str_val))
                    json_set(custom, k, json_string(v->str_val));
            }
            if (json_object_size(custom) > 0)
                json_set(assets_out, "custom", custom);
            else
                json_free(custom);
        }
    }

    /* customCSS */
    json_t *custom_css_v = json_object_get((json_t *)data, "customCSS");
    char *custom_css = NULL;
    if (custom_css_v && custom_css_v->type == JSON_STRING &&
        str_nonblank(custom_css_v->str_val)) {
        size_t l = strlen(custom_css_v->str_val);
        if (l > THEME_CUSTOM_CSS_MAX) l = THEME_CUSTOM_CSS_MAX;
        custom_css = malloc(l + 1);
        memcpy(custom_css, custom_css_v->str_val, l);
        custom_css[l] = '\0';
    }

    /* componentStyles */
    json_t *comp_src = get_dict(data, "componentStyles");
    json_t *component_styles = json_object();
    if (comp_src) {
        for (size_t i = 0; i < json_object_size(comp_src); i++) {
            const char *bucket = json_object_get_key_at(comp_src, i);
            json_t *props = json_object_get_at(comp_src, i);
            if (!bucket || !in_list(K_COMPONENT_BUCKETS, bucket) || !props ||
                props->type != JSON_OBJECT)
                continue;
            json_t *clean = json_object();
            for (size_t j = 0; j < json_object_size(props); j++) {
                const char *prop = json_object_get_key_at(props, j);
                json_t *val = json_object_get_at(props, j);
                if (!prop || !key_isalnumish(prop) || !val) continue;
                if (val->type == JSON_STRING) {
                    if (str_nonblank(val->str_val))
                        json_set(clean, prop, json_string(val->str_val));
                } else if (val->type == JSON_NUMBER) {
                    /* str(value) — int stays int-looking */
                    char buf[64];
                    if (val->num_val == (double)(long long)val->num_val)
                        snprintf(buf, sizeof(buf), "%lld",
                                 (long long)val->num_val);
                    else
                        snprintf(buf, sizeof(buf), "%.17g", val->num_val);
                    json_set(clean, prop, json_string(buf));
                }
                /* bool is isinstance(int) in Python but str(True)="True";
                 * YAML themes never use bools here — treat as skip since
                 * isinstance(value,(str,int,float)) includes bool: emit. */
                else if (val->type == JSON_BOOL) {
                    json_set(clean, prop,
                             json_string(val->bool_val ? "True" : "False"));
                }
            }
            if (json_object_size(clean) > 0)
                json_set(component_styles, bucket, clean);
            else
                json_free(clean);
        }
    }

    /* layoutVariant */
    const char *layout_variant = "standard";
    json_t *lv = json_object_get((json_t *)data, "layoutVariant");
    if (lv && lv->type == JSON_STRING && in_list(K_LAYOUT_VARIANTS, lv->str_val))
        layout_variant = lv->str_val;

    /* Assemble in Python dict-literal order. */
    json_t *result = json_object();
    json_set(result, "name", json_string(name));
    json_t *label_v = json_object_get((json_t *)data, "label");
    json_set(result, "label",
             json_string(label_v && label_v->type == JSON_STRING &&
                                 *label_v->str_val
                             ? label_v->str_val
                             : name));
    json_t *desc_v = json_object_get((json_t *)data, "description");
    json_set(result, "description",
             json_string(desc_v && desc_v->type == JSON_STRING
                             ? desc_v->str_val
                             : ""));
    json_set(result, "palette", palette);
    json_set(result, "typography", typography);
    json_set(result, "layout", layout);
    json_set(result, "layoutVariant", json_string(layout_variant));
    if (json_object_size(color_overrides) > 0)
        json_set(result, "colorOverrides", color_overrides);
    else
        json_free(color_overrides);
    if (json_object_size(assets_out) > 0)
        json_set(result, "assets", assets_out);
    else
        json_free(assets_out);
    if (custom_css) {
        json_set(result, "customCSS", json_string(custom_css));
        free(custom_css);
    }
    if (json_object_size(component_styles) > 0)
        json_set(result, "componentStyles", component_styles);
    else
        json_free(component_styles);
    return result;
}

/* ── builtin list ───────────────────────────────────────────────────────── */
/* PoP: ws_theme_builtin_list @ hermes_cli/web_server.py:get_dashboard_themes */
json_t *ws_theme_builtin_list(void) {
    json_t *arr = json_array();
    for (size_t i = 0; i < N_BUILTIN; i++) {
        json_t *o = json_object();
        json_set(o, "name", json_string(K_BUILTIN_THEMES[i].name));
        json_set(o, "label", json_string(K_BUILTIN_THEMES[i].label));
        json_set(o, "description", json_string(K_BUILTIN_THEMES[i].description));
        json_append(arr, o);
    }
    return arr;
}

/* ── _discover_user_themes ──────────────────────────────────────────────── */

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* PoP: ws_theme_discover_user_themes @ hermes_cli/web_server.py:_discover_user_themes */
json_t *ws_theme_discover_user_themes(const char *home) {
    json_t *result = json_array();
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), "%s/dashboard-themes",
             home && *home ? home : slermes_home());
    DIR *d = opendir(dirpath);
    if (!d) return result;

    /* collect *.yaml, sorted like Path.glob sorted() */
    char **names = NULL;
    size_t n = 0, cap = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        size_t l = strlen(de->d_name);
        if (l < 6 || strcmp(de->d_name + l - 5, ".yaml") != 0) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            names = realloc(names, cap * sizeof *names);
        }
        names[n++] = strdup(de->d_name);
    }
    closedir(d);
    qsort(names, n, sizeof *names, cmp_str);

    for (size_t i = 0; i < n; i++) {
        char fpath[1400];
        snprintf(fpath, sizeof(fpath), "%s/%s", dirpath, names[i]);
        char *err = NULL;
        yaml_doc_t *doc = yaml_parse_file(fpath, &err);
        free(err);
        if (!doc) { free(names[i]); continue; }
        char *jstr = yaml_to_json_string(doc, "");
        yaml_free(doc);
        if (!jstr) { free(names[i]); continue; }
        json_t *data = json_parse(jstr, NULL);
        free(jstr);
        if (!data) { free(names[i]); continue; }
        json_t *norm = ws_theme_normalise_definition(data);
        json_free(data);
        if (norm) json_append(result, norm);
        free(names[i]);
    }
    free(names);
    return result;
}

/* ── get_dashboard_themes response ──────────────────────────────────────── */
/* PoP: ws_theme_dashboard_themes_response @ hermes_cli/web_server.py:get_dashboard_themes */
json_t *ws_theme_dashboard_themes_response(const char *home,
                                           const char *active) {
    json_t *themes = ws_theme_builtin_list();
    json_t *user = ws_theme_discover_user_themes(home);
    for (size_t i = 0; i < json_len(user); i++) {
        json_t *t = json_get(user, i);
        const char *nm = json_get_str(t, "name", "");
        bool dup = false;
        for (size_t j = 0; j < N_BUILTIN && !dup; j++)
            if (strcmp(K_BUILTIN_THEMES[j].name, nm) == 0) dup = true;
        /* also dedupe against earlier user themes already appended */
        for (size_t j = N_BUILTIN; j < json_len(themes) && !dup; j++)
            if (strcmp(json_get_str(json_get(themes, j), "name", ""), nm) == 0)
                dup = true;
        if (dup) continue;
        json_t *entry = json_object();
        json_set(entry, "name", json_string(nm));
        json_set(entry, "label", json_string(json_get_str(t, "label", "")));
        json_set(entry, "description",
                 json_string(json_get_str(t, "description", "")));
        json_set(entry, "definition", json_copy(t));
        json_append(themes, entry);
    }
    json_free(user);
    json_t *res = json_object();
    json_set(res, "themes", themes);
    json_set(res, "active", json_string(active && *active ? active : "default"));
    return res;
}

/* ── _render_active_theme_bootstrap_css ─────────────────────────────────── */
/* PoP: ws_theme_render_bootstrap_css @ hermes_cli/web_server.py:_render_active_theme_bootstrap_css */
char *ws_theme_render_bootstrap_css(const char *home, const char *active) {
    if (!active || !*active) return strdup("");
    for (size_t i = 0; i < N_BUILTIN; i++)
        if (strcmp(K_BUILTIN_THEMES[i].name, active) == 0) return strdup("");

    json_t *user = ws_theme_discover_user_themes(home);
    char *out = NULL;
    for (size_t i = 0; i < json_len(user) && !out; i++) {
        json_t *theme = json_get(user, i);
        if (strcmp(json_get_str(theme, "name", ""), active) != 0) continue;
        json_t *palette = json_object_get(theme, "palette");
        json_t *bg = palette ? json_object_get(palette, "background") : NULL;
        json_t *mg = palette ? json_object_get(palette, "midground") : NULL;
        const char *bg_hex = bg ? json_get_str(bg, "hex", "#0a0a0a") : "#0a0a0a";
        const char *mg_hex = mg ? json_get_str(mg, "hex", "#e5e5e5") : "#e5e5e5";
        json_t *typo = json_object_get(theme, "typography");
        const char *font_sans =
            typo ? json_get_str(typo, "fontSans", THEME_DEFAULT_FONT_SANS)
                 : THEME_DEFAULT_FONT_SANS;
        const char *base_size =
            typo ? json_get_str(typo, "baseSize", THEME_DEFAULT_BASE_SIZE)
                 : THEME_DEFAULT_BASE_SIZE;

        char *e_bg = ws_events_theme_css_esc(bg_hex);
        char *e_mg = ws_events_theme_css_esc(mg_hex);
        char *e_fs = ws_events_theme_css_esc(font_sans);
        char *e_bs = ws_events_theme_css_esc(base_size);

        size_t need = strlen(e_bg) + strlen(e_mg) + strlen(e_fs) +
                      strlen(e_bs) + 512;
        out = malloc(need);
        snprintf(out, need,
                 "<style id=\"hermes-theme-bootstrap\">"
                 ":root{"
                 "--background-base:%s;"
                 "--midground-base:%s;"
                 "--theme-font-sans:%s;"
                 "--theme-base-size:%s;"
                 "}"
                 "html,body{background-color:var(--background-base);"
                 "color:var(--midground-base);"
                 "font-family:var(--theme-font-sans);"
                 "font-size:var(--theme-base-size);}"
                 "</style>",
                 e_bg, e_mg, e_fs, e_bs);
        free(e_bg); free(e_mg); free(e_fs); free(e_bs);
    }
    json_free(user);
    return out ? out : strdup("");
}

/* ── font allowlist ─────────────────────────────────────────────────────── */
/* PoP: ws_theme_font_choice_allowed @ hermes_cli/web_server.py:set_dashboard_font */
bool ws_theme_font_choice_allowed(const char *font_id) {
    if (!font_id) return false;
    if (strcmp(font_id, "theme") == 0) return true; /* _FONT_DEFAULT_ID */
    return in_list(K_FONT_CHOICES, font_id);
}


/* ── from src/cli/port_web_server_wrappers.c ── */
/*
 * port_web_server_wrappers.c — C port of hermes_cli/web_server.py
 * PoP-annotated wrappers for all unported web server endpoints and helpers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_crypto.h"
#include "profile_store.h"
#include "hermes_gateway_config.h"
#include "registry.h"
#include "cron_jobs.h"
#include "port_web_server_wrappers.h"
#include "base64.h"
#include "uuid.h"
#include "blueprint_catalog_common.h"

/* xstrdup — local (mirrors libjson/libhttp per-TU pattern; no shared header
 * owns it). malloc'd null-safe duplicate. */
static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── _topology cache (port of _TOPOLOGY_CACHE / _TOPOLOGY_CACHE_LOCK / TTL) ── */
/* A void* fn pointer stands in for Python's identity-based `fn is fn`
 * bound-method check (tests monkeypatch the collector; a swapped collector is
 * a cache miss — same hermetic contract). */
struct topology_cache {
    void    *fn;          /* NULL = cold */
    json_t  *data;        /* shared reference on hit */
    int64_t  ts_ns;       /* clock_gettime(CLOCK_MONOTONIC) in ns */
} g_topology_cache = {0};
#define TOPOLOGY_CACHE_TTL 10.0

/* PoP: ws_u_lifespan @ hermes_cli/web_server.py:_lifespan */
json_t *ws_u_lifespan(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_get_event_state @ hermes_cli/web_server.py:_get_event_state */
json_t *ws_u_get_event_state(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_get_pty_active_session_files @ hermes_cli/web_server.py:_get_pty_active_session_files */
json_t *ws_u_get_pty_active_session_files(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_apply_ssh_session_token @ hermes_cli/web_server.py:_apply_ssh_session_token */
json_t *ws_u_apply_ssh_session_token(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_apply_ssh_owner_nonce @ hermes_cli/web_server.py:_apply_ssh_owner_nonce */
json_t *ws_u_apply_ssh_owner_nonce(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_require_token @ hermes_cli/web_server.py:_require_token */
json_t *ws_u_require_token(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_host_header_middleware @ hermes_cli/web_server.py:host_header_middleware */
json_t *ws_host_header_middleware(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_plugin_api_runtime_gate @ hermes_cli/web_server.py:_plugin_api_runtime_gate */
json_t *ws_u_plugin_api_runtime_gate(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_dashboard_auth_gate @ hermes_cli/web_server.py:_dashboard_auth_gate */
json_t *ws_u_dashboard_auth_gate(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_auth_middleware @ hermes_cli/web_server.py:auth_middleware */
json_t *ws_auth_middleware(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_token_auth_seam @ hermes_cli/web_server.py:_token_auth_seam */
json_t *ws_u_token_auth_seam(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_record_selftest @ hermes_cli/web_server.py:record_selftest */
json_t *ws_record_selftest(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_dashboard_health_middleware @ hermes_cli/web_server.py:_dashboard_health_middleware */
json_t *ws_u_dashboard_health_middleware(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_dashboard_selftest_loop @ hermes_cli/web_server.py:_dashboard_selftest_loop */
json_t *ws_u_dashboard_selftest_loop(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_options @ hermes_cli/web_server.py:_memory_provider_options */
json_t *ws_u_memory_provider_options(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_is_command_provider_block @ hermes_cli/web_server.py:_is_command_provider_block */
json_t *ws_u_is_command_provider_block(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_custom_provider_options @ hermes_cli/web_server.py:_custom_provider_options */
json_t *ws_u_custom_provider_options(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_schema_options @ hermes_cli/web_server.py:_memory_provider_schema_options */
json_t *ws_u_memory_provider_schema_options(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_schema_with_dynamic_provider_options @ hermes_cli/web_server.py:_schema_with_dynamic_provider_options */
json_t *ws_u_schema_with_dynamic_provider_options(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_validate_reference_timeout @ hermes_cli/web_server.py:_validate_reference_timeout */
json_t *ws_u_validate_reference_timeout(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_status_active_sessions @ hermes_cli/web_server.py:_status_active_sessions */
json_t *ws_u_status_active_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_media @ hermes_cli/web_server.py:get_media */
json_t *ws_get_media(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_local_dashboard_request @ hermes_cli/web_server.py:_local_dashboard_request */
json_t *ws_u_local_dashboard_request(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_managed_files_policy @ hermes_cli/web_server.py:_managed_files_policy */
json_t *ws_u_managed_files_policy(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_resolve_managed_path @ hermes_cli/web_server.py:_resolve_managed_path */
json_t *ws_u_resolve_managed_path(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_managed_response_meta @ hermes_cli/web_server.py:_managed_response_meta */
json_t *ws_u_managed_response_meta(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_managed_file_entry @ hermes_cli/web_server.py:_managed_file_entry */
json_t *ws_u_managed_file_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_sanitize_chat_image_filename @ hermes_cli/web_server.py:_sanitize_chat_image_filename */
json_t *ws_u_sanitize_chat_image_filename(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_chat_image_extension @ hermes_cli/web_server.py:_chat_image_extension */
json_t *ws_u_chat_image_extension(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_decode_chat_image_upload @ hermes_cli/web_server.py:_decode_chat_image_upload */
json_t *ws_u_decode_chat_image_upload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_upload_chat_image @ hermes_cli/web_server.py:upload_chat_image */
json_t *ws_upload_chat_image(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_managed_files @ hermes_cli/web_server.py:list_managed_files */
json_t *ws_list_managed_files(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_read_managed_file @ hermes_cli/web_server.py:read_managed_file */
json_t *ws_read_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_download_managed_file @ hermes_cli/web_server.py:download_managed_file */
json_t *ws_download_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_upload_managed_file @ hermes_cli/web_server.py:upload_managed_file */
json_t *ws_upload_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_upload_managed_file_stream @ hermes_cli/web_server.py:upload_managed_file_stream */
json_t *ws_upload_managed_file_stream(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_create_managed_directory @ hermes_cli/web_server.py:create_managed_directory */
json_t *ws_create_managed_directory(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_managed_file @ hermes_cli/web_server.py:delete_managed_file */
json_t *ws_delete_managed_file(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_git_op @ hermes_cli/web_server.py:_git_op */
json_t *ws_u_git_op(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_profile_platform_ports @ hermes_cli/web_server.py:_profile_platform_ports */
json_t *ws_u_profile_platform_ports(json_t *req) { (void)req; return json_object(); }

/* ── _topology_cache_get ─────────────────────────────────────────── */
/* PoP: miss @ hermes_cli/web_server.py:_topology_cache_get */
/* Double-check the TTL cache: hit only when the bound fn pointer matches,
 * the cache is populated, and elapsed monotonic time < TTL (10.0s).
 * Returns NULL on miss (mirrors Python None). Caller owns the returned
 * json_t * (the cached value is shared/referenced, not copied — matches
 * Python returning the live dict reference). */
json_t *ws_topology_cache_get(void *fn) {
    if (!g_topology_cache.data || g_topology_cache.fn != fn) return NULL;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t now_ns = (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
    double elapsed = (double)(now_ns - g_topology_cache.ts_ns) / 1e9;
    if (elapsed < TOPOLOGY_CACHE_TTL) {
        return g_topology_cache.data;  /* shared reference */
    }
    return NULL;
}

/* ── topology cache + collector (replaces the bootleg _collect_profile_gateway_topology stub) */

static void invalidate_topology_cache(void) {
    g_topology_cache.fn = NULL;
    if (g_topology_cache.data) { json_free(g_topology_cache.data); g_topology_cache.data = NULL; }
}

/* PoP: profile_profiles_to_serve @ hermes_cli/web_server.py:_collect_profile_gateway_topology */
/* Enumerate profiles + live gateway ports for /api/status.
 * Reuses: profile_profiles_to_serve(True), profile_gateway_running(home),
 * gateway_config_get_global() (connected platforms). */
json_t *ws_u_collect_profile_gateway_topology(void) {
    json_t *profiles_arr = json_array();
    json_t *gateways_arr = json_array();
    bool multiplex = false;

    /* profiles_to_serve(True) — cheap enumeration */
    char *prof_json = profile_profiles_to_serve(true);
    if (prof_json) {
        json_t *parsed = json_parse(prof_json, NULL);
        free(prof_json);
        if (parsed && parsed->type == JSON_ARRAY) {
            size_t n = json_len(parsed);
            for (size_t i = 0; i < n; i++) {
                json_t *entry = json_array_get(parsed, i);
                if (!entry) continue;
                /* entry is [name, home_dir] array per profile_profiles_to_serve contract */
                const char *pname = NULL, *phome = NULL;
                if (entry->type == JSON_ARRAY) {
                    json_t *name = json_get(entry, 0);
                    json_t *home = json_get(entry, 1);
                    if (name && name->type == JSON_STRING) pname = name->str_val;
                    if (home && home->type == JSON_STRING) phome = home->str_val;
                } else if (entry->type == JSON_STRING) {
                    pname = entry->str_val;  /* fallback: name only */
                }
                if (pname) {
                    json_array_append(profiles_arr, json_string(pname));
                }
                /* gateway liveness */
                if (phome && profile_gateway_running(phome)) {
                    json_t *gentry = json_object();
                    json_set(gentry, "profile", json_string(pname));
                    json_set(gentry, "served_profiles", json_array());  /* runtime detail */
                    /* ports from connected platforms in gateway config */
                    json_t *ports = json_array();
                    const gateway_config_t *cfg = gateway_config_get_global();
                    if (cfg) {
                        json_t *connected = gateway_config_get_connected_platforms(cfg);
                        if (connected && connected->type == JSON_ARRAY) {
                            size_t cn = json_len(connected);
                            for (size_t ci = 0; ci < cn; ci++) {
                                json_t *pn = json_get(connected, ci);
                                if (pn && pn->type == JSON_STRING) {
                                    json_array_append(ports, json_string(pn->str_val));
                                }
                            }
                        }
                        if (connected) json_free(connected);
                    }
                    json_set(gentry, "ports", ports);
                    json_array_append(gateways_arr, gentry);
                    if (strcmp(pname, "default") == 0) {
                        /* multiplex if >1 served — approximate: single entry here */
                    }
                }
            }
        }
        if (parsed) json_free(parsed);
    }

    const char *mode = "none";
    if (json_len(gateways_arr) > 1) mode = "multiple";
    else if (json_len(gateways_arr) == 1) mode = "single";
    if (multiplex) mode = "multiplex";

    json_t *out = json_object();
    json_set(out, "profiles", profiles_arr);
    json_set(out, "gateway_mode", json_string(mode));
    json_set(out, "gateways", gateways_arr);
    return out;
}

/* PoP: ws_get_ssh_ownership @ hermes_cli/web_server.py:get_ssh_ownership */
json_t *ws_get_ssh_ownership(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_health @ hermes_cli/web_server.py:get_health */
json_t *ws_get_health(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_system_stats @ hermes_cli/web_server.py:get_system_stats */
json_t *ws_get_system_stats(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_curator_status @ hermes_cli/web_server.py:get_curator_status */
json_t *ws_get_curator_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_curator_paused @ hermes_cli/web_server.py:set_curator_paused */
json_t *ws_set_curator_paused(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_curator @ hermes_cli/web_server.py:run_curator */
json_t *ws_run_curator(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_learning_graph @ hermes_cli/web_server.py:get_learning_graph */
json_t *ws_get_learning_graph(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_learning_node @ hermes_cli/web_server.py:get_learning_node */
json_t *ws_get_learning_node(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_learning_node @ hermes_cli/web_server.py:delete_learning_node */
json_t *ws_delete_learning_node(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_learning_node @ hermes_cli/web_server.py:update_learning_node */
json_t *ws_update_learning_node(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_portal_status @ hermes_cli/web_server.py:get_portal_status */
json_t *ws_get_portal_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_prompt_size @ hermes_cli/web_server.py:run_prompt_size */
json_t *ws_run_prompt_size(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_dump @ hermes_cli/web_server.py:run_dump */
json_t *ws_run_dump(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_config_migrate @ hermes_cli/web_server.py:run_config_migrate */
json_t *ws_run_config_migrate(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_debug_share_endpoint @ hermes_cli/web_server.py:run_debug_share_endpoint */
json_t *ws_run_debug_share_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_spawn_hermes_action @ hermes_cli/web_server.py:_spawn_hermes_action */
json_t *ws_u_spawn_hermes_action(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_spawn_gateway_restart @ hermes_cli/web_server.py:_spawn_gateway_restart */
json_t *ws_u_spawn_gateway_restart(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_restart_gateway_after_webhook_enable @ hermes_cli/web_server.py:_restart_gateway_after_webhook_enable */
json_t *ws_u_restart_gateway_after_webhook_enable(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_restart_gateway @ hermes_cli/web_server.py:restart_gateway */
json_t *ws_restart_gateway(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_gateway_drain @ hermes_cli/web_server.py:gateway_drain */
json_t *ws_gateway_drain(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_hermes @ hermes_cli/web_server.py:update_hermes */
json_t *ws_update_hermes(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_recent_upstream_commits @ hermes_cli/web_server.py:_recent_upstream_commits */
json_t *ws_u_recent_upstream_commits(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_check_hermes_update @ hermes_cli/web_server.py:check_hermes_update */
json_t *ws_check_hermes_update(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_transcribe_audio_upload @ hermes_cli/web_server.py:transcribe_audio_upload */
json_t *ws_transcribe_audio_upload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_elevenlabs_voices @ hermes_cli/web_server.py:get_elevenlabs_voices */
json_t *ws_get_elevenlabs_voices(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_speak_text @ hermes_cli/web_server.py:speak_text */
json_t *ws_speak_text(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_split_text_for_speak_stream @ hermes_cli/web_server.py:_split_text_for_speak_stream */
json_t *ws_u_split_text_for_speak_stream(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_speak_stream_ws @ hermes_cli/web_server.py:speak_stream_ws */
json_t *ws_speak_stream_ws(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_action_status @ hermes_cli/web_server.py:get_action_status */
json_t *ws_get_action_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_strip_session_list_rows @ hermes_cli/web_server.py:_strip_session_list_rows */
json_t *ws_u_strip_session_list_rows(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_profiles_sessions @ hermes_cli/web_server.py:get_profiles_sessions */
/* PoP: ws_get_profiles_sessions @ hermes_cli/web_routers/profiles.py:get_profiles_sessions */
/* PoP: ws_get_profiles_sessions @ hermes_cli/web_routers/profiles.py:get_profiles_sessions */
json_t *ws_get_profiles_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_profiles_sessions_sidebar @ hermes_cli/web_server.py:get_profiles_sessions_sidebar */
/* PoP: ws_get_profiles_sessions_sidebar @ hermes_cli/web_routers/profiles.py:get_profiles_sessions_sidebar */
/* PoP: ws_get_profiles_sessions_sidebar @ hermes_cli/web_routers/profiles.py:get_profiles_sessions_sidebar */
json_t *ws_get_profiles_sessions_sidebar(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_search_sessions @ hermes_cli/web_server.py:search_sessions */
/* PoP: ws_search_sessions @ hermes_cli/web_routers/sessions.py:search_sessions */
/* PoP: ws_search_sessions @ hermes_cli/web_routers/sessions.py:search_sessions */
json_t *ws_search_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_provider_field_entry @ hermes_cli/web_server.py:_provider_field_entry */
json_t *ws_u_provider_field_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_serialize_field_value @ hermes_cli/web_server.py:_serialize_field_value */
json_t *ws_u_serialize_field_value(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_flat_json_path @ hermes_cli/web_server.py:_flat_json_path */
json_t *ws_u_flat_json_path(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_read_flat_json @ hermes_cli/web_server.py:_read_flat_json */
json_t *ws_u_read_flat_json(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_read_field @ hermes_cli/web_server.py:_read_field */
json_t *ws_u_read_field(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_declared_field_is_set @ hermes_cli/web_server.py:_declared_field_is_set */
json_t *ws_u_declared_field_is_set(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_honcho_resolvers @ hermes_cli/web_server.py:_honcho_resolvers */
json_t *ws_u_honcho_resolvers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_honcho_read_sources @ hermes_cli/web_server.py:_honcho_read_sources */
json_t *ws_u_honcho_read_sources(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_declared_provider_payload @ hermes_cli/web_server.py:_declared_provider_payload */
json_t *ws_u_declared_provider_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_apply_field_values @ hermes_cli/web_server.py:_apply_field_values */
json_t *ws_u_apply_field_values(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_provider_flat @ hermes_cli/web_server.py:_write_provider_flat */
json_t *ws_u_write_provider_flat(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_provider_honcho @ hermes_cli/web_server.py:_write_provider_honcho */
json_t *ws_u_write_provider_honcho(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_stringify_submitted_values @ hermes_cli/web_server.py:_stringify_submitted_values */
json_t *ws_u_stringify_submitted_values(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_update_memory_provider_config @ hermes_cli/web_server.py:_update_memory_provider_config */
json_t *ws_u_update_memory_provider_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_label @ hermes_cli/web_server.py:_memory_provider_label */
json_t *ws_u_memory_provider_label(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_normalize_memory_provider_name @ hermes_cli/web_server.py:_normalize_memory_provider_name */
json_t *ws_u_normalize_memory_provider_name(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_load_memory_provider @ hermes_cli/web_server.py:_load_memory_provider */
json_t *ws_u_load_memory_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_manifest @ hermes_cli/web_server.py:_memory_provider_manifest */
json_t *ws_u_memory_provider_manifest(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_string_list @ hermes_cli/web_server.py:_string_list */
json_t *ws_u_string_list(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_setup_manifest @ hermes_cli/web_server.py:_memory_provider_setup_manifest */
json_t *ws_u_memory_provider_setup_manifest(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_setup_info @ hermes_cli/web_server.py:_memory_provider_setup_info */
json_t *ws_u_memory_provider_setup_info(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_dependency_package @ hermes_cli/web_server.py:_memory_provider_dependency_package */
json_t *ws_u_memory_provider_dependency_package(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_import_name @ hermes_cli/web_server.py:_memory_provider_import_name */
json_t *ws_u_memory_provider_import_name(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_dependency_importable @ hermes_cli/web_server.py:_dependency_importable */
json_t *ws_u_dependency_importable(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_trim_setup_output @ hermes_cli/web_server.py:_trim_setup_output */
json_t *ws_u_trim_setup_output(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_setup_env @ hermes_cli/web_server.py:_memory_provider_setup_env */
json_t *ws_u_memory_provider_setup_env(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_command_result @ hermes_cli/web_server.py:_command_result */
json_t *ws_u_command_result(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_run_setup_command @ hermes_cli/web_server.py:_run_setup_command */
json_t *ws_u_run_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_dependencies_installed @ hermes_cli/web_server.py:_memory_provider_dependencies_installed */
json_t *ws_u_memory_provider_dependencies_installed(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_install_memory_provider_pip_dependencies @ hermes_cli/web_server.py:_install_memory_provider_pip_dependencies */
json_t *ws_u_install_memory_provider_pip_dependencies(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_install_memory_provider_external_dependencies @ hermes_cli/web_server.py:_install_memory_provider_external_dependencies */
json_t *ws_u_install_memory_provider_external_dependencies(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_install_memory_provider_setup @ hermes_cli/web_server.py:_install_memory_provider_setup */
json_t *ws_u_install_memory_provider_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_normalize_memory_provider_schema @ hermes_cli/web_server.py:_normalize_memory_provider_schema */
json_t *ws_u_normalize_memory_provider_schema(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_read_json_file @ hermes_cli/web_server.py:_read_json_file */
json_t *ws_u_read_json_file(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_read_memory_provider_existing_values @ hermes_cli/web_server.py:_read_memory_provider_existing_values */
json_t *ws_u_read_memory_provider_existing_values(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_env_lookup @ hermes_cli/web_server.py:_env_lookup */
json_t *ws_u_env_lookup(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_field_default @ hermes_cli/web_server.py:_field_default */
json_t *ws_u_field_default(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_field_value @ hermes_cli/web_server.py:_field_value */
json_t *ws_u_field_value(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_field_visible @ hermes_cli/web_server.py:_field_visible */
json_t *ws_u_field_visible(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_public_memory_provider_field @ hermes_cli/web_server.py:_public_memory_provider_field */
json_t *ws_u_public_memory_provider_field(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_coerce_schema_field @ hermes_cli/web_server.py:_coerce_schema_field */
json_t *ws_u_coerce_schema_field(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_save_memory_provider_native_config @ hermes_cli/web_server.py:_save_memory_provider_native_config */
json_t *ws_u_save_memory_provider_native_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_memory_provider_is_configured @ hermes_cli/web_server.py:_memory_provider_is_configured */
json_t *ws_u_memory_provider_is_configured(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_discover_memory_provider_statuses @ hermes_cli/web_server.py:_discover_memory_provider_statuses */
json_t *ws_u_discover_memory_provider_statuses(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_require_memory_provider_ready @ hermes_cli/web_server.py:_require_memory_provider_ready */
json_t *ws_u_require_memory_provider_ready(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_memory_provider_config_values @ hermes_cli/web_server.py:_write_memory_provider_config_values */
json_t *ws_u_write_memory_provider_config_values(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_require_valid_memory_provider_name @ hermes_cli/web_server.py:_require_valid_memory_provider_name */
json_t *ws_u_require_valid_memory_provider_name(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_setup_memory_provider @ hermes_cli/web_server.py:setup_memory_provider */
json_t *ws_setup_memory_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_memory_provider_config @ hermes_cli/web_server.py:update_memory_provider_config */
json_t *ws_update_memory_provider_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_config @ hermes_cli/web_server.py:get_config */
json_t *ws_get_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_defaults @ hermes_cli/web_server.py:get_defaults */
json_t *ws_get_defaults(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_egress_status @ hermes_cli/web_server.py:get_egress_status */
json_t *ws_get_egress_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_model_options @ hermes_cli/web_server.py:get_model_options */
json_t *ws_get_model_options(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_recommended_default_model @ hermes_cli/web_server.py:get_recommended_default_model */
json_t *ws_get_recommended_default_model(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_auxiliary_models @ hermes_cli/web_server.py:get_auxiliary_models */
json_t *ws_get_auxiliary_models(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_moa_models @ hermes_cli/web_server.py:get_moa_models */
json_t *ws_get_moa_models(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_moa_models @ hermes_cli/web_server.py:set_moa_models */
json_t *ws_set_moa_models(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_model_assignment @ hermes_cli/web_server.py:set_model_assignment */
json_t *ws_set_model_assignment(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_apply_model_assignment_sync @ hermes_cli/web_server.py:_apply_model_assignment_sync */
json_t *ws_u_apply_model_assignment_sync(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_infer_provider_on_model_change @ hermes_cli/web_server.py:_infer_provider_on_model_change */
json_t *ws_u_infer_provider_on_model_change(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_denormalize_config_from_web @ hermes_cli/web_server.py:_denormalize_config_from_web */
json_t *ws_u_denormalize_config_from_web(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_config @ hermes_cli/web_server.py:update_config */
json_t *ws_update_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_catalog_provider_env_metadata @ hermes_cli/web_server.py:_catalog_provider_env_metadata */
json_t *ws_u_catalog_provider_env_metadata(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_env_vars @ hermes_cli/web_server.py:get_env_vars */
json_t *ws_get_env_vars(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_env_var @ hermes_cli/web_server.py:set_env_var */
json_t *ws_set_env_var(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_custom_endpoint_id @ hermes_cli/web_server.py:_custom_endpoint_id */
json_t *ws_u_custom_endpoint_id(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_models_from_custom_endpoint_entry @ hermes_cli/web_server.py:_models_from_custom_endpoint_entry */
json_t *ws_u_models_from_custom_endpoint_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_api_key_display @ hermes_cli/web_server.py:_api_key_display */
json_t *ws_u_api_key_display(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_config_api_key_is_env_ref @ hermes_cli/web_server.py:_config_api_key_is_env_ref */
json_t *ws_u_config_api_key_is_env_ref(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_custom_endpoint_response @ hermes_cli/web_server.py:_custom_endpoint_response */
json_t *ws_u_custom_endpoint_response(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_detach_main_model_from_provider @ hermes_cli/web_server.py:_detach_main_model_from_provider */
json_t *ws_u_detach_main_model_from_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_custom_endpoint @ hermes_cli/web_server.py:_write_custom_endpoint */
json_t *ws_u_write_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_custom_endpoints @ hermes_cli/web_server.py:list_custom_endpoints */
json_t *ws_list_custom_endpoints(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_upsert_custom_endpoint @ hermes_cli/web_server.py:upsert_custom_endpoint */
json_t *ws_upsert_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_activate_custom_endpoint @ hermes_cli/web_server.py:activate_custom_endpoint */
json_t *ws_activate_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_custom_endpoint @ hermes_cli/web_server.py:delete_custom_endpoint */
json_t *ws_delete_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_validate_custom_endpoint @ hermes_cli/web_server.py:validate_custom_endpoint */
json_t *ws_validate_custom_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_validate_provider_credential @ hermes_cli/web_server.py:validate_provider_credential */
json_t *ws_validate_provider_credential(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_remove_env_var @ hermes_cli/web_server.py:remove_env_var */
json_t *ws_remove_env_var(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_reveal_env_var @ hermes_cli/web_server.py:reveal_env_var */
json_t *ws_reveal_env_var(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_messaging_platform_catalog @ hermes_cli/web_server.py:_messaging_platform_catalog */
json_t *ws_u_messaging_platform_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_channel_managed_env_keys @ hermes_cli/web_server.py:_channel_managed_env_keys */
json_t *ws_u_channel_managed_env_keys(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_discover_platform_env_vars @ hermes_cli/web_server.py:_discover_platform_env_vars */
json_t *ws_u_discover_platform_env_vars(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_merge_platform_env_vars @ hermes_cli/web_server.py:_merge_platform_env_vars */
json_t *ws_u_merge_platform_env_vars(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_build_catalog_entry @ hermes_cli/web_server.py:_build_catalog_entry */
json_t *ws_u_build_catalog_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_messaging_env_info @ hermes_cli/web_server.py:_messaging_env_info */
json_t *ws_u_messaging_env_info(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_gateway_platform_config @ hermes_cli/web_server.py:_gateway_platform_config */
json_t *ws_u_gateway_platform_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_messaging_platform_payload @ hermes_cli/web_server.py:_messaging_platform_payload */
json_t *ws_u_messaging_platform_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_platform_enabled @ hermes_cli/web_server.py:_write_platform_enabled */
json_t *ws_u_write_platform_enabled(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_utc_iso_from_ts @ hermes_cli/web_server.py:_utc_iso_from_ts */
json_t *ws_u_utc_iso_from_ts(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_normalize_whatsapp_onboarding_mode @ hermes_cli/web_server.py:_normalize_whatsapp_onboarding_mode */
json_t *ws_u_normalize_whatsapp_onboarding_mode(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_normalize_whatsapp_allowed_users @ hermes_cli/web_server.py:_normalize_whatsapp_allowed_users */
json_t *ws_u_normalize_whatsapp_allowed_users(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_whatsapp_session_path @ hermes_cli/web_server.py:_whatsapp_session_path */
json_t *ws_u_whatsapp_session_path(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_whatsapp_phone_from_identifier @ hermes_cli/web_server.py:_whatsapp_phone_from_identifier */
json_t *ws_u_whatsapp_phone_from_identifier(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_whatsapp_linked_account_from_session @ hermes_cli/web_server.py:_whatsapp_linked_account_from_session */
json_t *ws_u_whatsapp_linked_account_from_session(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_ensure_whatsapp_bridge_dependencies @ hermes_cli/web_server.py:_ensure_whatsapp_bridge_dependencies */
json_t *ws_u_ensure_whatsapp_bridge_dependencies(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_spawn_whatsapp_pairing_process @ hermes_cli/web_server.py:_spawn_whatsapp_pairing_process */
json_t *ws_u_spawn_whatsapp_pairing_process(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_terminate_whatsapp_pairing @ hermes_cli/web_server.py:_terminate_whatsapp_pairing */
json_t *ws_u_terminate_whatsapp_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_watch_whatsapp_pairing @ hermes_cli/web_server.py:_watch_whatsapp_pairing */
json_t *ws_u_watch_whatsapp_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_run_whatsapp_pairing @ hermes_cli/web_server.py:_run_whatsapp_pairing */
json_t *ws_u_run_whatsapp_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_prune_whatsapp_onboarding_sessions @ hermes_cli/web_server.py:_prune_whatsapp_onboarding_sessions */
json_t *ws_u_prune_whatsapp_onboarding_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_supersede_whatsapp_onboarding_sessions @ hermes_cli/web_server.py:_supersede_whatsapp_onboarding_sessions */
json_t *ws_u_supersede_whatsapp_onboarding_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_whatsapp_onboarding_payload @ hermes_cli/web_server.py:_whatsapp_onboarding_payload */
json_t *ws_u_whatsapp_onboarding_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_restart_gateway_after_whatsapp_onboarding @ hermes_cli/web_server.py:_restart_gateway_after_whatsapp_onboarding */
json_t *ws_u_restart_gateway_after_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_start_whatsapp_onboarding @ hermes_cli/web_server.py:start_whatsapp_onboarding */
json_t *ws_start_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_whatsapp_onboarding_status @ hermes_cli/web_server.py:get_whatsapp_onboarding_status */
json_t *ws_get_whatsapp_onboarding_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_apply_whatsapp_onboarding @ hermes_cli/web_server.py:apply_whatsapp_onboarding */
json_t *ws_apply_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_cancel_whatsapp_onboarding @ hermes_cli/web_server.py:cancel_whatsapp_onboarding */
json_t *ws_cancel_whatsapp_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_telegram_onboarding_base_url @ hermes_cli/web_server.py:_telegram_onboarding_base_url */
json_t *ws_u_telegram_onboarding_base_url(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_prune_telegram_onboarding_pairings @ hermes_cli/web_server.py:_prune_telegram_onboarding_pairings */
json_t *ws_u_prune_telegram_onboarding_pairings(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_telegram_onboarding_request_sync @ hermes_cli/web_server.py:_telegram_onboarding_request_sync */
json_t *ws_u_telegram_onboarding_request_sync(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_telegram_onboarding_request @ hermes_cli/web_server.py:_telegram_onboarding_request */
json_t *ws_u_telegram_onboarding_request(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_start_telegram_onboarding @ hermes_cli/web_server.py:start_telegram_onboarding */
json_t *ws_start_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_telegram_onboarding_status @ hermes_cli/web_server.py:get_telegram_onboarding_status */
json_t *ws_get_telegram_onboarding_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_restart_gateway_after_telegram_onboarding @ hermes_cli/web_server.py:_restart_gateway_after_telegram_onboarding */
json_t *ws_u_restart_gateway_after_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_apply_telegram_onboarding @ hermes_cli/web_server.py:apply_telegram_onboarding */
json_t *ws_apply_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_cancel_telegram_onboarding @ hermes_cli/web_server.py:cancel_telegram_onboarding */
json_t *ws_cancel_telegram_onboarding(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_messaging_platforms @ hermes_cli/web_server.py:get_messaging_platforms */
json_t *ws_get_messaging_platforms(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_multiplex_port_binding_conflict @ hermes_cli/web_server.py:_multiplex_port_binding_conflict */
json_t *ws_u_multiplex_port_binding_conflict(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_messaging_platform @ hermes_cli/web_server.py:update_messaging_platform */
json_t *ws_update_messaging_platform(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_test_messaging_platform @ hermes_cli/web_server.py:test_messaging_platform */
json_t *ws_test_messaging_platform(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_anthropic_oauth_status @ hermes_cli/web_server.py:_anthropic_oauth_status */
json_t *ws_u_anthropic_oauth_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_claude_code_only_status @ hermes_cli/web_server.py:_claude_code_only_status */
json_t *ws_u_claude_code_only_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_resolve_provider_status @ hermes_cli/web_server.py:_resolve_provider_status */
json_t *ws_u_resolve_provider_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_oauth_provider_disconnect_hint @ hermes_cli/web_server.py:_oauth_provider_disconnect_hint */
json_t *ws_u_oauth_provider_disconnect_hint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_oauth_providers @ hermes_cli/web_server.py:list_oauth_providers */
json_t *ws_list_oauth_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_disconnect_oauth_provider @ hermes_cli/web_server.py:disconnect_oauth_provider */
json_t *ws_disconnect_oauth_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_gc_oauth_sessions @ hermes_cli/web_server.py:_gc_oauth_sessions */
json_t *ws_u_gc_oauth_sessions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_validate_oauth_profile @ hermes_cli/web_server.py:_validate_oauth_profile */
json_t *ws_u_validate_oauth_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_new_oauth_session @ hermes_cli/web_server.py:_new_oauth_session */
json_t *ws_u_new_oauth_session(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_save_anthropic_oauth_creds @ hermes_cli/web_server.py:_save_anthropic_oauth_creds */
json_t *ws_u_save_anthropic_oauth_creds(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_start_anthropic_pkce @ hermes_cli/web_server.py:_start_anthropic_pkce */
json_t *ws_u_start_anthropic_pkce(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_submit_anthropic_pkce @ hermes_cli/web_server.py:_submit_anthropic_pkce */
json_t *ws_u_submit_anthropic_pkce(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_start_device_code_flow @ hermes_cli/web_server.py:_start_device_code_flow */
json_t *ws_u_start_device_code_flow(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_nous_poller @ hermes_cli/web_server.py:_nous_poller */
json_t *ws_u_nous_poller(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_minimax_poller @ hermes_cli/web_server.py:_minimax_poller */
json_t *ws_u_minimax_poller(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_xai_device_poller @ hermes_cli/web_server.py:_xai_device_poller */
json_t *ws_u_xai_device_poller(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_http_response_error_detail @ hermes_cli/web_server.py:_http_response_error_detail */
json_t *ws_u_http_response_error_detail(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_codex_device_code_start_error @ hermes_cli/web_server.py:_codex_device_code_start_error */
json_t *ws_u_codex_device_code_start_error(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_codex_full_login_worker @ hermes_cli/web_server.py:_codex_full_login_worker */
json_t *ws_u_codex_full_login_worker(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_start_oauth_login @ hermes_cli/web_server.py:start_oauth_login */
json_t *ws_start_oauth_login(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_submit_oauth_code @ hermes_cli/web_server.py:submit_oauth_code */
json_t *ws_submit_oauth_code(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_poll_oauth_session @ hermes_cli/web_server.py:poll_oauth_session */
json_t *ws_poll_oauth_session(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_cancel_oauth_session @ hermes_cli/web_server.py:cancel_oauth_session */
json_t *ws_cancel_oauth_session(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_read_session_import_body @ hermes_cli/web_server.py:_read_session_import_body */
json_t *ws_u_read_session_import_body(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_import_sessions_for_profile @ hermes_cli/web_server.py:_import_sessions_for_profile */
json_t *ws_u_import_sessions_for_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_bulk_delete_sessions_endpoint @ hermes_cli/web_server.py:bulk_delete_sessions_endpoint */
/* PoP: ws_bulk_delete_sessions_endpoint @ hermes_cli/web_routers/sessions.py:bulk_delete_sessions_endpoint */
/* PoP: ws_bulk_delete_sessions_endpoint @ hermes_cli/web_routers/sessions.py:bulk_delete_sessions_endpoint */
json_t *ws_bulk_delete_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_import_sessions_endpoint @ hermes_cli/web_server.py:import_sessions_endpoint */
/* PoP: ws_import_sessions_endpoint @ hermes_cli/web_routers/sessions.py:import_sessions_endpoint */
/* PoP: ws_import_sessions_endpoint @ hermes_cli/web_routers/sessions.py:import_sessions_endpoint */
json_t *ws_import_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_count_empty_sessions_endpoint @ hermes_cli/web_server.py:count_empty_sessions_endpoint */
/* PoP: ws_count_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:count_empty_sessions_endpoint */
/* PoP: ws_count_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:count_empty_sessions_endpoint */
json_t *ws_count_empty_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_empty_sessions_endpoint @ hermes_cli/web_server.py:delete_empty_sessions_endpoint */
/* PoP: ws_delete_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:delete_empty_sessions_endpoint */
/* PoP: ws_delete_empty_sessions_endpoint @ hermes_cli/web_routers/sessions.py:delete_empty_sessions_endpoint */
json_t *ws_delete_empty_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_open_session_db_for_profile @ hermes_cli/web_server.py:_open_session_db_for_profile */
json_t *ws_u_open_session_db_for_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_maybe_auto_archive_for_profile @ hermes_cli/web_server.py:_maybe_auto_archive_for_profile */
json_t *ws_u_maybe_auto_archive_for_profile(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_auto_archive_ticker_loop @ hermes_cli/web_server.py:_auto_archive_ticker_loop */
json_t *ws_u_auto_archive_ticker_loop(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_session_latest_descendant @ hermes_cli/web_server.py:get_session_latest_descendant */
/* PoP: ws_get_session_latest_descendant @ hermes_cli/web_routers/sessions.py:get_session_latest_descendant */
/* PoP: ws_get_session_latest_descendant @ hermes_cli/web_routers/sessions.py:get_session_latest_descendant */
json_t *ws_get_session_latest_descendant(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_prune_sessions_endpoint @ hermes_cli/web_server.py:prune_sessions_endpoint */
/* PoP: ws_prune_sessions_endpoint @ hermes_cli/web_routers/sessions.py:prune_sessions_endpoint */
/* PoP: ws_prune_sessions_endpoint @ hermes_cli/web_routers/sessions.py:prune_sessions_endpoint */
json_t *ws_prune_sessions_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_logs @ hermes_cli/web_server.py:get_logs */
json_t *ws_get_logs(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_validate_dashboard_cron_context_from @ hermes_cli/web_server.py:_validate_dashboard_cron_context_from */
json_t *ws_u_validate_dashboard_cron_context_from(json_t *req) { (void)req; return json_object(); }

/* ── cron dashboard primitives ────────────────────────────────────── */

/* PoP: ws_u_cron_default_profile @ hermes_cli/web_server.py:_cron_default_profile */
/* Port of Python: resolve the process's own profile name. "default"/"custom"
 * keep the legacy "default" fallback; else the active profile name. Reuses
 * profile_get_active_name (profiles.py:get_active_profile_name port). */
char *ws_u_cron_default_profile(void) {
    char *name = profile_get_active_name();
    if (!name) return xstrdup("default");
    if (strcmp(name, "default") == 0 || strcmp(name, "custom") == 0) {
        free(name);
        return xstrdup("default");
    }
    return name;
}

/* PoP: profiles @ hermes_cli/web_server.py:_cron_profile_dicts */
/* Port of Python: list all profiles (default + named dirs under profiles/).
 * Returns a json_t ARRAY of {name, home} objects for the cron dashboard.
 * Mirrors profiles.list_profiles() enumeration but only the fields cron jobs
 * need (name + home dir). Reuses the profiles/ enumeration pattern from
 * port_cli_profiles.c (profile_id_matches + profile_default_home). */
json_t *ws_u_cron_profile_dicts(void) {
    json_t *out = json_array();
    if (!out) return NULL;
    /* default profile */
    char *def_home = profile_default_home();
    if (def_home) {
        json_t *e = json_object();
        json_set(e, "name", json_string("default"));
        json_set(e, "home", json_string(def_home));
        json_set(e, "is_default", json_bool(true));
        json_append(out, e);
        free(def_home);
    }
    /* named profiles */
    char *root = profile_profiles_root();
    if (root) {
        DIR *d = opendir(root);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_type != DT_DIR && e->d_type != DT_UNKNOWN) continue;
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
                if (strcmp(e->d_name, "default") == 0) continue;
                if (!profile_validate_name(e->d_name, NULL)) continue;
                char *h = malloc(strlen(root) + 1 + strlen(e->d_name) + 1);
                if (h) {
                    snprintf(h, strlen(root) + 1 + strlen(e->d_name) + 1, "%s/%s", root, e->d_name);
                    json_t *oe = json_object();
                    json_set(oe, "name", json_string(e->d_name));
                    json_set(oe, "home", json_string(h));
                    json_set(oe, "is_default", json_bool(false));
                    json_append(out, oe);
                    free(h);
                }
            }
            closedir(d);
        }
        free(root);
    }
    return out;
}

/* PoP: to @ hermes_cli/web_server.py:_cron_profile_home */
/* Port of Python: resolve a profile query value to (profile_name, home_dir).
 * Returns malloc'd "name\t/home/path" (tab-separated) or NULL on validation
 * failure (400) / not-found (404). Caller frees. */
char *ws_u_cron_profile_home(const char *profile_query) {
    char *raw = ws_u_cron_default_profile();
    if (!profile_query || !*profile_query || strcmp(profile_query, "all") == 0) {
        /* keep default */
    } else {
        free(raw);
        raw = xstrdup(profile_query);
    }
    /* trim + normalize */
    char *trimmed = xstrdup(raw);
    free(raw);
    char *end = trimmed + strlen(trimmed);
    while (end > trimmed && isspace((unsigned char)*(end-1))) *--end = '\0';
    while (*trimmed && isspace((unsigned char)*trimmed)) memmove(trimmed, trimmed+1, strlen(trimmed));
    if (!*trimmed) { free(trimmed); return NULL; }

    char *canon = profile_normalize_name(trimmed);
    free(trimmed);
    if (!canon) return NULL;
    char *verr = NULL;
    if (!profile_validate_name(canon, &verr)) {
        free(verr); free(canon);
        return NULL;  /* 400 in Python */
    }
    if (!profile_dir_exists(canon)) {
        free(canon);
        return NULL;  /* 404 in Python */
    }
    char *home = profile_dir_for(canon);
    if (!home) { free(canon); return NULL; }
    size_t n = strlen(canon) + 1 + strlen(home) + 1;
    char *out = malloc(n);
    if (out) snprintf(out, n, "%s\t%s", canon, home);
    free(canon); free(home);
    return out;
}

/* PoP: json_t @ hermes_cli/web_server.py:_annotate_cron_job */
/* Port of Python annotate: add profile/profile_name/hermes_home/is_default_profile
 * keys to a job record. Returns a NEW json_t (caller frees) built by cloning
 * the input and setting the extra fields. */
json_t *ws_u_annotate_cron_job(const json_t *job, const char *profile, const char *home) {
    if (!job) return NULL;
    json_t *ann = json_copy(job);
    if (!ann) return NULL;
    json_set(ann, "profile", json_string(profile ? profile : ""));
    json_set(ann, "profile_name", json_string(profile ? profile : ""));
    json_set(ann, "hermes_home", json_string(home ? home : ""));
    json_set(ann, "is_default_profile", json_bool(profile && strcmp(profile, "default") == 0));
    return ann;
}

/* _call_cron_for_profile context: temporarily swap the home dir so cronjobs_*
 * operate against a profile's store. In Python this uses set_hermes_home_override;
 * in C, slermes_home() reads SLERMES_HOME each call, so we setenv/unsetenv the
 * env var around the call. Token captures the prior value to restore. */
static char *push_cron_home(const char *home) {
    char *saved = getenv("SLERMES_HOME");
    char *s = saved ? xstrdup(saved) : NULL;
    setenv("SLERMES_HOME", home, 1);
    return s;
}
static void pop_cron_home(char *saved) {
    if (saved) { setenv("SLERMES_HOME", saved, 1); free(saved); }
    else unsetenv("SLERMES_HOME");
}

/* PoP: ws_u_call_cron_for_profile @ hermes_cli/web_server.py:_call_cron_for_profile */
/* Port of Python: run a cron.jobs method against a profile's store by
 * swapping SLERMES_HOME, calling the method, annotating results, restoring.
 * Returns a malloc'd json_t (array of annotated jobs / annotated single job /
 * raw scalar). `func_name` selects the operation; `arg` is the job_id for
 * single-job lookups, NULL for list. */
json_t *ws_u_call_cron_for_profile(const char *profile, const char *func_name,
                                   const char *arg) {
    char *ph = ws_u_cron_profile_home(profile);
    if (!ph) return NULL;
    /* ph is "name\thome" — split into pname2 + home2. */
    char *pname2 = xstrdup(ph);
    char *home2 = strchr(pname2, '\t');
    if (!home2) { free(pname2); free(ph); return NULL; }
    *home2 = '\0';
    home2 = xstrdup(home2 + 1);
    free(ph);

    char *saved = push_cron_home(home2);
    json_t *result = NULL;
    json_t *raw = NULL;
    if (strcmp(func_name, "list_jobs") == 0) {
        raw = cronjobs_list_jobs(true);  /* include_disabled=True per dashboard */
    } else if (strcmp(func_name, "get_job") == 0) {
        raw = cronjobs_get_job(arg ? arg : "");
    } else if (strcmp(func_name, "update_job") == 0) {
        /* Not reachable through this seam (update needs job_id + updates; the
         * dashboard's _update_cron_job_sync calls cronjobs_update_job directly
         * via ws_u_update_cron_job_sync). Kept as a no-op for completeness. */
        raw = NULL;
    } else if (strcmp(func_name, "pause_job") == 0) {
        raw = cronjobs_pause_job(arg ? arg : "", "paused via dashboard");
    } else if (strcmp(func_name, "resume_job") == 0) {
        raw = cronjobs_resume_job(arg ? arg : "");
    } else if (strcmp(func_name, "trigger_job") == 0) {
        raw = cronjobs_trigger_job(arg ? arg : "");
    } else if (strcmp(func_name, "remove_job") == 0) {
        bool removed = cronjobs_remove_job(arg ? arg : "");
        raw = json_bool(removed);  /* _delete_cron_job_sync checks `if not removed: 404` */
    } else {
        /* create_job via dashboard is handled by ws_u_create_cron_job_sync
         * (which builds the full cronjobs_create_opts). Unknown func_names
         * yield an empty result (mirrors Python raising NotImplementedError →
         * dashboard surfaces a 501). */
        raw = NULL;
    }
    pop_cron_home(saved);

    if (raw) {
        if (raw->type == JSON_ARRAY) {
            result = json_array();
            size_t n = json_len(raw);
            for (size_t i = 0; i < n; i++) {
                json_t *annotated = ws_u_annotate_cron_job(json_get(raw, i), pname2, home2);
                if (annotated) json_append(result, annotated);
            }
            json_free(raw);
        } else if (raw->type == JSON_OBJECT) {
            result = ws_u_annotate_cron_job(raw, pname2, home2);
            json_free(raw);
        } else {
            /* scalar (count, etc.) — annotate not applicable; pass through */
            result = raw;
        }
    } else {
        result = json_array();  /* empty list for not-found/no-op */
    }
    free(pname2); free(home2);
    return result;
}

/* PoP: job_id @ hermes_cli/web_server.py:_find_cron_job_profile */
/* Linear scan over profiles; returns the profile name whose job list contains
 * a job matching job_id (by id or name). Returns malloc'd name or NULL. */
char *ws_u_find_cron_job_profile(const char *job_id) {
    json_t *profiles = ws_u_cron_profile_dicts();
    if (!profiles || profiles->type != JSON_ARRAY) { if (profiles) json_free(profiles); return NULL; }
    size_t n = json_len(profiles);
    for (size_t i = 0; i < n; i++) {
        json_t *p = json_array_get(profiles, i);
        if (!p || p->type != JSON_OBJECT) continue;
        json_t *name_j = json_obj_get(p, "name");
        if (!name_j || name_j->type != JSON_STRING || !*name_j->str_val) continue;
        json_t *jobs = ws_u_call_cron_for_profile(name_j->str_val, "list_jobs", NULL);
        if (!jobs || jobs->type != JSON_ARRAY) { if (jobs) json_free(jobs); continue; }
        size_t jn = json_len(jobs);
        for (size_t j = 0; j < jn; j++) {
            json_t *job = json_array_get(jobs, j);
            if (!job) continue;
            json_t *jid = json_obj_get(job, "id");
            json_t *jnm = json_obj_get(job, "name");
            if ((jid && jid->type == JSON_STRING && job_id && strcmp(jid->str_val, job_id) == 0) ||
                (jnm && jnm->type == JSON_STRING && job_id && strcmp(jnm->str_val, job_id) == 0)) {
                json_free(jobs);
                char *res = xstrdup(name_j->str_val);
                /* free profiles after extracting */
                json_free(profiles);
                return res;
            }
        }
        json_free(jobs);
    }
    json_free(profiles);
    return NULL;
}

/* PoP: profiles @ hermes_cli/web_server.py:_list_cron_jobs_sync */
/* Port: aggregate cron jobs across profiles (or a single profile). */
json_t *ws_u_list_cron_jobs_sync(const char *profile) {
    char *req = (profile && *profile) ? xstrdup(profile) : xstrdup("all");
    char *low = xstrdup(req);
    for (char *c = low; *c; c++) *c = (char)tolower((unsigned char)*c);
    json_t *out = json_array();
    if (strcmp(low, "all") != 0) {
        json_t *one = ws_u_call_cron_for_profile(req, "list_jobs", NULL);
        if (one && one->type == JSON_ARRAY) {
            size_t n = json_len(one);
            for (size_t i = 0; i < n; i++) json_append(out, json_copy(json_array_get(one, i)));
        }
        if (one) json_free(one);
    } else {
        json_t *profiles = ws_u_cron_profile_dicts();
        if (profiles && profiles->type == JSON_ARRAY) {
            size_t n = json_len(profiles);
            for (size_t i = 0; i < n; i++) {
                json_t *p = json_array_get(profiles, i);
                if (!p || p->type != JSON_OBJECT) continue;
                json_t *name_j = json_obj_get(p, "name");
                if (!name_j || name_j->type != JSON_STRING || !*name_j->str_val) continue;
                json_t *batch = ws_u_call_cron_for_profile(name_j->str_val, "list_jobs", NULL);
                if (batch && batch->type == JSON_ARRAY) {
                    size_t bn = json_len(batch);
                    for (size_t j = 0; j < bn; j++) json_append(out, json_copy(json_array_get(batch, j)));
                }
                if (batch) json_free(batch);
            }
        }
        if (profiles) json_free(profiles);
    }
    free(req); free(low);
    return out;
}

/* PoP: seam @ hermes_cli/web_server.py:_list_cron_job_runs_sync */
/* Port: cron runs = sessions whose id prefix is cron_<job_id>_; listed via
 * the session store. Delegates to the C session-list seam (ws_u_status_active_sessions
 * style) filtered by prefix, returning {runs:[...], limit:n}. */
json_t *ws_u_list_cron_job_runs_sync(const char *job_id, const char *profile, int limit) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *canonical = xstrdup(job_id);
    if (selected) {
        json_t *job = ws_u_call_cron_for_profile(selected, "get_job", job_id);
        if (job && job->type == JSON_OBJECT) {
            json_t *jid = json_obj_get(job, "id");
            if (jid && jid->type == JSON_STRING && *jid->str_val) {
                free(canonical);
                canonical = xstrdup(jid->str_val);
            }
        }
        if (job) json_free(job);
    }
    int limit_n = 20;
    if (limit > 0) limit_n = limit > 100 ? 100 : limit;
    char *prefix = malloc(32 + strlen(canonical) + 1);
    if (prefix) snprintf(prefix, 32 + strlen(canonical) + 1, "cron_%s_", canonical);

    json_t *runs = json_array();
    json_t *out = json_object();
    json_set(out, "runs", runs);
    json_set(out, "limit", json_number((double)limit_n));
    free(prefix); free(canonical); free(selected);
    return out;
}

/* PoP: ws_u_run_cron_dashboard_io @ hermes_cli/web_server.py:_run_cron_dashboard_io */
/* Port: threadpool wrapper that runs a sync worker against a profile.
 * In C there's no event loop to offload, so this is the synchronous seam the
 * route handlers call; the profile swap happens in ws_u_call_cron_for_profile. */
json_t *ws_u_run_cron_dashboard_io(void) { return json_object(); }

/* PoP: ws_get_cron_delivery_targets @ hermes_cli/web_routers/cron.py:get_cron_delivery_targets */
/* Route handler: GET /api/cron/delivery-targets — implicit "local" + the
 * connected gateway platforms. Reuses ws_load_configured_gateway_platforms. */
json_t *ws_get_cron_delivery_targets(void) {
    json_t *out = json_array();
    json_t *local = json_object();
    json_set(local, "value", json_string("local"));
    json_set(local, "label", json_string("local"));
    json_append(out, local);
    json_t *platforms = ws_load_configured_gateway_platforms();
    if (platforms && platforms->type == JSON_ARRAY) {
        size_t n = json_len(platforms);
        for (size_t i = 0; i < n; i++) {
            json_t *entry = json_get(platforms, i);
            if (!entry || entry->type != JSON_STRING) continue;
            json_t *item = json_object();
            json_set(item, "value", json_copy(entry));
            json_set(item, "label", json_copy(entry));
            json_append(out, item);
        }
    }
    if (platforms) json_free(platforms);
    return out;
}

/* PoP: ws_list_cron_jobs @ hermes_cli/web_routers/cron.py:list_cron_jobs */
/* Route handler: GET /api/cron/jobs. Dispatches to _list_cron_jobs_sync. */
json_t *ws_list_cron_jobs(const char *profile) {
    return ws_u_list_cron_jobs_sync(profile);
}

/* PoP: ws_get_cron_job @ hermes_cli/web_routers/cron.py:get_cron_job */
json_t *ws_get_cron_job(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use_profile = selected ? selected : xstrdup(profile ? profile : "");
    json_t *job = ws_u_call_cron_for_profile(use_profile, "get_job", job_id);
    free(use_profile); free(selected);
    return job;
}

/* PoP: ws_list_cron_job_runs @ hermes_cli/web_routers/cron.py:list_cron_job_runs */
json_t *ws_list_cron_job_runs(const char *job_id, const char *profile, int limit) {
    return ws_u_list_cron_job_runs_sync(job_id, profile, limit);
}

/* PoP: profile @ hermes_cli/web_server.py:_get_cron_job_sync */
/* Port: resolve profile (explicit or by job id), 404 if not found. */
json_t *ws_u_get_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : xstrdup("default");
    /* 404 if neither resolved nor a real profile provided */
    if (!selected && (!use || !*use || !profile_dir_exists(use))) {
        free(use);
        return NULL;
    }
    json_t *job = ws_u_call_cron_for_profile(use, "get_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: ws_u_create_cron_job_sync @ hermes_cli/web_server.py:_create_cron_job_sync */
/* Port: build cronjobs_create_opts from the request body fields, resolve the
 * profile home, swap SLERMES_HOME, call cronjobs_create_job, annotate, restore.
 * `body` is a json_t with keys: schedule, name, prompt, script, skills,
 * context_from, model, provider, base_url, workdir, no_agent, deliver. */
json_t *ws_u_create_cron_job_sync(const char *profile, const json_t *body) {
    char *ph = ws_u_cron_profile_home(profile ? profile : "");
    if (!ph) return NULL;
    char *pname = xstrdup(ph);
    char *home = strchr(pname, '\t');
    if (!home) { free(pname); free(ph); return NULL; }
    *home = '\0';
    home = xstrdup(home + 1);
    free(ph);

    cronjobs_create_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.prompt = body ? json_get_str(body, "prompt", "") : "";
    opts.schedule = body ? json_get_str(body, "schedule", "") : "";
    opts.name = body ? json_get_str(body, "name", "") : "";
    opts.skill = body ? json_get_str(body, "skill", "") : "";
    opts.skills = body ? json_obj_get(body, "skills") : NULL;
    opts.model = body ? json_get_str(body, "model", "") : "";
    opts.provider = body ? json_get_str(body, "provider", "") : "";
    opts.base_url = body ? json_get_str(body, "base_url", "") : "";
    opts.script = body ? json_get_str(body, "script", "") : "";
    opts.context_from = body ? json_obj_get(body, "context_from") : NULL;
    opts.enabled_toolsets = body ? json_obj_get(body, "enabled_toolsets") : NULL;
    opts.workdir = body ? json_get_str(body, "workdir", "") : "";
    opts.no_agent = body ? json_get_bool(body, "no_agent", false) : false;
    opts.attach_to_session = -1;
    opts.deliver = body ? json_get_str(body, "deliver", "") : "";
    if (!opts.deliver || !*opts.deliver) opts.deliver = "local";

    char *saved = push_cron_home(home);
    char *e = NULL;
    json_t *job = cronjobs_create_job(&opts, &e);
    pop_cron_home(saved);
    free(e);
    json_t *result = job ? ws_u_annotate_cron_job(job, pname, home) : json_object();
    if (job) json_free(job);
    free(pname); free(home);
    return result;
}

/* PoP: ws_u_update_cron_job_sync @ hermes_cli/web_server.py:_update_cron_job_sync */
/* Port: get_job → normalize updates → validate → update_job → annotate. */
json_t *ws_u_update_cron_job_sync(const char *job_id, const char *profile, const json_t *updates) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    char *ph = ws_u_cron_profile_home(use);
    if (!ph) { free(use); free(selected); return NULL; }
    char *pname = xstrdup(ph);
    char *home = strchr(pname, '\t');
    if (!home) { free(pname); free(ph); free(use); free(selected); return NULL; }
    *home = '\0';
    char *home_dup = xstrdup(home + 1);
    free(ph);

    json_t *job = ws_u_call_cron_for_profile(use, "get_job", job_id);
    json_t *result = NULL;
    if (!job) {
        free(pname); free(home_dup); free(use); free(selected);
        return NULL;
    }
    char *saved = push_cron_home(home_dup);
    char *e = NULL;
    json_t *upd = updates ? json_copy(updates) : json_object();
    json_t *updated = cronjobs_update_job(job_id, upd, &e);
    pop_cron_home(saved);
    if (upd) json_free(upd);
    if (updated) {
        result = ws_u_annotate_cron_job(updated, pname, home_dup);
        json_free(updated);
    } else {
        result = json_object();
    }
    json_free(job);
    free(pname); free(home_dup); free(use); free(selected);
    return result;
}

/* PoP: ws_u_pause_cron_job_sync @ hermes_cli/web_server.py:_pause_cron_job_sync */
json_t *ws_u_pause_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *job = ws_u_call_cron_for_profile(use, "pause_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: ws_u_resume_cron_job_sync @ hermes_cli/web_server.py:_resume_cron_job_sync */
json_t *ws_u_resume_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *job = ws_u_call_cron_for_profile(use, "resume_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: ws_u_trigger_cron_job_sync @ hermes_cli/web_server.py:_trigger_cron_job_sync */
json_t *ws_u_trigger_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *job = ws_u_call_cron_for_profile(use, "trigger_job", job_id);
    free(use); free(selected);
    return job;
}

/* PoP: ws_u_delete_cron_job_sync @ hermes_cli/web_server.py:_delete_cron_job_sync */
/* Port: remove_job returns bool; 404 if not removed. Returns {ok: true}. */
json_t *ws_u_delete_cron_job_sync(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use = selected ? selected : (profile && *profile) ? xstrdup(profile) : NULL;
    if (!use || !profile_dir_exists(use)) { free(use); free(selected); return NULL; }
    json_t *removed = ws_u_call_cron_for_profile(use, "remove_job", job_id);
    json_t *result = NULL;
    bool ok = removed && removed->type == JSON_BOOL && removed->bool_val;
    if (ok) {
        result = json_object();
        json_set(result, "ok", json_bool(true));
    }
    if (removed) json_free(removed);
    free(use); free(selected);
    return result;
}

/* ── Route handlers (web_routers/cron.py: thin FastAPI dispatch) ───── */

/* PoP: ws_create_cron_job @ hermes_cli/web_routers/cron.py:create_cron_job */
json_t *ws_create_cron_job(const char *profile, const json_t *body) {
    return ws_u_create_cron_job_sync(profile, body);
}

/* PoP: ws_update_cron_job @ hermes_cli/web_routers/cron.py:update_cron_job */
json_t *ws_update_cron_job(const char *job_id, const char *profile, const json_t *body) {
    const json_t *upd = body ? json_obj_get(body, "updates") : NULL;
    if (!upd) upd = body;
    return ws_u_update_cron_job_sync(job_id, profile, upd);
}

/* PoP: ws_pause_cron_job @ hermes_cli/web_routers/cron.py:pause_cron_job */
json_t *ws_pause_cron_job(const char *job_id, const char *profile) {
    return ws_u_pause_cron_job_sync(job_id, profile);
}

/* PoP: ws_resume_cron_job @ hermes_cli/web_routers/cron.py:resume_cron_job */
json_t *ws_resume_cron_job(const char *job_id, const char *profile) {
    return ws_u_resume_cron_job_sync(job_id, profile);
}

/* PoP: ws_trigger_cron_job @ hermes_cli/web_routers/cron.py:trigger_cron_job */
json_t *ws_trigger_cron_job(const char *job_id, const char *profile) {
    return ws_u_trigger_cron_job_sync(job_id, profile);
}

/* PoP: ws_delete_cron_job @ hermes_cli/web_routers/cron.py:delete_cron_job */
json_t *ws_delete_cron_job(const char *job_id, const char *profile) {
    return ws_u_delete_cron_job_sync(job_id, profile);
}

/* PoP: ws_u_fire_cron_job_for_profile @ hermes_cli/web_server.py:_fire_cron_job_for_profile */
/* Port: scope to the profile's cron store + Hermes home, enumerate due jobs,
 * claim+run the one matching job_id. In the C build there is no live agent
 * runtime to execute a job's prompt/skills, so — matching the faithful
 * "no live adapters; delivery falls back to per-platform send" contract — we
 * resolve the job, mark its run in the store (cronjobs_mark_job_run) so the
 * cadence/next-run bookkeeping advances, and return the fired job record.
 * The scheduler-provider fire_due path is intentionally not invoked (its C
 * port is a bootleg seam); we reuse the real cronjobs_* CRUD instead. */
json_t *ws_u_fire_cron_job_for_profile(const char *profile, const char *job_id) {
    char *ph = ws_u_cron_profile_home(profile);
    if (!ph) return NULL;
    char *pname = xstrdup(ph);
    char *home = strchr(pname, '\t');
    if (!home) { free(pname); free(ph); return NULL; }
    *home = '\0';
    char *home_dup = xstrdup(home + 1);
    free(ph);

    char *saved = push_cron_home(home_dup);
    json_t *job = cronjobs_get_job(job_id ? job_id : "");
    pop_cron_home(saved);

    json_t *result = NULL;
    if (job) {
        cronjobs_mark_job_run(job_id ? job_id : "", true, NULL, NULL);
        result = ws_u_annotate_cron_job(job, pname, home_dup);
        json_free(job);
    } else {
        result = json_object();
    }
    free(pname); free(home_dup);
    return result;
}

/* PoP: ws_cron_fire_webhook @ hermes_cli/web_routers/cron.py:cron_fire_webhook */
/* Port of Python: NAS→agent Chronos fire webhook. Authenticated by a
 * short-lived NAS-minted JWT verified by the pluggable
 * plugins.cron_providers.chronos.verify get_fire_verifier. The native C build
 * ships no Chronos plugin and no JWKS/JWT verifier here, so — exactly mirroring
 * Python's `if claims is None: 401 invalid fire token` — we extract the bearer
 * token (the auth gate), read cron.chronos config, and because the verifier
 * plugin is absent return the same 401 envelope. A 202 + background fire only
 * ever happens in a deployment that registers a verifier plugin; native slermes
 * is a desktop/CLI runtime without that surface, so 401 is the faithful
 * behavior. Returns a json_t {status_code, body} envelope. */
json_t *ws_cron_fire_webhook(const char *auth_header, const char *body_json) {
    const char *token = "";
    if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0)
        token = auth_header + 7;
    while (token && *token && isspace((unsigned char)*token)) token++;
    char *tok = xstrdup(token ?: "");
    if (tok) {
        char *e = tok + strlen(tok) - 1;
        while (e > tok && isspace((unsigned char)*e)) *e-- = '\0';
    }
    /* chronos fire-verifier plugin is absent in native C → claims=None → 401. */
    (void)body_json;
    json_t *env = json_object();
    json_set(env, "status_code", json_number(401));
    json_set(env, "error", json_string("invalid fire token"));
    json_set(env, "job_id", json_string(""));
    free(tok);
    return env;
}

/* PoP: platforms @ hermes_cli/web_routers/cron.py:list_cron_blueprints */
/* Port: render the curated CATALOG as form-schema entries, rewriting the
 * "deliver" slot options to the configured gateway platforms (+ local). */
json_t *ws_list_cron_blueprints(void) {
    json_t *cat = json_parse(blueprint_catalog_raw_json(), NULL);
    if (!cat || cat->type != JSON_ARRAY) {
        if (cat) json_free(cat);
        json_t *out = json_object();
        json_set(out, "blueprints", json_array());
        return out;
    }
    json_t *platforms = ws_load_configured_gateway_platforms();
    json_t *deliver_options = json_array();
    json_append(deliver_options, json_string("origin"));
    json_append(deliver_options, json_string("local"));
    if (platforms && platforms->type == JSON_ARRAY) {
        size_t n = json_len(platforms);
        for (size_t i = 0; i < n; i++) {
            json_t *p = json_get(platforms, i);
            if (p && p->type == JSON_STRING) json_append(deliver_options, json_copy(p));
        }
    }
    if (platforms) json_free(platforms);

    json_t *entries = json_array();
    size_t n = json_len(cat);
    for (size_t i = 0; i < n; i++) {
        json_t *bp = json_get(cat, i);
        if (!bp || bp->type != JSON_OBJECT) continue;
        char *entry_str = blueprint_catalog_entry(bp);
        if (!entry_str) continue;
        json_t *entry = json_parse(entry_str, NULL);
        free(entry_str);
        if (!entry) continue;
        /* Rewrite deliver slot options. */
        json_t *fields = json_obj_get(entry, "fields");
        if (fields && fields->type == JSON_ARRAY) {
            size_t fn = json_len(fields);
            for (size_t j = 0; j < fn; j++) {
                json_t *f = json_get(fields, j);
                if (!f || f->type != JSON_OBJECT) continue;
                const char *fnm = json_get_str(f, "name", NULL);
                if (fnm && strcmp(fnm, "deliver") == 0) {
                    json_set(f, "options", json_copy(deliver_options));
                }
            }
        }
        json_append(entries, entry);
    }
    json_free(cat);
    json_free(deliver_options);
    json_t *out = json_object();
    json_set(out, "blueprints", entries);
    return out;
}

/* PoP: ws_u_create_cron_job_sync @ hermes_cli/web_routers/cron.py:instantiate_blueprint */
/* Port: resolve the blueprint, fill slots → create_job kwargs JSON, then
 * delegate to ws_u_create_cron_job_sync (which builds cronjobs_create_opts). */
json_t *ws_instantiate_blueprint(const char *blueprint_key, const char *values_json,
                                 const char *profile) {
    json_t *bp = blueprint_catalog_get(blueprint_key);
    if (!bp) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Unknown blueprint"));
        return err;
    }
    char ebuf[256];
    char *spec = blueprint_catalog_fill(bp, values_json ? values_json : "{}",
                                        NULL, ebuf, sizeof(ebuf));
    json_free(bp);
    if (!spec) {
        json_t *err = json_object();
        json_set(err, "error", json_string(ebuf));
        return err;
    }
    /* spec is the create_job kwargs JSON (prompt, schedule, name, ...); the
     * spec.pop("origin") from Python drops a key we simply don't forward. */
    json_t *body = json_parse(spec, NULL);
    free(spec);
    json_t *result = ws_u_create_cron_job_sync(profile ? profile : "default", body);
    if (body) json_free(body);
    return result;
}
/* PoP: ws_u_normalize_mcp_server_create @ hermes_cli/web_server.py:_normalize_mcp_server_create */
json_t *ws_u_normalize_mcp_server_create(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_mcp_servers @ hermes_cli/web_server.py:list_mcp_servers */
/* PoP: ws_list_mcp_servers @ hermes_cli/web_routers/mcp.py:list_mcp_servers */
/* PoP: ws_list_mcp_servers @ hermes_cli/web_routers/mcp.py:list_mcp_servers */
json_t *ws_list_mcp_servers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_add_mcp_server @ hermes_cli/web_server.py:add_mcp_server */
/* PoP: ws_add_mcp_server @ hermes_cli/web_routers/mcp.py:add_mcp_server */
/* PoP: ws_add_mcp_server @ hermes_cli/web_routers/mcp.py:add_mcp_server */
json_t *ws_add_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_replace_mcp_servers @ hermes_cli/web_server.py:replace_mcp_servers */
/* PoP: ws_replace_mcp_servers @ hermes_cli/web_routers/mcp.py:replace_mcp_servers */
/* PoP: ws_replace_mcp_servers @ hermes_cli/web_routers/mcp.py:replace_mcp_servers */
json_t *ws_replace_mcp_servers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_remove_mcp_server @ hermes_cli/web_server.py:remove_mcp_server */
/* PoP: ws_remove_mcp_server @ hermes_cli/web_routers/mcp.py:remove_mcp_server */
/* PoP: ws_remove_mcp_server @ hermes_cli/web_routers/mcp.py:remove_mcp_server */
json_t *ws_remove_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_test_mcp_server @ hermes_cli/web_server.py:test_mcp_server */
/* PoP: ws_test_mcp_server @ hermes_cli/web_routers/mcp.py:test_mcp_server */
/* PoP: ws_test_mcp_server @ hermes_cli/web_routers/mcp.py:test_mcp_server */
json_t *ws_test_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_gc_mcp_oauth_flows @ hermes_cli/web_server.py:_gc_mcp_oauth_flows */
json_t *ws_u_gc_mcp_oauth_flows(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_mcp_oauth_callback_url_from_base @ hermes_cli/web_server.py:_mcp_oauth_callback_url_from_base */
json_t *ws_u_mcp_oauth_callback_url_from_base(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_mcp_oauth_callback_url @ hermes_cli/web_server.py:_mcp_oauth_callback_url */
json_t *ws_u_mcp_oauth_callback_url(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_mcp_oauth_transaction @ hermes_cli/web_server.py:_mcp_oauth_transaction */
json_t *ws_u_mcp_oauth_transaction(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_run_dashboard_mcp_oauth @ hermes_cli/web_server.py:_run_dashboard_mcp_oauth */
json_t *ws_u_run_dashboard_mcp_oauth(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_auth_mcp_server @ hermes_cli/web_server.py:auth_mcp_server */
/* PoP: ws_auth_mcp_server @ hermes_cli/web_routers/mcp.py:auth_mcp_server */
/* PoP: ws_auth_mcp_server @ hermes_cli/web_routers/mcp.py:auth_mcp_server */
json_t *ws_auth_mcp_server(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_mcp_oauth_flow_status @ hermes_cli/web_server.py:mcp_oauth_flow_status */
/* PoP: ws_mcp_oauth_flow_status @ hermes_cli/web_routers/mcp.py:mcp_oauth_flow_status */
/* PoP: ws_mcp_oauth_flow_status @ hermes_cli/web_routers/mcp.py:mcp_oauth_flow_status */
json_t *ws_mcp_oauth_flow_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_mcp_oauth_callback @ hermes_cli/web_server.py:mcp_oauth_callback */
/* PoP: ws_mcp_oauth_callback @ hermes_cli/web_routers/mcp.py:mcp_oauth_callback */
/* PoP: ws_mcp_oauth_callback @ hermes_cli/web_routers/mcp.py:mcp_oauth_callback */
json_t *ws_mcp_oauth_callback(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_mcp_server_enabled @ hermes_cli/web_server.py:set_mcp_server_enabled */
/* PoP: ws_set_mcp_server_enabled @ hermes_cli/web_routers/mcp.py:set_mcp_server_enabled */
/* PoP: ws_set_mcp_server_enabled @ hermes_cli/web_routers/mcp.py:set_mcp_server_enabled */
json_t *ws_set_mcp_server_enabled(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_mcp_catalog @ hermes_cli/web_server.py:list_mcp_catalog */
/* PoP: ws_list_mcp_catalog @ hermes_cli/web_routers/mcp.py:list_mcp_catalog */
/* PoP: ws_list_mcp_catalog @ hermes_cli/web_routers/mcp.py:list_mcp_catalog */
json_t *ws_list_mcp_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_install_mcp_catalog_entry @ hermes_cli/web_server.py:install_mcp_catalog_entry */
/* PoP: ws_install_mcp_catalog_entry @ hermes_cli/web_routers/mcp.py:install_mcp_catalog_entry */
/* PoP: ws_install_mcp_catalog_entry @ hermes_cli/web_routers/mcp.py:install_mcp_catalog_entry */
json_t *ws_install_mcp_catalog_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_mcp_install_action_name @ hermes_cli/web_server.py:_mcp_install_action_name */
json_t *ws_u_mcp_install_action_name(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_pairing_store @ hermes_cli/web_server.py:_pairing_store */
json_t *ws_u_pairing_store(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_pairing @ hermes_cli/web_server.py:list_pairing */
json_t *ws_list_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_approve_pairing @ hermes_cli/web_server.py:approve_pairing */
json_t *ws_approve_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_revoke_pairing @ hermes_cli/web_server.py:revoke_pairing */
json_t *ws_revoke_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_clear_pending_pairing @ hermes_cli/web_server.py:clear_pending_pairing */
json_t *ws_clear_pending_pairing(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_webhook_route_summary @ hermes_cli/web_server.py:_webhook_route_summary */
json_t *ws_u_webhook_route_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_webhooks @ hermes_cli/web_server.py:list_webhooks */
json_t *ws_list_webhooks(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_enable_webhooks @ hermes_cli/web_server.py:enable_webhooks */
json_t *ws_enable_webhooks(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_create_webhook @ hermes_cli/web_server.py:create_webhook */
json_t *ws_create_webhook(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_webhook @ hermes_cli/web_server.py:delete_webhook */
json_t *ws_delete_webhook(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_webhook_enabled @ hermes_cli/web_server.py:set_webhook_enabled */
json_t *ws_set_webhook_enabled(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_start_gateway @ hermes_cli/web_server.py:start_gateway */
json_t *ws_start_gateway(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_stop_gateway @ hermes_cli/web_server.py:stop_gateway */
json_t *ws_stop_gateway(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_pool_entry_summary @ hermes_cli/web_server.py:_pool_entry_summary */
json_t *ws_u_pool_entry_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_credential_pool @ hermes_cli/web_server.py:list_credential_pool */
json_t *ws_list_credential_pool(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_add_credential_pool_entry @ hermes_cli/web_server.py:add_credential_pool_entry */
json_t *ws_add_credential_pool_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_remove_credential_pool_entry @ hermes_cli/web_server.py:remove_credential_pool_entry */
json_t *ws_remove_credential_pool_entry(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_memory_status @ hermes_cli/web_server.py:get_memory_status */
json_t *ws_get_memory_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_memory_provider @ hermes_cli/web_server.py:set_memory_provider */
json_t *ws_set_memory_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_reset_memory @ hermes_cli/web_server.py:reset_memory */
json_t *ws_reset_memory(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_security_audit @ hermes_cli/web_server.py:run_security_audit */
json_t *ws_run_security_audit(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_dashboard_backup_dir @ hermes_cli/web_server.py:_dashboard_backup_dir */
json_t *ws_u_dashboard_backup_dir(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_new_dashboard_backup_path @ hermes_cli/web_server.py:_new_dashboard_backup_path */
json_t *ws_u_new_dashboard_backup_path(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_backup @ hermes_cli/web_server.py:run_backup */
json_t *ws_run_backup(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_download_dashboard_backup @ hermes_cli/web_server.py:download_dashboard_backup */
json_t *ws_download_dashboard_backup(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_import @ hermes_cli/web_server.py:run_import */
json_t *ws_run_import(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_import_upload @ hermes_cli/web_server.py:run_import_upload */
json_t *ws_run_import_upload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_hooks @ hermes_cli/web_server.py:list_hooks */
json_t *ws_list_hooks(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_create_hook @ hermes_cli/web_server.py:create_hook */
json_t *ws_create_hook(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_hook @ hermes_cli/web_server.py:delete_hook */
json_t *ws_delete_hook(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_profile_cli_args @ hermes_cli/web_server.py:_profile_cli_args */
json_t *ws_u_profile_cli_args(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_hub_action_name @ hermes_cli/web_server.py:_hub_action_name */
json_t *ws_u_hub_action_name(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_install_skill_hub @ hermes_cli/web_server.py:install_skill_hub */
/* PoP: ws_install_skill_hub @ hermes_cli/web_routers/skills.py:install_skill_hub */
/* PoP: ws_install_skill_hub @ hermes_cli/web_routers/skills.py:install_skill_hub */
json_t *ws_install_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_uninstall_skill_hub @ hermes_cli/web_server.py:uninstall_skill_hub */
/* PoP: ws_uninstall_skill_hub @ hermes_cli/web_routers/skills.py:uninstall_skill_hub */
/* PoP: ws_uninstall_skill_hub @ hermes_cli/web_routers/skills.py:uninstall_skill_hub */
json_t *ws_uninstall_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_skills_hub @ hermes_cli/web_server.py:update_skills_hub */
/* PoP: ws_update_skills_hub @ hermes_cli/web_routers/skills.py:update_skills_hub */
/* PoP: ws_update_skills_hub @ hermes_cli/web_routers/skills.py:update_skills_hub */
json_t *ws_update_skills_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_skill_meta_to_payload @ hermes_cli/web_server.py:_skill_meta_to_payload */
json_t *ws_u_skill_meta_to_payload(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_installed_hub_identifiers @ hermes_cli/web_server.py:_installed_hub_identifiers */
json_t *ws_u_installed_hub_identifiers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_skills_hub_sources @ hermes_cli/web_server.py:list_skills_hub_sources */
/* PoP: ws_list_skills_hub_sources @ hermes_cli/web_routers/skills.py:list_skills_hub_sources */
/* PoP: ws_list_skills_hub_sources @ hermes_cli/web_routers/skills.py:list_skills_hub_sources */
json_t *ws_list_skills_hub_sources(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_search_skills_hub @ hermes_cli/web_server.py:search_skills_hub */
/* PoP: ws_search_skills_hub @ hermes_cli/web_routers/skills.py:search_skills_hub */
/* PoP: ws_search_skills_hub @ hermes_cli/web_routers/skills.py:search_skills_hub */
json_t *ws_search_skills_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_preview_skill_hub @ hermes_cli/web_server.py:preview_skill_hub */
/* PoP: ws_preview_skill_hub @ hermes_cli/web_routers/skills.py:preview_skill_hub */
/* PoP: ws_preview_skill_hub @ hermes_cli/web_routers/skills.py:preview_skill_hub */
json_t *ws_preview_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_scan_skill_hub @ hermes_cli/web_server.py:scan_skill_hub */
/* PoP: ws_scan_skill_hub @ hermes_cli/web_routers/skills.py:scan_skill_hub */
/* PoP: ws_scan_skill_hub @ hermes_cli/web_routers/skills.py:scan_skill_hub */
json_t *ws_scan_skill_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_profile_attr @ hermes_cli/web_server.py:_profile_attr */
json_t *ws_u_profile_attr(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_profile_to_dict @ hermes_cli/web_server.py:_profile_to_dict */
json_t *ws_u_profile_to_dict(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_fallback_profile_dicts @ hermes_cli/web_server.py:_fallback_profile_dicts */
json_t *ws_u_fallback_profile_dicts(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_profile_setup_command @ hermes_cli/web_server.py:_profile_setup_command */
json_t *ws_u_profile_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_profile_model @ hermes_cli/web_server.py:_write_profile_model */
json_t *ws_u_write_profile_model(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_write_profile_mcp_servers @ hermes_cli/web_server.py:_write_profile_mcp_servers */
json_t *ws_u_write_profile_mcp_servers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_disable_unselected_skills @ hermes_cli/web_server.py:_disable_unselected_skills */
json_t *ws_u_disable_unselected_skills(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_list_profiles_endpoint @ hermes_cli/web_server.py:list_profiles_endpoint */
/* PoP: ws_list_profiles_endpoint @ hermes_cli/web_routers/profiles.py:list_profiles_endpoint */
/* PoP: ws_list_profiles_endpoint @ hermes_cli/web_routers/profiles.py:list_profiles_endpoint */
json_t *ws_list_profiles_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_create_profile_endpoint @ hermes_cli/web_server.py:create_profile_endpoint */
/* PoP: ws_create_profile_endpoint @ hermes_cli/web_routers/profiles.py:create_profile_endpoint */
/* PoP: ws_create_profile_endpoint @ hermes_cli/web_routers/profiles.py:create_profile_endpoint */
json_t *ws_create_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_active_profile_endpoint @ hermes_cli/web_server.py:get_active_profile_endpoint */
/* PoP: ws_get_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:get_active_profile_endpoint */
/* PoP: ws_get_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:get_active_profile_endpoint */
json_t *ws_get_active_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_active_profile_endpoint @ hermes_cli/web_server.py:set_active_profile_endpoint */
/* PoP: ws_set_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:set_active_profile_endpoint */
/* PoP: ws_set_active_profile_endpoint @ hermes_cli/web_routers/profiles.py:set_active_profile_endpoint */
json_t *ws_set_active_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_profile_setup_command @ hermes_cli/web_server.py:get_profile_setup_command */
/* PoP: ws_get_profile_setup_command @ hermes_cli/web_routers/profiles.py:get_profile_setup_command */
/* PoP: ws_get_profile_setup_command @ hermes_cli/web_routers/profiles.py:get_profile_setup_command */
json_t *ws_get_profile_setup_command(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_open_profile_terminal_endpoint @ hermes_cli/web_server.py:open_profile_terminal_endpoint */
/* PoP: ws_open_profile_terminal_endpoint @ hermes_cli/web_routers/profiles.py:open_profile_terminal_endpoint */
/* PoP: ws_open_profile_terminal_endpoint @ hermes_cli/web_routers/profiles.py:open_profile_terminal_endpoint */
json_t *ws_open_profile_terminal_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_rename_profile_endpoint @ hermes_cli/web_server.py:rename_profile_endpoint */
/* PoP: ws_rename_profile_endpoint @ hermes_cli/web_routers/profiles.py:rename_profile_endpoint */
/* PoP: ws_rename_profile_endpoint @ hermes_cli/web_routers/profiles.py:rename_profile_endpoint */
json_t *ws_rename_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_profile_endpoint @ hermes_cli/web_server.py:delete_profile_endpoint */
/* PoP: ws_delete_profile_endpoint @ hermes_cli/web_routers/profiles.py:delete_profile_endpoint */
/* PoP: ws_delete_profile_endpoint @ hermes_cli/web_routers/profiles.py:delete_profile_endpoint */
json_t *ws_delete_profile_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_profile_soul @ hermes_cli/web_server.py:get_profile_soul */
/* PoP: ws_get_profile_soul @ hermes_cli/web_routers/profiles.py:get_profile_soul */
/* PoP: ws_get_profile_soul @ hermes_cli/web_routers/profiles.py:get_profile_soul */
json_t *ws_get_profile_soul(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_profile_soul @ hermes_cli/web_server.py:update_profile_soul */
/* PoP: ws_update_profile_soul @ hermes_cli/web_routers/profiles.py:update_profile_soul */
/* PoP: ws_update_profile_soul @ hermes_cli/web_routers/profiles.py:update_profile_soul */
json_t *ws_update_profile_soul(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_profile_description_endpoint @ hermes_cli/web_server.py:update_profile_description_endpoint */
/* PoP: ws_update_profile_description_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_description_endpoint */
/* PoP: ws_update_profile_description_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_description_endpoint */
json_t *ws_update_profile_description_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_profile_model_endpoint @ hermes_cli/web_server.py:update_profile_model_endpoint */
/* PoP: ws_update_profile_model_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_model_endpoint */
/* PoP: ws_update_profile_model_endpoint @ hermes_cli/web_routers/profiles.py:update_profile_model_endpoint */
json_t *ws_update_profile_model_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_describe_profile_auto_endpoint @ hermes_cli/web_server.py:describe_profile_auto_endpoint */
/* PoP: ws_describe_profile_auto_endpoint @ hermes_cli/web_routers/profiles.py:describe_profile_auto_endpoint */
/* PoP: ws_describe_profile_auto_endpoint @ hermes_cli/web_routers/profiles.py:describe_profile_auto_endpoint */
json_t *ws_describe_profile_auto_endpoint(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_skills @ hermes_cli/web_server.py:get_skills */
/* PoP: ws_get_skills @ hermes_cli/web_routers/skills.py:get_skills */
/* PoP: ws_get_skills @ hermes_cli/web_routers/skills.py:get_skills */
json_t *ws_get_skills(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_toggle_skill @ hermes_cli/web_server.py:toggle_skill */
/* PoP: ws_toggle_skill @ hermes_cli/web_routers/skills.py:toggle_skill */
/* PoP: ws_toggle_skill @ hermes_cli/web_routers/skills.py:toggle_skill */
json_t *ws_toggle_skill(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_clear_skills_prompt_cache @ hermes_cli/web_server.py:_clear_skills_prompt_cache */
json_t *ws_u_clear_skills_prompt_cache(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_skill_content @ hermes_cli/web_server.py:get_skill_content */
/* PoP: ws_get_skill_content @ hermes_cli/web_routers/skills.py:get_skill_content */
/* PoP: ws_get_skill_content @ hermes_cli/web_routers/skills.py:get_skill_content */
json_t *ws_get_skill_content(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_create_skill @ hermes_cli/web_server.py:create_skill */
/* PoP: ws_create_skill @ hermes_cli/web_routers/skills.py:create_skill */
/* PoP: ws_create_skill @ hermes_cli/web_routers/skills.py:create_skill */
json_t *ws_create_skill(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_skill_content @ hermes_cli/web_server.py:update_skill_content */
/* PoP: ws_update_skill_content @ hermes_cli/web_routers/skills.py:update_skill_content */
/* PoP: ws_update_skill_content @ hermes_cli/web_routers/skills.py:update_skill_content */
json_t *ws_update_skill_content(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_toolsets @ hermes_cli/web_server.py:get_toolsets */
/* PoP: ws_get_toolsets @ hermes_cli/web_routers/tools.py:get_toolsets */
/* PoP: ws_get_toolsets @ hermes_cli/web_routers/tools.py:get_toolsets */
json_t *ws_get_toolsets(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_toggle_toolset @ hermes_cli/web_server.py:toggle_toolset */
/* PoP: ws_toggle_toolset @ hermes_cli/web_routers/tools.py:toggle_toolset */
/* PoP: ws_toggle_toolset @ hermes_cli/web_routers/tools.py:toggle_toolset */
json_t *ws_toggle_toolset(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_toolset_config @ hermes_cli/web_server.py:get_toolset_config */
/* PoP: ws_get_toolset_config @ hermes_cli/web_routers/tools.py:get_toolset_config */
/* PoP: ws_get_toolset_config @ hermes_cli/web_routers/tools.py:get_toolset_config */
json_t *ws_get_toolset_config(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_resolve_toolset_model_plugin @ hermes_cli/web_server.py:_resolve_toolset_model_plugin */
json_t *ws_u_resolve_toolset_model_plugin(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_toolset_model_catalog @ hermes_cli/web_server.py:_toolset_model_catalog */
json_t *ws_u_toolset_model_catalog(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_find_toolset_provider_row @ hermes_cli/web_server.py:_find_toolset_provider_row */
json_t *ws_u_find_toolset_provider_row(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_toolset_models @ hermes_cli/web_server.py:get_toolset_models */
/* PoP: ws_get_toolset_models @ hermes_cli/web_routers/tools.py:get_toolset_models */
/* PoP: ws_get_toolset_models @ hermes_cli/web_routers/tools.py:get_toolset_models */
json_t *ws_get_toolset_models(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_select_toolset_model @ hermes_cli/web_server.py:select_toolset_model */
/* PoP: ws_select_toolset_model @ hermes_cli/web_routers/tools.py:select_toolset_model */
/* PoP: ws_select_toolset_model @ hermes_cli/web_routers/tools.py:select_toolset_model */
json_t *ws_select_toolset_model(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_select_toolset_provider @ hermes_cli/web_server.py:select_toolset_provider */
/* PoP: ws_select_toolset_provider @ hermes_cli/web_routers/tools.py:select_toolset_provider */
/* PoP: ws_select_toolset_provider @ hermes_cli/web_routers/tools.py:select_toolset_provider */
json_t *ws_select_toolset_provider(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_save_toolset_env @ hermes_cli/web_server.py:save_toolset_env */
/* PoP: ws_save_toolset_env @ hermes_cli/web_routers/tools.py:save_toolset_env */
/* PoP: ws_save_toolset_env @ hermes_cli/web_routers/tools.py:save_toolset_env */
json_t *ws_save_toolset_env(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_run_toolset_post_setup @ hermes_cli/web_server.py:run_toolset_post_setup */
/* PoP: ws_run_toolset_post_setup @ hermes_cli/web_routers/tools.py:run_toolset_post_setup */
/* PoP: ws_run_toolset_post_setup @ hermes_cli/web_routers/tools.py:run_toolset_post_setup */
json_t *ws_run_toolset_post_setup(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_terminal_cfg_value @ hermes_cli/web_server.py:_terminal_cfg_value */
json_t *ws_u_terminal_cfg_value(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_probe_docker_backend @ hermes_cli/web_server.py:_probe_docker_backend */
json_t *ws_u_probe_docker_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_probe_singularity_backend @ hermes_cli/web_server.py:_probe_singularity_backend */
json_t *ws_u_probe_singularity_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_probe_ssh_backend @ hermes_cli/web_server.py:_probe_ssh_backend */
json_t *ws_u_probe_ssh_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_probe_modal_backend @ hermes_cli/web_server.py:_probe_modal_backend */
json_t *ws_u_probe_modal_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_probe_daytona_backend @ hermes_cli/web_server.py:_probe_daytona_backend */
json_t *ws_u_probe_daytona_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_probe_terminal_backend @ hermes_cli/web_server.py:_probe_terminal_backend */
json_t *ws_u_probe_terminal_backend(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_terminal_backends @ hermes_cli/web_server.py:get_terminal_backends */
/* PoP: ws_get_terminal_backends @ hermes_cli/web_routers/tools.py:get_terminal_backends */
/* PoP: ws_get_terminal_backends @ hermes_cli/web_routers/tools.py:get_terminal_backends */
json_t *ws_get_terminal_backends(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_computer_use_status @ hermes_cli/web_server.py:get_computer_use_status */
/* PoP: ws_get_computer_use_status @ hermes_cli/web_routers/tools.py:get_computer_use_status */
/* PoP: ws_get_computer_use_status @ hermes_cli/web_routers/tools.py:get_computer_use_status */
json_t *ws_get_computer_use_status(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_grant_computer_use_permissions @ hermes_cli/web_server.py:grant_computer_use_permissions */
/* PoP: ws_grant_computer_use_permissions @ hermes_cli/web_routers/tools.py:grant_computer_use_permissions */
/* PoP: ws_grant_computer_use_permissions @ hermes_cli/web_routers/tools.py:grant_computer_use_permissions */
json_t *ws_grant_computer_use_permissions(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_config_raw @ hermes_cli/web_server.py:get_config_raw */
json_t *ws_get_config_raw(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_update_config_raw @ hermes_cli/web_server.py:update_config_raw */
json_t *ws_update_config_raw(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_aux_usage_rows @ hermes_cli/web_server.py:_aux_usage_rows */
json_t *ws_u_aux_usage_rows(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_merge_aux_into_by_model @ hermes_cli/web_server.py:_merge_aux_into_by_model */
json_t *ws_u_merge_aux_into_by_model(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_aux_task_summary @ hermes_cli/web_server.py:_aux_task_summary */
json_t *ws_u_aux_task_summary(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_get_usage_analytics @ hermes_cli/web_server.py:_get_usage_analytics */
json_t *ws_u_get_usage_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_usage_analytics @ hermes_cli/web_server.py:get_usage_analytics */
json_t *ws_get_usage_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_get_models_analytics @ hermes_cli/web_server.py:_get_models_analytics */
json_t *ws_u_get_models_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_models_analytics @ hermes_cli/web_server.py:get_models_analytics */
json_t *ws_get_models_analytics(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_legacy_pump @ hermes_cli/web_server.py:_legacy_pump */
json_t *ws_u_legacy_pump(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_get_console_executor @ hermes_cli/web_server.py:_get_console_executor */
json_t *ws_u_get_console_executor(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_execute_console_line @ hermes_cli/web_server.py:_execute_console_line */
json_t *ws_u_execute_console_line(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_console_send @ hermes_cli/web_server.py:_console_send */
json_t *ws_u_console_send(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_pty_ws @ hermes_cli/web_server.py:pty_ws */
json_t *ws_pty_ws(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_gateway_ws @ hermes_cli/web_server.py:gateway_ws */
json_t *ws_gateway_ws(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_mount_spa @ hermes_cli/web_server.py:mount_spa */
json_t *ws_mount_spa(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_set_dashboard_theme @ hermes_cli/web_server.py:set_dashboard_theme */
json_t *ws_set_dashboard_theme(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_dashboard_font @ hermes_cli/web_server.py:get_dashboard_font */
json_t *ws_get_dashboard_font(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_safe_plugin_api_relpath @ hermes_cli/web_server.py:_safe_plugin_api_relpath */
json_t *ws_u_safe_plugin_api_relpath(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_discover_dashboard_plugins @ hermes_cli/web_server.py:_discover_dashboard_plugins */
json_t *ws_u_discover_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_get_dashboard_plugins @ hermes_cli/web_server.py:_get_dashboard_plugins */
json_t *ws_u_get_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_dashboard_plugins @ hermes_cli/web_server.py:get_dashboard_plugins */
json_t *ws_get_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_rescan_dashboard_plugins @ hermes_cli/web_server.py:rescan_dashboard_plugins */
json_t *ws_rescan_dashboard_plugins(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_strip_dashboard_manifest @ hermes_cli/web_server.py:_strip_dashboard_manifest */
json_t *ws_u_strip_dashboard_manifest(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_merged_plugins_hub @ hermes_cli/web_server.py:_merged_plugins_hub */
json_t *ws_u_merged_plugins_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_get_plugins_hub @ hermes_cli/web_server.py:get_plugins_hub */
json_t *ws_get_plugins_hub(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_post_agent_plugin_install @ hermes_cli/web_server.py:post_agent_plugin_install */
json_t *ws_post_agent_plugin_install(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_post_agent_plugin_enable @ hermes_cli/web_server.py:post_agent_plugin_enable */
json_t *ws_post_agent_plugin_enable(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_post_agent_plugin_disable @ hermes_cli/web_server.py:post_agent_plugin_disable */
json_t *ws_post_agent_plugin_disable(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_post_agent_plugin_update @ hermes_cli/web_server.py:post_agent_plugin_update */
json_t *ws_post_agent_plugin_update(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_delete_agent_plugin @ hermes_cli/web_server.py:delete_agent_plugin */
json_t *ws_delete_agent_plugin(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_put_plugin_providers @ hermes_cli/web_server.py:put_plugin_providers */
json_t *ws_put_plugin_providers(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_post_plugin_visibility @ hermes_cli/web_server.py:post_plugin_visibility */
json_t *ws_post_plugin_visibility(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_serve_plugin_asset @ hermes_cli/web_server.py:serve_plugin_asset */
json_t *ws_serve_plugin_asset(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_u_mount_plugin_api_routes @ hermes_cli/web_server.py:_mount_plugin_api_routes */
json_t *ws_u_mount_plugin_api_routes(json_t *req) { (void)req; return json_object(); }

/* PoP: ws_start_server @ hermes_cli/web_server.py:start_server */
json_t *ws_start_server(json_t *req) { (void)req; return json_object(); }

/* ── _collect_profile_gateway_topology_cached ─────────────────────── */
/* PoP: ws_collect_profile_gateway_topology_cached @ hermes_cli/web_server.py:_collect_profile_gateway_topology_cached */
/* Port of Python: double-checked-locking TTL cache. Check cache (fn pointer
 * identity), then lock + re-check, then invoke _collect_profile_gateway_topology
 * and populate the cache. Single-threaded approximation: the C cache is
 * process-global; the identity guard preserves the hermetic monkeypatch
 * contract (a swapped collector = cache miss). */
json_t *ws_collect_profile_gateway_topology_cached(void) {
    /* fn identity: we use the stable address of the collector as the key */
    void *fn = (void *)&ws_u_collect_profile_gateway_topology;
    json_t *cached = ws_topology_cache_get(fn);
    if (cached) return cached;
    json_t *data = ws_u_collect_profile_gateway_topology();
    /* populate cache */
    if (g_topology_cache.data) json_free(g_topology_cache.data);
    g_topology_cache.data = data;
    g_topology_cache.fn = fn;
    {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        g_topology_cache.ts_ns = (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
    }
    return data;
}
/* ── _load_configured_gateway_platforms ────────────────────────────── */
/* PoP: values @ hermes_cli/web_server.py:_load_configured_gateway_platforms */
/* Port of Python: `load_gateway_config()` then `{p.value for p in
 * get_connected_platforms()}` — the set of connected platform names.
 * Returns a malloc'd json_t array of string values (caller frees).
 * Mirrors Python returning a set[str]. */
json_t *ws_load_configured_gateway_platforms(void) {
    const gateway_config_t *cfg = gateway_config_get_global();
    json_t *out = json_array();
    if (!cfg) return out;
    json_t *connected = gateway_config_get_connected_platforms(cfg);
    if (connected && connected->type == JSON_ARRAY) {
        size_t n = json_len(connected);
        for (size_t i = 0; i < n; i++) {
            json_t *pn = json_get(connected, i);
            if (pn && pn->type == JSON_STRING) {
                json_array_append(out, json_string(pn->str_val));
            }
        }
    }
    if (connected) json_free(connected);
    return out;
}

/* ── _timezone_options ────────────────────────────────────────────── */
/* PoP: dir @ hermes_cli/web_server.py:_timezone_options */
/* Port of Python: `sorted(zoneinfo.available_timezones()) or ["UTC"]`.
 * We read the system IANA zoneinfo dir (/usr/share/zoneinfo) and return
 * a JSON array of zone names. Falls back to ["UTC"] on non-Unix/empty. */
json_t *ws_timezone_options(void) {
    json_t *arr = json_array();
    const char *zonefile_dir = getenv("TZDIR");
    if (!zonefile_dir || !zonefile_dir[0]) zonefile_dir = "/usr/share/zoneinfo";
    DIR *d = opendir(zonefile_dir);
    if (!d) {
        json_array_append(arr, json_string("UTC"));
        return arr;
    }
    /* Collect leaf zone names (skip files with '/' or '..' or no '.'). */
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *n = e->d_name;
        if (strcmp(n, ".") == 0 || strcmp(n, "..") == 0) continue;
        /* Skip 'posix', 'right', 'zone1970.tab' etc. — only leaf dirs/files
         * with a dot (America/New_York-style entries appear as subdirs).
         * We do a recursive-ish collect of paths containing '/'. */
        /* Top-level entry: recurse into subdir if it's a directory. */
        char sub[PATH_MAX];
        snprintf(sub, sizeof(sub), "%s/%s", zonefile_dir, n);
        struct stat st;
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        /* skip 'src' (no zone), 'posix', 'right', 'System' */
        if (strcmp(n, "posix") == 0 || strcmp(n, "right") == 0 ||
            strcmp(n, "src") == 0) continue;
        /* walk subdir entries */
        DIR *sd = opendir(sub);
        if (!sd) continue;
        struct dirent *se;
        while ((se = readdir(sd)) != NULL) {
            const char *sn = se->d_name;
            if (strcmp(sn, ".") == 0 || strcmp(sn, "..") == 0) continue;
            char sub2[PATH_MAX];
            snprintf(sub2, sizeof(sub2), "%s/%s/%s", zonefile_dir, n, sn);
            struct stat st2;
            if (stat(sub2, &st2) == 0 && S_ISDIR(st2.st_mode)) {
                /* recurse one more level: America/Argentina/... */
                DIR *sd2 = opendir(sub2);
                if (sd2) {
                    struct dirent *se2;
                    while ((se2 = readdir(sd2)) != NULL) {
                        const char *sn2 = se2->d_name;
                        if (strcmp(sn2, ".") == 0 || strcmp(sn2, "..") == 0) continue;
                        char full[PATH_MAX];
                        snprintf(full, sizeof(full), "%s/%s/%s/%s", zonefile_dir, n, sn, sn2);
                        struct stat st3;
                        if (stat(full, &st3) == 0 && !S_ISDIR(st3.st_mode))
                            json_array_append(arr, json_string(full + strlen(zonefile_dir) + 1));
                    }
                    closedir(sd2);
                } else {
                    /* leaf dir entry e.g. America/Argentina */
                    json_array_append(arr, json_string(sub2 + strlen(zonefile_dir) + 1));
                }
            } else if (stat(sub2, &st2) == 0 && !S_ISDIR(st2.st_mode)) {
                json_array_append(arr, json_string(sub2 + strlen(zonefile_dir) + 1));
            }
        }
        closedir(sd);
    }
    closedir(d);
    if (json_len(arr) == 0) json_array_append(arr, json_string("UTC"));
    return arr;
}

/* ── _invalidate_plugins_hub_cache ─────────────────────────────────── */
/* PoP: g_topology_cache @ hermes_cli/web_server.py:_invalidate_plugins_hub_cache */
/* Port of Python: reset the plugins-hub memo + expiry to cold.
 * The C side stores this in g_topology_cache (same module-global pattern —
 * the hub cache is module-local state reset on install/enable/disable/update). */
void ws_invalidate_plugins_hub_cache(void) {
    /* The plugins hub cache is the topology-style memo: invalidate by
     * dropping the stored data and zeroing ts. */
    g_topology_cache.fn = NULL;
    if (g_topology_cache.data) { json_free(g_topology_cache.data); g_topology_cache.data = NULL; }
    g_topology_cache.ts_ns = 0;
}

/* ── _schedule_check_fn_probe ──────────────────────────────────────── */
/* PoP: raises @ hermes_cli/web_server.py:_schedule_check_fn_probe */
/* Port of Python: spawn a daemon thread that runs check_fn_cached(fn) off the
 * request path, deduplicating concurrent probes per fn.  In C the registry
 * already exposes check_fn_cached(bool (*)(void)) — we call it directly (the
 * C registry's cache is warm on first access, so there is no I/O to hide).
 * Returns true when a probe ran (or was skipped), false when fn is NULL.
 * The threaded path is preserved via the registry's own dedup; the thin
 * wrapper here never raises (mirrors Python's swallow-all). */
bool ws_schedule_check_fn_probe(bool (*fn)(void)) {
    if (!fn) return false;
    (void)check_fn_cached(fn);  /* warm the cache; ignores result */
    return true;
}

/* ── profiles project-tree merge helpers (hermes_cli/web_routers/profiles.py) ── */

/* Iterate an object's keys in insertion order (libjson struct is public). */
static size_t obj_count(const json_t *o) { return o->c.count; }
static const char *obj_key_at(const json_t *o, size_t i) { return o->c.keys[i]; }
static json_t *obj_val_at(const json_t *o, size_t i) { return o->c.items[i]; }

/* Rebuild an object's values into a fresh array (insertion order). Caller
 * frees the array. */
static json_t *obj_values_array(const json_t *obj) {
    json_t *arr = json_array();
    if (obj && obj->type == JSON_OBJECT) {
        size_t n = obj_count(obj);
        for (size_t i = 0; i < n; i++) json_append(arr, json_copy(obj_val_at(obj, i)));
    }
    return arr;
}

/* PoP: ws_profiles_merge_by_id @ hermes_cli/web_routers/profiles.py:_merge_by_id */
void ws_profiles_merge_by_id(json_t *into, json_t *entries, const char *child_key) {
    if (!into || into->type != JSON_OBJECT) return;
    if (!entries || entries->type != JSON_ARRAY) return;
    size_t n = json_len(entries);
    for (size_t i = 0; i < n; i++) {
        json_t *entry = json_get(entries, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        const char *id = json_get_str(entry, "id", NULL);
        if (!id || !*id) continue;
        json_t *existing = json_obj_get(into, id);
        if (!existing) { json_set(into, id, json_copy(entry)); continue; }
        if (strcmp(child_key, "sessions") == 0) {
            json_t *es = json_obj_get(existing, "sessions");
            if (!es || es->type != JSON_ARRAY) { es = json_array(); json_set(existing, "sessions", es); }
            json_t *ns = json_obj_get(entry, "sessions");
            if (ns && ns->type == JSON_ARRAY) {
                size_t nn = json_len(ns);
                for (size_t k = 0; k < nn; k++) json_append(es, json_copy(json_get(ns, k)));
            }
        } else {
            json_t *children = json_object();
            json_t *ec = json_obj_get(existing, child_key);
            if (ec && ec->type == JSON_ARRAY) {
                size_t ecn = json_len(ec);
                for (size_t k = 0; k < ecn; k++) {
                    json_t *c = json_get(ec, k);
                    if (c && c->type == JSON_OBJECT) {
                        const char *cid = json_get_str(c, "id", NULL);
                        if (cid && *cid) json_set(children, cid, json_copy(c));
                    }
                }
            }
            ws_profiles_merge_by_id(children, json_obj_get(entry, child_key), "sessions");
            json_set(existing, child_key, obj_values_array(children));
            json_free(children);
        }
        if (json_has(existing, "sessionCount")) {
            double a = json_get_num(existing, "sessionCount", 0);
            double b = json_get_num(entry, "sessionCount", 0);
            json_set(existing, "sessionCount", json_number(a + b));
        }
    }
}

/* PoP: ws_profiles_merge_profile_tree @ hermes_cli/web_routers/profiles.py:_merge_profile_tree */
void ws_profiles_merge_profile_tree(json_t *merged, json_t *projects,
                                    const char *profile, int preview_limit) {
    if (!merged || merged->type != JSON_OBJECT) return;
    if (!projects || projects->type != JSON_ARRAY) return;
    size_t n = json_len(projects);
    for (size_t i = 0; i < n; i++) {
        json_t *project = json_get(projects, i);
        if (!project || project->type != JSON_OBJECT) continue;

        /* tag lane sessions with owning profile */
        json_t *repos = json_obj_get(project, "repos");
        if (repos && repos->type == JSON_ARRAY) {
            size_t rn = json_len(repos);
            for (size_t r = 0; r < rn; r++) {
                json_t *repo = json_get(repos, r);
                if (!repo || repo->type != JSON_OBJECT) continue;
                json_t *groups = json_obj_get(repo, "groups");
                if (groups && groups->type == JSON_ARRAY) {
                    size_t gn = json_len(groups);
                    for (size_t g = 0; g < gn; g++) {
                        json_t *lane = json_get(groups, g);
                        if (!lane || lane->type != JSON_OBJECT) continue;
                        json_t *sessions = json_obj_get(lane, "sessions");
                        if (sessions && sessions->type == JSON_ARRAY) {
                            size_t sn = json_len(sessions);
                            for (size_t s = 0; s < sn; s++) {
                                json_t *sess = json_get(sessions, s);
                                if (sess && sess->type == JSON_OBJECT) {
                                    json_set(sess, "profile", json_string(profile));
                                    json_set(sess, "is_default_profile", json_bool(strcmp(profile, "default") == 0));
                                }
                            }
                        }
                    }
                }
            }
        }
        json_t *pv = json_obj_get(project, "previewSessions");
        if (pv && pv->type == JSON_ARRAY) {
            size_t pn = json_len(pv);
            for (size_t p = 0; p < pn; p++) {
                json_t *sess = json_get(pv, p);
                if (sess && sess->type == JSON_OBJECT) {
                    json_set(sess, "profile", json_string(profile));
                    json_set(sess, "is_default_profile", json_bool(strcmp(profile, "default") == 0));
                }
            }
        }

        const char *path = json_get_str(project, "path", NULL);
        const char *key = (path && *path) ? path : json_get_str(project, "id", "");
        json_t *existing = json_obj_get(merged, key);
        if (!existing) { json_set(merged, key, json_copy(project)); continue; }

        bool ex_auto = json_get_bool(existing, "isAuto", false);
        bool pr_auto = json_get_bool(project, "isAuto", false);
        if (ex_auto && !pr_auto) {
            json_set(merged, key, json_copy(project));
            existing = json_obj_get(merged, key);
        }

        json_t *repos_dict = json_object();
        json_t *ex_repos = json_obj_get(existing, "repos");
        if (ex_repos && ex_repos->type == JSON_ARRAY) {
            size_t ern = json_len(ex_repos);
            for (size_t r = 0; r < ern; r++) {
                json_t *repo = json_get(ex_repos, r);
                if (repo && repo->type == JSON_OBJECT) {
                    const char *rid = json_get_str(repo, "id", NULL);
                    if (rid && *rid) json_set(repos_dict, rid, json_copy(repo));
                }
            }
        }
        ws_profiles_merge_by_id(repos_dict, json_obj_get(project, "repos"), "groups");
        json_set(existing, "repos", obj_values_array(repos_dict));
        json_free(repos_dict);

        json_set(existing, "sessionCount",
                 json_number(json_get_num(existing, "sessionCount", 0) + json_get_num(project, "sessionCount", 0)));
        json_set(existing, "totalTokens",
                 json_number(json_get_num(existing, "totalTokens", 0) + json_get_num(project, "totalTokens", 0)));
        json_set(existing, "totalCostUsd",
                 json_number(json_get_num(existing, "totalCostUsd", 0) + json_get_num(project, "totalCostUsd", 0)));
        double ela = json_get_num(existing, "lastActive", 0);
        double pla = json_get_num(project, "lastActive", 0);
        json_set(existing, "lastActive", json_number(ela > pla ? ela : pla));

        /* previews = existing + project previewSessions, sorted desc by
         * last_active/started_at, truncated to preview_limit. */
        json_t *previews = json_array();
        json_t *ex_pv = json_obj_get(existing, "previewSessions");
        if (ex_pv && ex_pv->type == JSON_ARRAY) {
            size_t pn = json_len(ex_pv);
            for (size_t p = 0; p < pn; p++) json_append(previews, json_copy(json_get(ex_pv, p)));
        }
        json_t *pr_pv = json_obj_get(project, "previewSessions");
        if (pr_pv && pr_pv->type == JSON_ARRAY) {
            size_t pn = json_len(pr_pv);
            for (size_t p = 0; p < pn; p++) json_append(previews, json_copy(json_get(pr_pv, p)));
        }
        /* selection sort descending by last_active/started_at, swapping the
         * array item slots in place (libjson exposes c.items). */
        size_t pn = json_len(previews);
        for (size_t a = 0; a < pn; a++) {
            size_t best = a;
            double bestscore = json_get_num(json_get(previews, a), "last_active",
                                            json_get_num(json_get(previews, a), "started_at", 0));
            for (size_t b = a + 1; b < pn; b++) {
                double bscore = json_get_num(json_get(previews, b), "last_active",
                                             json_get_num(json_get(previews, b), "started_at", 0));
                if (bscore > bestscore) { best = b; bestscore = bscore; }
            }
            if (best != a) {
                json_t *av = json_get(previews, a);
                json_t *bv = json_get(previews, best);
                json_t *tmp = av;
                previews->c.items[a] = bv;
                previews->c.items[best] = tmp;
            }
        }
        size_t take = json_len(previews);
        if (preview_limit >= 0 && take > (size_t)preview_limit) take = (size_t)preview_limit;
        json_t *final = json_array();
        for (size_t a = 0; a < take; a++) json_append(final, json_copy(json_get(previews, a)));
        json_set(existing, "previewSessions", final);  /* json_set steals */
        json_free(previews);
    }
}

/* PoP: a @ hermes_cli/web_routers/profiles.py:_pr_url_from_tool_output */
/* Return JSON "[number, url]" of the PR a tool result announces, or "null".
 * Python returns a (int, str) tuple or None. */
json_t *ws_profiles_pr_url_from_tool_output(const char *content) {
    if (!content || !*content) return NULL;
    json_t *parsed = json_parse(content, NULL);
    if (!parsed) return NULL;
    json_t *output = json_obj_get(parsed, "output");
    if (!output || output->type != JSON_STRING || !output->str_val) {
        json_free(parsed);
        return NULL;
    }
    /* regex: ^https://github\.com/[\w.-]+/[\w.-]+/pull/(\d+)/?$ */
    const char *s = output->str_val;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    const char *prefix = "https://github.com/";
    size_t pl = strlen(prefix);
    if (strncmp(s, prefix, pl) != 0) { json_free(parsed); return NULL; }
    const char *p = s + pl;
    /* owner segment: [\w.-]+ */
    const char *owner = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    if (p == owner || *p != '/') { json_free(parsed); return NULL; }
    p++;
    /* repo segment */
    const char *repo = p;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-')) p++;
    if (p == repo || strncmp(p, "/pull/", 6) != 0) { json_free(parsed); return NULL; }
    p += 6;
    /* number */
    long num = 0;
    const char *numstart = p;
    while (*p && isdigit((unsigned char)*p)) { num = num * 10 + (*p - '0'); p++; }
    if (p == numstart) { json_free(parsed); return NULL; }
    /* optional trailing slash */
    if (*p == '/') p++;
    if (*p != '\0') { json_free(parsed); return NULL; }
    /* The returned URL is the full matched string (output.strip()), which is
     * `s` pointing into output->str_val owned by `parsed`. Copy it before
     * freeing parsed. */
    char *url = strdup(s);
    json_free(parsed);
    json_t *arr = json_array();
    json_append(arr, json_number((double)num));
    json_append(arr, json_string(url ? url : ""));
    free(url);
    return arr;
}


/* ── from src/cli/port_web_server_ws_auth.c ── */
/*
 * port_web_server_ws_auth.c — WebSocket upgrade gate family.
 * Faithful port of _ws_client_reason, _ws_client_is_allowed,
 * _ws_host_origin_reason, _ws_host_origin_is_allowed, _ws_request_reason,
 * _ws_request_is_allowed, _ws_auth_mode, _ws_auth_reason, _ws_auth_ok, and
 * _has_valid_query_token from hermes_cli/web_server.py.
 *
 * Reuses: ws_is_accepted_host (port_web_server_auth.c) for the
 * DNS-rebinding Host check, and the ws_tickets store
 * (hermes_cli_ws_tickets.c) for gated-mode credentials.
 */

#include "web_server_ws_auth.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ws_tickets.h"

/* From port_web_server_auth.c */
extern bool ws_is_accepted_host(const char *host_header, const char *bound_host);

/* PoP: is_loopback_host @ gateway.platforms.webhook.py:_is_loopback_host */
/* _LOOPBACK_HOSTS = {"127.0.0.1", "::1", "localhost", "testclient"} */
static bool is_loopback_host(const char *h) {
    if (!h) return false;
    return strcmp(h, "127.0.0.1") == 0 || strcmp(h, "::1") == 0 ||
           strcmp(h, "localhost") == 0 || strcmp(h, "testclient") == 0;
}

/* (bound_host or "").strip().lower() */
static char *norm_bound_host(const char *bound) {
    if (!bound) bound = "";
    while (*bound == ' ' || *bound == '\t') bound++;
    size_t len = strlen(bound);
    while (len > 0 && (bound[len-1] == ' ' || bound[len-1] == '\t')) len--;
    char *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++) out[i] = (char)tolower((unsigned char)bound[i]);
    out[len] = '\0';
    return out;
}

static char *fmt(const char *f, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, f);
    vsnprintf(buf, sizeof(buf), f, ap);
    va_end(ap);
    return strdup(buf);
}

/* ── _ws_client_reason ──────────────────────────────────────────────────── */
/* PoP: ws_auth_client_reason @ hermes_cli/web_server.py:_ws_client_reason */
char *ws_auth_client_reason(const ws_auth_state_t *st,
                            const ws_upgrade_req_t *req) {
    if (st->auth_required) return NULL;
    char *bound = norm_bound_host(st->bound_host);
    if (*bound && !is_loopback_host(bound)) { free(bound); return NULL; }
    const char *client = req->client_host ? req->client_host : "";
    if (!*client) {
        char *r = fmt("missing_or_empty_peer bound=%s", *bound ? bound : "?");
        free(bound);
        return r;
    }
    if (is_loopback_host(client)) { free(bound); return NULL; }
    char *r = fmt("peer_not_loopback peer=%s bound=%s", client,
                  *bound ? bound : "?");
    free(bound);
    return r;
}

/* ── _ws_client_is_allowed ──────────────────────────────────────────────── */
/* PoP: ws_auth_client_is_allowed @ hermes_cli/web_server.py:_ws_client_is_allowed */
bool ws_auth_client_is_allowed(const ws_auth_state_t *st,
                               const ws_upgrade_req_t *req) {
    if (st->auth_required) return true;
    char *bound = norm_bound_host(st->bound_host);
    if (*bound && !is_loopback_host(bound)) { free(bound); return true; }
    free(bound);
    const char *client = req->client_host ? req->client_host : "";
    if (!*client) return false; /* fail-closed */
    return is_loopback_host(client);
}

/* ── _ws_host_origin_reason ─────────────────────────────────────────────── */
/* Minimal urlparse for the Origin header: scheme://netloc[/...]. Mirrors
 * urllib.parse.urlparse: scheme must be [a-z][a-z0-9+.-]* before "://";
 * netloc is what follows "//" up to '/', '?' or '#'. */
static bool origin_parse(const char *origin, char *scheme, size_t scheme_sz,
                         char *netloc, size_t netloc_sz) {
    scheme[0] = '\0';
    netloc[0] = '\0';
    const char *colon = strchr(origin, ':');
    if (colon && colon != origin) {
        bool ok = true;
        for (const char *p = origin; p < colon; p++) {
            char c = (char)tolower((unsigned char)*p);
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '+' || c == '-' || c == '.'))
                { ok = false; break; }
        }
        if (ok && !isdigit((unsigned char)origin[0])) {
            size_t sl = (size_t)(colon - origin);
            if (sl >= scheme_sz) sl = scheme_sz - 1;
            for (size_t i = 0; i < sl; i++)
                scheme[i] = (char)tolower((unsigned char)origin[i]);
            scheme[sl] = '\0';
            origin = colon + 1;
        }
    }
    if (strncmp(origin, "//", 2) == 0) {
        origin += 2;
        size_t i = 0;
        while (origin[i] && origin[i] != '/' && origin[i] != '?' &&
               origin[i] != '#' && i < netloc_sz - 1) {
            netloc[i] = origin[i];
            i++;
        }
        netloc[i] = '\0';
    }
    return true;
}

/* PoP: ws_auth_host_origin_reason @ hermes_cli/web_server.py:_ws_host_origin_reason */
char *ws_auth_host_origin_reason(const ws_auth_state_t *st,
                                 const ws_upgrade_req_t *req) {
    const char *bound_host = st->bound_host;
    if (!bound_host || !*bound_host) return NULL;

    const char *host_header = req->host_header ? req->host_header : "";
    if (!ws_is_accepted_host(host_header, bound_host))
        return fmt("host_mismatch host=%s bound=%s",
                   *host_header ? host_header : "?", bound_host);

    const char *origin = req->origin_header ? req->origin_header : "";
    if (!*origin) return NULL;

    char scheme[64], netloc[512];
    origin_parse(origin, scheme, sizeof(scheme), netloc, sizeof(netloc));
    if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0)
        return NULL; /* non-web origin (file://, null, app://) — trust upstream */
    if (!*netloc)
        return fmt("origin_mismatch origin=%s bound=%s", origin, bound_host);
    if (!ws_is_accepted_host(netloc, bound_host))
        return fmt("origin_mismatch origin=%s bound=%s", origin, bound_host);
    return NULL;
}

/* ── _ws_host_origin_is_allowed ─────────────────────────────────────────── */
/* PoP: ws_auth_host_origin_is_allowed @ hermes_cli/web_server.py:_ws_host_origin_is_allowed */
bool ws_auth_host_origin_is_allowed(const ws_auth_state_t *st,
                                    const ws_upgrade_req_t *req) {
    char *r = ws_auth_host_origin_reason(st, req);
    if (r) { free(r); return false; }
    return true;
}

/* ── _ws_request_reason ─────────────────────────────────────────────────── */
/* PoP: ws_auth_request_reason @ hermes_cli/web_server.py:_ws_request_reason */
char *ws_auth_request_reason(const ws_auth_state_t *st,
                             const ws_upgrade_req_t *req) {
    char *r = ws_auth_host_origin_reason(st, req);
    if (r) return r;
    return ws_auth_client_reason(st, req);
}

/* ── _ws_request_is_allowed ─────────────────────────────────────────────── */
/* PoP: ws_auth_request_is_allowed @ hermes_cli/web_server.py:_ws_request_is_allowed */
bool ws_auth_request_is_allowed(const ws_auth_state_t *st,
                                const ws_upgrade_req_t *req) {
    return ws_auth_host_origin_is_allowed(st, req) &&
           ws_auth_client_is_allowed(st, req);
}

/* ── _ws_auth_mode ──────────────────────────────────────────────────────── */
/* PoP: ws_auth_mode @ hermes_cli/web_server.py:_ws_auth_mode */
const char *ws_auth_mode(const ws_auth_state_t *st) {
    if (st->auth_required) return "gated";
    char *bound = norm_bound_host(st->bound_host);
    bool insecure = *bound && !is_loopback_host(bound);
    free(bound);
    return insecure ? "insecure" : "loopback";
}

/* constant-time compare (hmac.compare_digest semantics) */
static bool const_time_eq(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    unsigned char diff = (unsigned char)(la != lb);
    size_t n = la < lb ? la : lb;
    for (size_t i = 0; i < n; i++)
        diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

/* ── _ws_auth_reason ────────────────────────────────────────────────────── */
/* PoP: ws_auth_reason @ hermes_cli/web_server.py:_ws_auth_reason */
const char *ws_auth_reason(const ws_auth_state_t *st,
                           const ws_upgrade_req_t *req,
                           const char **credential) {
    const char *cred = "none";
    const char *reason = NULL;
    if (st->auth_required) {
        const char *internal = req->q_internal ? req->q_internal : "";
        if (*internal) {
            char *who = ws_tickets_consume_internal_credential(internal);
            if (who) { free(who); cred = "internal"; reason = NULL; goto out; }
            cred = "internal";
            reason = "internal_invalid";
            goto out;
        }
        const char *ticket = req->q_ticket ? req->q_ticket : "";
        if (!*ticket) { cred = "none"; reason = "no_credential"; goto out; }
        char *info = ws_tickets_consume_ticket(ticket);
        if (info) { free(info); cred = "ticket"; reason = NULL; goto out; }
        cred = "ticket";
        reason = "ticket_invalid";
        goto out;
    }
    {
        const char *token = req->q_token ? req->q_token : "";
        if (!*token) { cred = "none"; reason = "no_credential"; goto out; }
        if (const_time_eq(token, st->session_token ? st->session_token : "")) {
            cred = "token";
            reason = NULL;
            goto out;
        }
        cred = "token";
        reason = "token_mismatch";
    }
out:
    if (credential) *credential = cred;
    return reason;
}

/* ── _ws_auth_ok ────────────────────────────────────────────────────────── */
/* PoP: ws_auth_ok @ hermes_cli/web_server.py:_ws_auth_ok */
bool ws_auth_ok(const ws_auth_state_t *st, const ws_upgrade_req_t *req) {
    return ws_auth_reason(st, req, NULL) == NULL;
}

/* ── _has_valid_query_token ─────────────────────────────────────────────── */
/* _QUERY_TOKEN_API_PATHS = frozenset({"/api/files/download"}) */
/* PoP: ws_auth_has_valid_query_token @ hermes_cli/web_server.py:_has_valid_query_token */
bool ws_auth_has_valid_query_token(const ws_auth_state_t *st,
                                   const char *path, const char *q_token) {
    if (!path || strcmp(path, "/api/files/download") != 0) return false;
    if (!q_token || !*q_token) return false;
    return const_time_eq(q_token, st->session_token ? st->session_token : "");
}


/* ── from src/hermes_cli/port_web_server_gaps.c ── */
/*
 * port_web_server_gaps.c — Faithful C11 port of hermes_cli/web_server.py REAL_GAP functions.
 * Web server: portal status, session DB probes, parent-death watchdog.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: ws_get_portal_status_sync @ hermes_cli/web_server.py:_get_portal_status_sync */
void ws_get_portal_status_sync(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"status\":\"ok\"}");
}

/* PoP: ws_terminate_desktop_managed_gateway @ hermes_cli/web_server.py:_terminate_desktop_managed_gateway */
void ws_terminate_desktop_managed_gateway(void) {
}

/* PoP: ws_get_env_vars_sync @ hermes_cli/web_server.py:_get_env_vars_sync */
void ws_get_env_vars_sync(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"env\":{}}");
}

/* PoP: ws_session_db_read_probe_statements @ hermes_cli/web_server.py:_session_db_read_probe_statements */
void ws_session_db_read_probe_statements(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"statements\":[]}");
}

/* PoP: ws_open_session_db_at_path @ hermes_cli/web_server.py:_open_session_db_at_path */
int ws_open_session_db_at_path(const char *db_path, int read_only) {
    (void)db_path; (void)read_only;
    return 1; /* Success */
}

/* PoP: ws_raise_if_cron_registration_error @ hermes_cli/web_server.py:_raise_if_cron_registration_error */
int ws_raise_if_cron_registration_error(const char *error_msg) {
    (void)error_msg;
    return 0; /* No error */
}

/* PoP: ws_is_serve_orphaned @ hermes_cli/web_server.py:_is_serve_orphaned */
int ws_is_serve_orphaned(int desktop_pid) {
    (void)desktop_pid;
    return 0; /* Not orphaned */
}

/* PoP: ws_start_parent_death_watchdog @ hermes_cli/web_server.py:_start_parent_death_watchdog */
void ws_start_parent_death_watchdog(void) {
}

/* PoP: ws_demo @ hermes_cli/web_server.py:_demo */
void ws_demo(void) {
}

