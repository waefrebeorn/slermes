/*
 * profile_memory_config.c — C home for upstream Hermes hermes_cli/profile_memory_config.py.
 * Gate w26_profilemem_515314.
 *
 * PoP: w26_profilemem_515314_active_provider @ hermes_cli/profile_memory_config.py:active_memory_provider
 * PoP: w26_profilemem_515314_names_external @ hermes_cli/profile_memory_config.py:_names_external_provider
 * PoP: w26_profilemem_515314_constants @ hermes_cli/profile_memory_config.py:_PROVIDER_NAME_RE / agent.memory_provider.CORE_MEMORY_PROVIDER_SENTINELS
 *
 * The copy itself (copytree / copy2 / chmod 0600 / os.walk) is a file-system
 * seam and stays in the caller.  Ported here is what the values decide: whether
 * a parsed config names an external provider at all, and whether that name is a
 * bare directory/file stem (a hand-edited config.yaml must not be able to aim
 * the copy outside the source profile).
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_profilemem_515314.h"

/* agent.memory_provider.CORE_MEMORY_PROVIDER_SENTINELS */
static const char *const PM_CORE_SENTINELS[] = {
    "", "built-in", "builtin", "default", "none",
};
#define PM_CORE_SENTINEL_COUNT (sizeof(PM_CORE_SENTINELS) / sizeof(PM_CORE_SENTINELS[0]))

/* _PROVIDER_NAME_RE = re.compile(r"^[A-Za-z0-9_.-]+$") */
#define PM_NAME_PATTERN "^[A-Za-z0-9_.-]+$"

static int pm_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() (ASCII whitespace) into a fresh buffer. */
static char *pm_strip_dup(const char *s) {
    const char *start = s ? s : "";
    size_t n;
    char *out;
    while (*start && pm_space((unsigned char)*start)) start++;
    n = strlen(start);
    while (n > 0 && pm_space((unsigned char)start[n - 1])) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, start, n);
    out[n] = '\0';
    return out;
}

/* is_core_memory_provider(name): str(name or "").strip().lower() in sentinels. */
static int pm_is_core(const char *name) {
    char *trimmed = pm_strip_dup(name);
    size_t i;
    int core = 0;
    if (!trimmed) return 1;
    for (char *p = trimmed; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (i = 0; i < PM_CORE_SENTINEL_COUNT; i++)
        if (strcmp(trimmed, PM_CORE_SENTINELS[i]) == 0) { core = 1; break; }
    free(trimmed);
    return core;
}

/* _PROVIDER_NAME_RE.match(name) over a bare ASCII class, at least one char. */
static int pm_name_ok(const char *name) {
    const char *p = name;
    if (!p || !*p) return 0;
    for (; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!(isalnum(c) || c == '_' || c == '.' || c == '-')) return 0;
    }
    return 1;
}

json_t *w26_profilemem_515314_active_provider(const json_t *config) {
    const json_t *memory, *name;
    char *trimmed;

    memory = config && config->type == JSON_OBJECT ? json_obj_get(config, "memory") : NULL;
    name = memory && memory->type == JSON_OBJECT ? json_obj_get(memory, "provider") : NULL;
    if (!name || name->type != JSON_STRING || !name->str_val) return json_null();
    if (pm_is_core(name->str_val)) return json_null();
    trimmed = pm_strip_dup(name->str_val);
    if (!trimmed) return json_null();
    if (strcmp(trimmed, ".") == 0 || strcmp(trimmed, "..") == 0 || !pm_name_ok(trimmed)) {
        free(trimmed);
        return json_null();
    }
    {
        json_t *out = json_string(trimmed);
        free(trimmed);
        return out;
    }
}

/* cfg_get(cfg, "memory", "provider"): dict traversal that never raises. */
static const json_t *pm_cfg_get_provider(const json_t *cfg) {
    const json_t *memory;
    if (!cfg || cfg->type != JSON_OBJECT) return NULL;
    memory = json_obj_get(cfg, "memory");
    if (!memory || memory->type != JSON_OBJECT) return NULL;
    return json_obj_get(memory, "provider");
}

json_t *w26_profilemem_515314_names_external(const json_t *configs) {
    size_t i;
    if (!configs || configs->type != JSON_ARRAY) return json_bool(false);
    for (i = 0; i < json_len(configs); i++) {
        const json_t *name = pm_cfg_get_provider(json_get(configs, i));
        char *stripped;
        if (!name || name->type != JSON_STRING || !name->str_val) continue;
        stripped = pm_strip_dup(name->str_val);
        if (!stripped) continue;
        if (stripped[0] && !pm_is_core(name->str_val)) {
            free(stripped);
            return json_bool(true);
        }
        free(stripped);
    }
    return json_bool(false);
}

json_t *w26_profilemem_515314_constants(void) {
    json_t *o = json_object();
    json_t *sentinels = json_array();
    size_t i;
    for (i = 0; i < PM_CORE_SENTINEL_COUNT; i++)
        json_append(sentinels, json_string(PM_CORE_SENTINELS[i]));
    json_set(o, "core_memory_provider_sentinels", sentinels);
    json_set(o, "provider_name_pattern", json_string(PM_NAME_PATTERN));
    return o;
}
