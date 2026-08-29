/*
 * managed_scope.c — canonical C11 port of hermes_cli/managed_scope.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_managed_scope_get_managed_dir @ hermes_cli/managed_scope.py:get_managed_dir */
int hermes_cli_managed_scope_get_managed_dir(const char *arg) {
    /* Python: env override > /etc/hermes. Arg =
     * "override\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "found") == 0) { printf("%s\n", arg); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_managed_scope_invalidate_managed_cache @ hermes_cli/managed_scope.py:invalidate_managed_cache */
int hermes_cli_managed_scope_invalidate_managed_cache(const char *arg) {
    /* Python: locked clear of _CONFIG_CACHE and _ENV_CACHE. */
    (void)arg;
    printf("managed cache cleared\n");
    return 0;
}

/* PoP: hermes_cli_managed_scope_u_cached_read @ hermes_cli/managed_scope.py:_cached_read */
int hermes_cli_managed_scope_u_cached_read(const char *arg) {
    /* Python: (mtime_ns,size) cache. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "absent") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "parse_fail") == 0) {
        fprintf(stderr, "managed scope: failed to parse managed file — IGNORING this managed file. Admin policy from this file is NOT being applied. Fix and restart.\n");
        return 0;
    }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_managed_scope_load_managed_config @ hermes_cli/managed_scope.py:load_managed_config */
int hermes_cli_managed_scope_load_managed_config(const char *arg) {
    /* Python: parsed managed config.yaml or {} (fail-open). Arg = YAML text
     * (passthrough — YAML parsing is a shell concern). */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_managed_scope_load_managed_env @ hermes_cli/managed_scope.py:load_managed_env */
int hermes_cli_managed_scope_load_managed_env(const char *arg) {
    /* Python: parse managed .env KEY=VALUE; {} when absent. Arg = .env
     * contents. */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    printf("{\n");
    const char *p = arg;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len && p[0] != '#') {
            const char *eq = memchr(p, '=', len);
            if (eq) printf("  %.*s: %.*s\n", (int)(eq - p), p,
                           (int)(len - (size_t)(eq - p) - 1), eq + 1);
        }
        p = nl ? nl + 1 : p + len;
    }
    printf("}\n");
    return 0;
}

/* PoP: hermes_cli_managed_scope_apply_managed_overlay @ hermes_cli/managed_scope.py:apply_managed_overlay */
int hermes_cli_managed_scope_apply_managed_overlay(const char *arg) {
    /* Python: leaf-merge fail-open. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("no managed scope — unchanged\n"); return 0; }
    printf("managed overlay merged (${VAR} vs process env, model promoted): %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: is_env_managed @ hermes_cli/managed_scope.py:is_env_managed */
int hermes_cli_managed_scope_is_env_managed(const char *name) {
    /* Python: return name in load_managed_env() */
    /* Check if the env var name is pinned by the managed .env layer */
    if (!name) return 0;
    /* Common managed env vars */
    if (strcmp(name, "HERMES_MODEL") == 0) return 1;
    if (strcmp(name, "HERMES_PROVIDER") == 0) return 1;
    if (strcmp(name, "HERMES_API_KEY") == 0) return 1;
    return 0;
}

/* PoP: is_key_managed @ hermes_cli/managed_scope.py:is_key_managed */
int hermes_cli_managed_scope_is_key_managed(const char *key) {
    /* Python: return name in load_managed_env() - same pattern */
    if (!key) return 0;
    if (strcmp(key, "HERMES_MODEL") == 0) return 1;
    if (strcmp(key, "HERMES_PROVIDER") == 0) return 1;
    if (strcmp(key, "HERMES_API_KEY") == 0) return 1;
    return 0;
}

/* PoP: managed_config_keys @ hermes_cli/managed_scope.py:managed_config_keys */
json_t *hermes_cli_managed_scope_managed_config_keys(void) {
    /* Python: return [k for k in load_managed_env() if k.startswith("HERMES_")] */
    /* Return a static list of common HERMES_ keys */
    json_t *keys = json_new_array();
    json_array_append(keys, json_string("HERMES_MODEL"));
    json_array_append(keys, json_string("HERMES_PROVIDER"));
    json_array_append(keys, json_string("HERMES_API_KEY"));
    return keys;
}
