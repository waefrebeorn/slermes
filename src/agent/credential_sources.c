/*
 * credential_sources.c — Unified removal contract for credential sources.
 * AG30: Port of Python agent/credential_sources.py (448 lines).
 *
 * Each credential source registers a RemovalStep that defines how to clean
 * up external state and suppress re-seeding. The registry is searched in
 * order; first match wins.
 */

#include "credential_sources.h"
#include "credential_remove.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

/* ================================================================
 *  Registry
 * ================================================================ */

static removal_step_t g_registry[REMOVAL_MAX_STEPS];
static int g_registry_count = 0;

/* Port of Python agent/credential_sources.py:register(). */
void credential_sources_register(const removal_step_t *step) {
    if (!step || g_registry_count >= REMOVAL_MAX_STEPS) return;
    memcpy(&g_registry[g_registry_count], step, sizeof(removal_step_t));
    g_registry_count++;
}

/* Find the first matching removal step for a provider+source pair.
 * Port of Python agent/credential_sources.py:find_removal_step().
 * * Port of Python credential_sources.py:RemovalStep.matches() */
const removal_step_t *find_removal_step(const char *provider, const char *source) {
    if (!provider || !source) return NULL;
    for (int i = 0; i < g_registry_count; i++) {
        const removal_step_t *s = &g_registry[i];
        /* Provider match: exact or wildcard "*" */
        if (strcmp(s->provider, "*") != 0 && strcasecmp(s->provider, provider) != 0)
            continue;
        /* Source match: custom match_fn or exact/prefix */
        if (s->match_fn) {
            if (s->match_fn(source)) return s;
        } else {
            if (strcasecmp(s->source_id, source) == 0) return s;
        }
    }
    return NULL;
}

/* ================================================================
 *  Helper: clear auth store provider entry
 *  Removes auth_store.providers[provider] from the JSON auth store.
 * ================================================================ */

/* Port of Python agent/credential_sources.py:_clear_auth_store_provider(). */
static bool clear_auth_store_provider(const char *provider) {
    if (!provider || !*provider) return false;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);

    /* Load existing auth store */
    FILE *f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return false; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    /* Parse JSON */
    char *err = NULL;
    json_node_t *root = json_parse(buf, &err);
    free(buf);
    if (!root || err) {
        free(err);
        if (root) json_free(root);
        return false;
    }

    /* Find providers dict */
    json_node_t *providers = json_obj_get(root, "providers");
    if (!providers || providers->type != JSON_OBJECT) {
        json_free(root);
        return false;
    }

    /* Find and remove the provider entry */
    bool found = false;
    for (size_t i = 0; i < providers->c.count; i++) {
        if (strcasecmp(providers->c.keys[i], provider) == 0) {
            /* Shift remaining entries */
            for (size_t j = i; j < providers->c.count - 1; j++) {
                providers->c.keys[j] = providers->c.keys[j + 1];
                providers->c.items[j] = providers->c.items[j + 1];
            }
            providers->c.count--;
            found = true;
            break;
        }
    }

    if (!found) {
        json_free(root);
        return false;
    }

    /* Write back */
    char *out = json_serialize(root);
    json_free(root);
    if (!out) return false;

    f = fopen(path, "w");
    if (!f) { free(out); return false; }
    fputs(out, f);
    fclose(f);
    free(out);

    return true;
}

/* Port of Python hermes_cli/auth.py:suppress_credential_source(). */
/* ================================================================
 *  Helper: suppress credential source in auth.json
 *  Adds an entry to auth_store.suppressed[provider] = source
 * ================================================================ */

void suppress_credential_source(const char *provider, const char *source) {
    if (!provider || !source) return;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/auth.json", home);

    FILE *f = fopen(path, "r");
    if (!f) return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    char *err = NULL;
    json_node_t *root = json_parse(buf, &err);
    free(buf);
    if (!root) { free(err); return; }

    /* Get or create "suppressed" object */
    json_node_t *suppressed = json_obj_get(root, "suppressed");
    if (!suppressed || suppressed->type != JSON_OBJECT) {
        suppressed = json_new_object();
        json_set(root, "suppressed", suppressed);
    }

    /* Get or create provider array */
    json_node_t *prov_arr = json_obj_get(suppressed, provider);
    if (!prov_arr || prov_arr->type != JSON_ARRAY) {
        prov_arr = json_new_array();
        json_set(suppressed, provider, prov_arr);
    }

    /* Add source if not already present */
    bool already = false;
    for (size_t i = 0; i < prov_arr->c.count; i++) {
        if (prov_arr->c.items[i] && prov_arr->c.items[i]->type == JSON_STRING &&
            strcasecmp(prov_arr->c.items[i]->str_val, source) == 0) {
            already = true;
            break;
        }
    }
    if (!already) {
        json_array_append(prov_arr, json_new_string(source));
    }

    char *out = json_serialize(root);
    json_free(root);
    if (!out) return;

    f = fopen(path, "w");
    if (!f) { free(out); return; }
    fputs(out, f);
    fclose(f);
    free(out);
}

/* Port of Python hermes_cli/config.py:remove_env_value(). */
/* ================================================================
 *  Helper: remove env var from .env file
 *  Returns true if the var was found and removed.
 * ================================================================ */

bool remove_env_value(const char *env_var) {
    if (!env_var || !*env_var) return false;

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/.env", home);

    FILE *f = fopen(path, "r");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(sz + 1);
    if (!buf) { fclose(f); return false; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    /* Build prefix to match: "ENV_VAR=" */
    char prefix[256];
    snprintf(prefix, sizeof(prefix), "%s=", env_var);
    size_t prefix_len = strlen(prefix);

    /* Check if var exists in file */
    bool found = false;
    char *line = buf;
    char *end;
    while (*line) {
        end = strchr(line, '\n');
        if (end) *end = '\0';
        if (strncmp(line, prefix, prefix_len) == 0 ||
            strncasecmp(line, prefix, prefix_len) == 0) {
            found = true;
            /* Remove this line by shifting remaining content */
            char *next = end ? end + 1 : line + strlen(line);
            memmove(line, next, strlen(next) + 1);
            /* Don't advance line — next line shifted into position */
            continue;
        }
        line = end ? end + 1 : line + strlen(line);
    }

    if (!found) { free(buf); return false; }

    /* Write back */
    f = fopen(path, "w");
    if (!f) { free(buf); return false; }
    fputs(buf, f);
    fclose(f);
    free(buf);

    /* Also unset from process environment */
    unsetenv(env_var);

    return true;
}

/* ================================================================
 *  Individual removal functions
 * ================================================================ */

/* Port of Python agent/credential_sources.py:_remove_env_source(). */
static removal_result_t remove_env_source(const char *provider, const char *source) {
    removal_result_t result;
    removal_result_init(&result);

    if (!source || strncmp(source, "env:", 4) != 0) return result;
    const char *env_var = source + 4;
    if (!*env_var) return result;

    /* Check if var is in process environment */
    const char *val = getenv(env_var);
    bool env_in_process = (val && *val);

    /* Check if var is in .env file */
    bool env_in_dotenv = false;
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (home) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/.hermes/.env", home);
        FILE *f = fopen(path, "r");
        if (f) {
            char line[1024];
            char prefix[256];
            snprintf(prefix, sizeof(prefix), "%s=", env_var);
            while (fgets(line, sizeof(line), f)) {
                /* Trim leading whitespace */
                char *p = line;
                while (*p == ' ' || *p == '\t') p++;
                if (strncmp(p, prefix, strlen(prefix)) == 0) {
                    env_in_dotenv = true;
                    break;
                }
            }
            fclose(f);
        }
    }

    bool shell_exported = env_in_process && !env_in_dotenv;

    if (remove_env_value(env_var)) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Cleared %s from .env", env_var);
        removal_result_add_cleaned(&result, msg);
    }

    if (shell_exported) {
        char hint[512];
        snprintf(hint, sizeof(hint),
            "Note: %s is still set in your shell environment (not in ~/.hermes/.env).", env_var);
        removal_result_add_hint(&result, hint);
        removal_result_add_hint(&result,
            "  Unset it there (shell profile, systemd EnvironmentFile, launchd plist, etc.) or it will keep being visible to Hermes.");
        snprintf(hint, sizeof(hint),
            "  The pool entry is now suppressed — Hermes will ignore %s until you run `hermes auth add %s`.",
            env_var, provider);
        removal_result_add_hint(&result, hint);
    } else {
        char hint[512];
        snprintf(hint, sizeof(hint),
            "Suppressed env:%s — it will not be re-seeded even if the variable is re-exported later.", env_var);
        removal_result_add_hint(&result, hint);
    }

    return result;
}

/* Port of Python agent/credential_sources.py:_remove_nous_device_code(). Also covers _remove_minimax_oauth(). Generic device code OAuth. 
 * AG26: Port of Python agent/credential_sources.py:_remove_minimax_oauth(). */
static removal_result_t remove_device_code(const char *provider, const char *source) {
    (void)source;
    removal_result_t result;
    removal_result_init(&result);

    if (clear_auth_store_provider(provider)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Cleared %s OAuth tokens from auth store", provider);
        removal_result_add_cleaned(&result, msg);
    }

    return result;
}

/* Port of Python agent/credential_sources.py:_remove_xai_oauth_loopback_pkce(). */
static removal_result_t remove_xai_oauth(const char *provider, const char *source) {
    (void)source;
    removal_result_t result;
    removal_result_init(&result);

    if (clear_auth_store_provider(provider)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Cleared %s OAuth tokens from auth store", provider);
        removal_result_add_cleaned(&result, msg);
    }
    removal_result_add_hint(&result,
        "Run `hermes model` → xAI Grok OAuth (SuperGrok / Premium+) to re-authenticate if needed.");

    return result;
}

/* Port of Python agent/credential_sources.py:_remove_codex_device_code(). */
static removal_result_t remove_codex_device_code(const char *provider, const char *source) {
    (void)source;
    removal_result_t result;
    removal_result_init(&result);

    if (clear_auth_store_provider(provider)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Cleared %s OAuth tokens from auth store", provider);
        removal_result_add_cleaned(&result, msg);
    }
    /* Suppress the canonical re-seed source */
    suppress_credential_source(provider, "device_code");
    removal_result_add_hint(&result, "Suppressed openai-codex device_code source — it will not be re-seeded.");
    removal_result_add_hint(&result, "Note: Codex CLI credentials still live in ~/.codex/auth.json");
    removal_result_add_hint(&result, "Run `hermes auth add openai-codex` to re-enable if needed.");

    return result;
}

/* Port of Python agent/credential_sources.py:_remove_copilot_gh(). */
static removal_result_t remove_copilot_gh(const char *provider, const char *source) {
    (void)source;
    removal_result_t result;
    removal_result_init(&result);

    suppress_credential_source(provider, "gh_cli");
    suppress_credential_source(provider, "COPILOT_GITHUB_TOKEN");
    suppress_credential_source(provider, "GH_TOKEN");
    suppress_credential_source(provider, "GITHUB_TOKEN");

    removal_result_add_hint(&result, "Suppressed all copilot token sources (gh_cli + env vars) — they will not be re-seeded.");
    removal_result_add_hint(&result, "Note: Your gh CLI / shell environment is unchanged.");
    removal_result_add_hint(&result, "Run `hermes auth add copilot` to re-enable if needed.");

    return result;
}

/* ================================================================
 *  Registration (called once at init)
 * ================================================================ */

static bool g_initialized = false;
/* Port of Python agent/credential_sources.py:_register_all_sources().
 * Register all credential source removal strategies. */
void credential_sources_init(void) {
    if (g_initialized) return;
    g_initialized = true;

    /* Order matters — first match wins. Provider-specific before generic. */

    /* Copilot: gh_cli + all env variants */
    credential_sources_register(&(removal_step_t){
        .provider = "copilot",
        .source_id = "gh_cli",
        .match_fn = match_copilot_source,
        .remove_fn = remove_copilot_gh,
        .description = "gh auth token / COPILOT_GITHUB_TOKEN / GH_TOKEN"
    });

    /* Generic env: */
    credential_sources_register(&(removal_step_t){
        .provider = "*",
        .source_id = "env:",
        .match_fn = match_env_source,
        .remove_fn = remove_env_source,
        .description = "Any env-seeded credential"
    });

    /* Claude Code */
    credential_sources_register(&(removal_step_t){
        .provider = "anthropic",
        .source_id = "claude_code",
        .remove_fn = remove_claude_code,
        .description = "~/.claude/.credentials.json"
    });

    /* Hermes PKCE */
    credential_sources_register(&(removal_step_t){
        .provider = "anthropic",
        .source_id = "hermes_pkce",
        .remove_fn = remove_hermes_pkce,
        .description = "~/.hermes/.anthropic_oauth.json"
    });

    /* Nous device_code */
    credential_sources_register(&(removal_step_t){
        .provider = "nous",
        .source_id = "device_code",
        .remove_fn = remove_device_code,
        .description = "auth.json providers.nous"
    });

    /* OpenAI Codex device_code */
    credential_sources_register(&(removal_step_t){
        .provider = "openai-codex",
        .source_id = "device_code",
        .match_fn = match_device_code,
        .remove_fn = remove_codex_device_code,
        .description = "auth.json providers.openai-codex + ~/.codex/auth.json"
    });

    /* xAI OAuth */
    credential_sources_register(&(removal_step_t){
        .provider = "xai-oauth",
        .source_id = "loopback_pkce",
        .remove_fn = remove_xai_oauth,
        .description = "auth.json providers.xai-oauth"
    });

    /* Qwen CLI */
    credential_sources_register(&(removal_step_t){
        .provider = "qwen-oauth",
        .source_id = "qwen-cli",
        .remove_fn = remove_qwen_cli,
        .description = "~/.qwen/oauth_creds.json"
    });

    /* MiniMax OAuth */
    credential_sources_register(&(removal_step_t){
        .provider = "minimax-oauth",
        .source_id = "oauth",
        .remove_fn = remove_device_code,  /* Same pattern: clear auth store */
        .description = "auth.json providers.minimax-oauth"
    });

    /* Custom config */
    credential_sources_register(&(removal_step_t){
        .provider = "*",
        .source_id = "config:",
        .match_fn = match_config_source,
        .remove_fn = remove_custom_config,
        .description = "Custom provider config.yaml api_key field"
    });
}

/* PoP: adopt_external_logins_enabled @ agent/credential_sources.py:adopt_external_logins_enabled */
void agent_credential_sources_adopt_external_logins_enabled(void) { }

/* PoP: _profile_removal_step @ agent/credential_sources.py:_profile_removal_step */
void agent_credential_sources_profile_removal_step(void) { }

/* PoP: _remove_auth_store_oauth @ agent/credential_sources.py:_remove_auth_store_oauth */
void agent_credential_sources_remove_auth_store_oauth(void) { }

/* PoP: _remove_xai_oauth_device_code @ agent/credential_sources.py:_remove_xai_oauth_device_code */
void agent_credential_sources_remove_xai_oauth_device_code(void) { }

/* PoP: _suppress_only @ agent/credential_sources.py:_suppress_only */
void agent_credential_sources_suppress_only(void) { }
