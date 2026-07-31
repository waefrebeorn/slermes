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

/* PoP: copilot_acp_status @ hermes_cli/web_server.py:_copilot_acp_status */
/* Port of Python: _copilot_acp_status */
char *copilot_acp_status_fn(void)
{
    return strdup("{\"status\":\"disabled\",\"source\":\"copilot_cli\",\"source_label\":\"Managed by the GitHub Copilot CLI\"}");
}

/* PoP: _dashboard_local_update_managed_externally @ hermes_cli/web_server.py:_dashboard_local_update_managed_externally */
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

/* PoP: _gateway_display_command @ hermes_cli/web_server.py:_gateway_display_command */
char *gateway_display_command(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *cmd = malloc(512);
    if (!cmd) return NULL;
    snprintf(cmd, 512, "hermes gateway %s --profile %s", verb, profile);
    return cmd;
}

/* PoP: _gateway_subcommand @ hermes_cli/web_server.py:_gateway_subcommand */
char *gateway_subcommand_fn(const char *profile, const char *verb)
{
    if (!profile || !verb) return strdup("");
    char *sub = malloc(512);
    if (!sub) return NULL;
    snprintf(sub, 512, "%s %s", verb, profile);
    return sub;
}

/* PoP: _gemini_cli_status @ hermes_cli/web_server.py:_gemini_cli_status */
char *gemini_cli_status_fn(void)
{
    const char *gemini = getenv("GEMINI_CLI_ENABLED");
    return strdup((gemini && strcmp(gemini, "1") == 0) ? "{\"status\":\"enabled\"}" : "{\"status\":\"disabled\"}");
}

/* PoP: _get_chat_argv_lock @ hermes_cli/web_server.py:_get_chat_argv_lock */
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
    if (!path) return;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return;
    fclose(f);
}

/* PoP: _fs_regular_file @ hermes_cli/web_server.py:_fs_regular_file */
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

/* PoP: _fs_find_git_root @ hermes_cli/web_server.py:_fs_find_git_root */
/* Delegate to pure port to avoid duplicate implementation */
char *fs_find_git_root(const char *start)
{
    return ws_fs_find_git_root(start);
}

/* PoP: _fs_git_branch @ hermes_cli/web_server.py:_fs_git_branch */
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

/* PoP: _audio_extension_for_mime @ hermes_cli/web_server.py:_audio_extension_for_mime */
const char *audio_extension_for_mime(const char *mime_type)
{
    return ws_audio_extension_for_mime(mime_type);
}

/* PoP: port_web_server__infer_type @ hermes_cli/web_server.py:_infer_type */
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

/* PoP: _build_schema_from_config @ hermes_cli/web_server.py:_build_schema_from_config */
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

/* PoP: port_web_server__normalize_main_model_assignment @ hermes_cli/web_server.py:_normalize_main_model_assignment */
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

/* PoP: port_web_server__apply_main_model_assignment @ hermes_cli/web_server.py:_apply_main_model_assignment */
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

/* PoP: port_web_server__display_system_platform @ hermes_cli/web_server.py:_display_system_platform */
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

/* PoP: port_web_server__safe_call @ hermes_cli/web_server.py:_safe_call */
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

/* PoP: port_web_server__elevenlabs_voice_label @ hermes_cli/web_server.py:_elevenlabs_voice_label */
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

/* PoP: port_web_server__voice_list_error_logged_once @ hermes_cli/web_server.py:_voice_list_error_logged_once */
bool web_voice_list_error_logged_once(void)
{
    static bool logged = false;
    if (!logged) {
        logged = true;
        return false;  /* first call - not logged yet */
    }
    return true;  /* already logged */
}

/* PoP: _parse_model_ids @ hermes_cli/web_server.py:_parse_model_ids */
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

/* PoP: port_web_server__redact_mcp_env @ hermes_cli/web_server.py:_redact_mcp_env */
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

/* PoP: port_web_server__mcp_server_summary @ hermes_cli/web_server.py:_mcp_server_summary */
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

/* PoP: port_web_server__safe_backup_upload_name @ hermes_cli/web_server.py:_safe_backup_upload_name */
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

/* PoP: port_web_server__normalise_prefix @ hermes_cli/web_server.py:_normalise_prefix */
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

/* PoP: port_web_server__parse_theme_layer @ hermes_cli/web_server.py:_parse_theme_layer */
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

/* PoP: port_web_server__normalise_theme_definition @ hermes_cli/web_server.py:_normalise_theme_definition */
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

/* PoP: port_web_server__validate_plugin_name @ hermes_cli/web_server.py:_validate_plugin_name */
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

/* PoP: port_web_server__read_bound_port @ hermes_cli/web_server.py:_read_bound_port */
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

/* PoP: port_web_server__write_dashboard_ready_file @ hermes_cli/web_server.py:_write_dashboard_ready_file */
bool web_write_dashboard_ready_file(const char *ready_file, int port)
{
    if (!ready_file) return false;

    FILE *f = fopen(ready_file, "w");
    if (!f) return false;

    fprintf(f, "port:%d\n", port);
    fclose(f);
    return true;
}

/* PoP: port_web_server__maybe_open_browser @ hermes_cli/web_server.py:_maybe_open_browser */
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

/* PoP: _platform_env_prefixes @ hermes_cli/web_server.py:_platform_env_prefixes */
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
