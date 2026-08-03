/*
 * credential_sources.c — Unified removal contract for credential sources.
 * AG30: Port of Python agent/credential_sources.py (448 lines).
 *
 * Each credential source registers a RemovalStep that defines how to clean
 * up external state and suppress re-seeding. The registry is searched in
 * order; first match wins.
 */

#include "credential_sources.h"
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
 *  RemovalResult / RemovalStep types
 * ================================================================ */

void removal_result_init(removal_result_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->suppress = true;
}

void removal_result_add_cleaned(removal_result_t *r, const char *msg) {
    if (!r || !msg) return;
    if (r->cleaned_count < REMOVAL_RESULT_MAX_LINES) {
        r->cleaned[r->cleaned_count++] = strdup(msg);
    }
}

void removal_result_add_hint(removal_result_t *r, const char *msg) {
    if (!r || !msg) return;
    if (r->hints_count < REMOVAL_RESULT_MAX_LINES) {
        r->hints[r->hints_count++] = strdup(msg);
    }
}

void removal_result_free(removal_result_t *r) {
    if (!r) return;
    for (int i = 0; i < r->cleaned_count; i++) free(r->cleaned[i]);
    for (int i = 0; i < r->hints_count; i++) free(r->hints[i]);
    memset(r, 0, sizeof(*r));
}

/* ================================================================
 *  Registry — hive-backed (no landlocked static array)
 * ================================================================ */

#include "hive.h"
static hive_t *g_registry = NULL;

/* Port of Python agent/credential_sources.py:register(). */
void credential_sources_register(const removal_step_t *step) {
    if (!step) return;
    if (!g_registry) g_registry = hive_new(8);
    removal_step_t *copy = malloc(sizeof(removal_step_t));
    if (!copy) return;
    memcpy(copy, step, sizeof(removal_step_t));
    bool ok = false;
    hive_insert(g_registry, copy, &ok);
    if (!ok) free(copy);
}

/* Find the first matching removal step for a provider+source pair.
 * Port of Python agent/credential_sources.py:find_removal_step().
 * * Port of Python credential_sources.py:RemovalStep.matches() */
const removal_step_t *find_removal_step(const char *provider, const char *source) {
    if (!provider || !source || !g_registry) return NULL;
    hive_iter_t it;
    hive_iter_begin(g_registry, &it);
    removal_step_t *s;
    while (hive_iter_next(g_registry, &it, NULL, (void **)&s)) {
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

    /* Find and remove the provider entry (case-insensitive match) */
    bool found = false;
    for (size_t i = 0; i < providers->c.count; i++) {
        if (strcasecmp(providers->c.keys[i], provider) == 0) {
            found = json_obj_del(providers, providers->c.keys[i]);
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

/* Port of Python agent/credential_sources.py:_remove_claude_code(). */
static removal_result_t remove_claude_code(const char *provider, const char *source) {
    (void)provider; (void)source;
    removal_result_t result;
    removal_result_init(&result);
    removal_result_add_hint(&result, "Suppressed claude_code credential — it will not be re-seeded.");
    removal_result_add_hint(&result, "Note: Claude Code credentials still live in ~/.claude/.credentials.json");
    removal_result_add_hint(&result, "Run `hermes auth add anthropic` to re-enable if needed.");
    return result;
}

/* Port of Python agent/credential_sources.py:_remove_hermes_pkce(). */
static removal_result_t remove_hermes_pkce(const char *provider, const char *source) {
    (void)provider; (void)source;
    removal_result_t result;
    removal_result_init(&result);

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return result;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/.anthropic_oauth.json", home);

    if (access(path, F_OK) == 0) {
        if (unlink(path) == 0) {
            removal_result_add_cleaned(&result, "Cleared Hermes Anthropic OAuth credentials");
        } else {
            char hint[512];
            snprintf(hint, sizeof(hint), "Could not delete %s", path);
            removal_result_add_hint(&result, hint);
        }
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

/* Port of Python agent/credential_sources.py:_remove_qwen_cli(). */
static removal_result_t remove_qwen_cli(const char *provider, const char *source) {
    (void)provider; (void)source;
    removal_result_t result;
    removal_result_init(&result);
    removal_result_add_hint(&result, "Suppressed qwen-cli credential — it will not be re-seeded.");
    removal_result_add_hint(&result, "Note: Qwen CLI credentials still live in ~/.qwen/oauth_creds.json");
    removal_result_add_hint(&result, "Run `hermes auth add qwen-oauth` to re-enable if needed.");
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

/* Port of Python agent/credential_sources.py:_remove_custom_config(). */
static removal_result_t remove_custom_config(const char *provider, const char *source) {
    (void)provider;
    removal_result_t result;
    removal_result_init(&result);

    char hint[512];
    snprintf(hint, sizeof(hint), "Suppressed %s — it will not be re-seeded.", source);
    removal_result_add_hint(&result, hint);
    removal_result_add_hint(&result,
        "Note: The underlying value in config.yaml is unchanged.  Edit it directly if you want to remove the credential from disk.");

    return result;
}

/* ================================================================
 *  Match functions
 * ================================================================ */

static bool match_env_source(const char *source) {
    return (source && strncmp(source, "env:", 4) == 0);
}

static bool match_copilot_source(const char *source) {
    if (!source) return false;
    return (strcmp(source, "gh_cli") == 0 || strncmp(source, "env:", 4) == 0);
}

static bool match_device_code(const char *source) {
    if (!source) return false;
    return (strcmp(source, "device_code") == 0 || strstr(source, ":device_code") != NULL);
}

static bool match_config_source(const char *source) {
    if (!source) return false;
    return (strncmp(source, "config:", 7) == 0 || strcmp(source, "model_config") == 0);
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
