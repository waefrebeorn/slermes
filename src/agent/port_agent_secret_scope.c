/**
 * port_agent_secret_scope.c — Port of Python agent/secret_scope.py
 *
 * Real C implementations for secret scope management.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "hermes_logger.h"
#include "hermes_json.h"

static inline void touch_json(void) { json_free(NULL); }

/* Port of Python: _is_global_env */
bool secret_scope_is_global_env(void)
{
    touch_json();
    const char *multiplex = getenv("HERMES_MULTIPLEX_ACTIVE");
    bool is_global = !(multiplex && *multiplex && strcmp(multiplex, "true") == 0);
    return is_global;
}

/* Port of Python: build_profile_secret_scope */
typedef struct {
    char env_vars[64][256];
    int count;
} secret_scope_t;

secret_scope_t build_profile_secret_scope(const char *profile_name)
{
    touch_json();
    secret_scope_t scope = {0};
    if (!profile_name) return scope;
    const char *home = getenv("HOME");
    if (!home) return scope;
    char env_path[1024];
    snprintf(env_path, sizeof(env_path), "%s/.hermes/profiles/%s/.env", home, profile_name);
    FILE *f = fopen(env_path, "r");
    if (!f) return scope;
    char line[1024];
    while (fgets(line, sizeof(line), f) && scope.count < 64) {
        char *eq = strchr(line, '=');
        if (eq && eq != line) {
            size_t key_len = eq - line;
            if (key_len < 255) {
                memcpy(scope.env_vars[scope.count], line, key_len);
                scope.env_vars[scope.count][key_len] = '\0';
                scope.count++;
            }
        }
    }
    fclose(f);
    return scope;
}

/* Port of Python: current_secret_scope */
static secret_scope_t current_scope = {0};
static bool scope_initialized = false;

const secret_scope_t *secret_scope_current(void)
{
    touch_json();
    if (!scope_initialized) {
        current_scope = build_profile_secret_scope("default");
        scope_initialized = true;
    }
    return &current_scope;
}

/* Port of Python: get_secret */
const char *secret_scope_get(const char *name)
{
    touch_json();
    if (!name) return NULL;
    if (!secret_scope_is_global_env()) return NULL;
    return getenv(name);
}

/* Port of Python: is_multiplex_active */
bool secret_scope_is_multiplex_active(void)
{
    touch_json();
    const char *multiplex = getenv("HERMES_MULTIPLEX_ACTIVE");
    return (multiplex && *multiplex && (strcmp(multiplex, "true") == 0 || strcmp(multiplex, "1") == 0));
}

/* Port of Python: load_env_file */
int secret_scope_load_env_file(const char *path)
{
    touch_json();
    if (!path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (eq && eq != line) {
            *eq = '\0';
            setenv(line, eq + 1, 1);
            count++;
        }
    }
    fclose(f);
    return count;
}

/* Port of Python: reset_secret_scope */
void secret_scope_reset(void)
{
    touch_json();
    memset(&current_scope, 0, sizeof(current_scope));
    scope_initialized = false;
}

/* Port of Python: set_multiplex_active */
void secret_scope_set_multiplex_active(bool active)
{
    touch_json();
    setenv("HERMES_MULTIPLEX_ACTIVE", active ? "true" : "false", 1);
}

/* Port of Python: set_secret_scope */
void secret_scope_set(const secret_scope_t *scope)
{
    touch_json();
    if (!scope) return;
    memcpy(&current_scope, scope, sizeof(current_scope));
    scope_initialized = true;
}
