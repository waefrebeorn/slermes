/**
 * port_agent_secret_scope.c — Port of Python agent/secret_scope.py
 *
 * Real C implementations for secret scope management.
 * Implements profile-scoped credential resolution for multi-profile gateway multiplexing.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#include "hermes_logger.h"
#include "hermes_json.h"

/* ── Thread-local storage for secret scope ───────────────────────────── */

static pthread_key_t secret_scope_key;
static bool secret_scope_key_initialized = false;
static pthread_once_t secret_scope_key_once = PTHREAD_ONCE_INIT;

static void secret_scope_key_destructor(void *value) {
    if (value) {
        json_free((json_t *)value);
    }
}

static void secret_scope_key_init(void) {
    pthread_key_create(&secret_scope_key, secret_scope_key_destructor);
    secret_scope_key_initialized = true;
}

/* PoP: secret_scope_get_current @ agent/secret_scope.py:current_secret_scope */
static json_t *secret_scope_get_current(void) {
    pthread_once(&secret_scope_key_once, secret_scope_key_init);
    return (json_t *)pthread_getspecific(secret_scope_key);
}

/* PoP: secret_scope_set_current @ agent/secret_scope.py:set_secret_scope */
static void secret_scope_set_current(json_t *scope) {
    pthread_once(&secret_scope_key_once, secret_scope_key_init);
    json_t *old = (json_t *)pthread_getspecific(secret_scope_key);
    if (old) json_free(old);
    pthread_setspecific(secret_scope_key, scope);
}

/* ── Multiplex active flag (process-global) ──────────────────────────── */

static bool multiplex_active = false;

/* PoP: secret_scope_set_multiplex_active @ agent/secret_scope.py:set_multiplex_active */
bool secret_scope_set_multiplex_active(bool active)
{
    multiplex_active = active;
    hermes_log(LOG_DEBUG, "secret_scope", "multiplex_active set to %d", active);
    return true;
}

/* PoP: secret_scope_is_multiplex_active @ agent/secret_scope.py:is_multiplex_active */
bool secret_scope_is_multiplex_active(void)
{
    return multiplex_active;
}

/* ── Global env var allowlist (port of _GLOBAL_ENV_EXACT/_GLOBAL_ENV_PREFIXES) ──────── */

static const char *global_env_exact[] = {
    "HERMES_HOME", "HERMES_PROFILE", "HERMES_GATEWAY_LOCK_DIR",
    "HERMES_MAX_ITERATIONS", "HERMES_MAX_TOKENS", "HERMES_API_TIMEOUT",
    "HERMES_REDACT_SECRETS", "HERMES_NOUS_TIMEOUT_SECONDS",
    "_HERMES_GATEWAY",
    "PATH", "HOME", "USER", "LANG", "LC_ALL", "TZ", "PWD", "SHELL", "TMPDIR",
    "VIRTUAL_ENV", "PYTHONPATH", "SSL_CERT_FILE",
    "HERMES_KANBAN_DB", "HERMES_KANBAN_WORKSPACES_ROOT", "HERMES_KANBAN_BOARD",
    NULL
};

static const char *global_env_prefixes[] = {
    "HERMES_KANBAN_",
    "HERMES_TELEGRAM_",
    "TERMINAL_",
    NULL
};

/* PoP: secret_scope_is_global_env @ agent/secret_scope.py:_is_global_env */
static bool secret_scope_is_global_env(const char *name)
{
    if (!name) return false;

    for (int i = 0; global_env_exact[i]; i++) {
        if (strcmp(name, global_env_exact[i]) == 0) return true;
    }

    for (int i = 0; global_env_prefixes[i]; i++) {
        if (strncmp(name, global_env_prefixes[i], strlen(global_env_prefixes[i])) == 0) return true;
    }

    return false;
}

/* ── Secret scope token for reset (opaque pointer to previous scope) ──── */

typedef json_t *secret_scope_token_t;

/* PoP: secret_scope_set_secret_scope @ agent/secret_scope.py:set_secret_scope */
secret_scope_token_t secret_scope_set_secret_scope(const char *secrets_json)
{
    json_t *scope = NULL;
    if (secrets_json && *secrets_json) {
        char *err_msg = NULL;
        scope = json_parse(secrets_json, &err_msg);
        if (!scope) {
            hermes_log(LOG_WARNING, "secret_scope", "Failed to parse secrets JSON: %s", err_msg ? err_msg : "unknown");
            free(err_msg);
            scope = json_object();
        }
    } else {
        scope = json_object();
    }

    json_t *old = secret_scope_get_current();
    secret_scope_set_current(scope);

    hermes_log(LOG_DEBUG, "secret_scope", "set_secret_scope: installed new scope (%p), old=%p", (void*)scope, (void*)old);
    return old;
}

/* PoP: secret_scope_reset_secret_scope @ agent/secret_scope.py:reset_secret_scope */
void secret_scope_reset_secret_scope(secret_scope_token_t token)
{
    json_t *current = secret_scope_get_current();
    if (current) json_free(current);
    secret_scope_set_current(token);
    hermes_log(LOG_DEBUG, "secret_scope", "reset_secret_scope: restored previous scope (%p)", (void*)token);
}

/* PoP: secret_scope_current_secret_scope @ agent/secret_scope.py:current_secret_scope */
json_t *secret_scope_current_secret_scope(void)
{
    return secret_scope_get_current();
}

/* PoP: secret_scope_get_secret @ agent/secret_scope.py:get_secret */
const char *secret_scope_get_secret(const char *name, const char *default_val)
{
    if (!name) return default_val;

    /* 1. Global env vars always read from os.environ */
    if (secret_scope_is_global_env(name)) {
        const char *val = getenv(name);
        return val ? val : default_val;
    }

    /* 2. Check thread-local secret scope */
    json_t *scope = secret_scope_get_current();
    if (scope && scope->type == JSON_OBJECT) {
        json_t *val_node = json_object_get(scope, name);
        if (val_node && val_node->type == JSON_STRING) {
            return json_node_get_string(val_node);
        }
    }

    /* 3. No scope installed */
    if (multiplex_active) {
        hermes_log(LOG_ERROR, "secret_scope", "get_secret(%s) called with no profile secret scope active while multiplexing is on", name);
        return default_val; /* In Python this raises UnscopedSecretError; in C we return default */
    }

    /* 4. Multiplex inactive: fall back to os.environ */
    const char *val = getenv(name);
    return val ? val : default_val;
}

/* ── .env file parsing ────────────────────────────────────────────────── */

static void secret_scope_parse_env_line(const char *line, char *key_out, size_t key_sz, char *val_out, size_t val_sz)
{
    char *eq = strchr(line, '=');
    if (!eq) return;

    size_t key_len = eq - line;
    if (key_len >= key_sz) key_len = key_sz - 1;
    memcpy(key_out, line, key_len);
    key_out[key_len] = '\0';

    const char *val = eq + 1;
    /* Strip matching quotes */
    if (strlen(val) >= 2 && val[0] == val[strlen(val)-1] && (val[0] == '"' || val[0] == '\'')) {
        val++;
        size_t val_len = strlen(val) - 1;
        if (val_len >= val_sz) val_len = val_sz - 1;
        memcpy(val_out, val, val_len);
        val_out[val_len] = '\0';
    } else {
        size_t val_len = strlen(val);
        if (val_len >= val_sz) val_len = val_sz - 1;
        memcpy(val_out, val, val_len);
        val_out[val_len] = '\0';
    }
}

/* PoP: secret_scope_load_env_file @ agent/secret_scope.py:load_env_file */
json_t *secret_scope_load_env_file(const char *path)
{
    json_t *secrets = json_object();
    if (!secrets) return NULL;

    FILE *f = fopen(path, "r");
    if (!f) return secrets;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        /* Strip newline */
        line[strcspn(line, "\r\n")] = '\0';
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        if (!*trimmed || *trimmed == '#') continue;

        if (strncmp(trimmed, "export ", 7) == 0) {
            trimmed += 7;
            while (*trimmed && isspace(*trimmed)) trimmed++;
        }

        if (strchr(trimmed, '=')) {
            char key[512], val[1024];
            secret_scope_parse_env_line(trimmed, key, sizeof(key), val, sizeof(val));
            if (*key) {
                json_object_set(secrets, key, json_new_string(val));
            }
        }
    }
    fclose(f);
    return secrets;
}

/* PoP: secret_scope_build_profile_secret_scope @ agent/secret_scope.py:build_profile_secret_scope */
json_t *secret_scope_build_profile_secret_scope(const char *hermes_home)
{
    if (!hermes_home) return json_object();

    char env_path[2048];
    snprintf(env_path, sizeof(env_path), "%s/.env", hermes_home);
    return secret_scope_load_env_file(env_path);
}

/* ── Helper: is global env (exposed for C callers) ───────────────────── */

/* PoP: secret_scope_is_global_env_fn @ agent/secret_scope.py:_is_global_env */
bool secret_scope_is_global_env_fn(const char *name)
{
    return secret_scope_is_global_env(name);
}