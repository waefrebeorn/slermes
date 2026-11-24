/*
 * config_runtime_keys.c - C home for upstream Hermes hermes_cli/config_runtime_keys.py.
 *
 * PoP: w18_configrunt_356cce_covered @ hermes_cli/config_runtime_keys.py:_covered
 * PoP: w18_configrunt_356cce_is_runtime_read_key @ hermes_cli/config_runtime_keys.py:is_runtime_read_key
 *
 * _covered is the exact `key == path or key.startswith(path + ".")` test.
 * is_runtime_read_key reproduces the dotted-join composition; the three
 * late-imported collaborators (is_preset_field_path, selection_config_keys,
 * plugin_declared_config_keys) are injected as fixture data seams.
 * CORE_RUNTIME_KEYS is a fixed module dict of two keys, iterated as a dict
 * (i.e. by key) by the Python `_covered(dotted, CORE_RUNTIME_KEYS)` call.
 */
#include "noc_w18_configrunt_356cce.h"

#include "hermes_json.h"

#include <string.h>

static int crk_covered(const char *key, const struct json_t *paths) {
    if (!key) key = "";
    if (!paths || paths->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(paths); ++i) {
        const json_t *p = json_get(paths, i);
        if (!p || p->type != JSON_STRING) continue;
        const char *s = p->str_val ? p->str_val : "";
        if (!strcmp(key, s)) return 1;
        size_t n = strlen(s);
        if (!strncmp(key, s, n) && key[n] == '.') return 1;
    }
    return 0;
}

int w18_configrunt_356cce_covered(const char *key, const struct json_t *paths) {
    return crk_covered(key, paths);
}

/* The keys of CORE_RUNTIME_KEYS, in source order. */
static const char *const CRK_CORE_KEYS[] = {
    "computer_use.backend",
    "dashboard.kanban",
};
static const size_t CRK_CORE_N = sizeof(CRK_CORE_KEYS) / sizeof(CRK_CORE_KEYS[0]);

static json_t *crk_core_paths(void) {
    json_t *a = json_array();
    for (size_t i = 0; i < CRK_CORE_N; ++i) json_append(a, json_string(CRK_CORE_KEYS[i]));
    return a;
}

static int crk_in_strings(const char *needle, const struct json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); ++i) {
        const json_t *s = json_get(arr, i);
        if (s && s->type == JSON_STRING && s->str_val && !strcmp(s->str_val, needle)) return 1;
    }
    return 0;
}

int w18_configrunt_356cce_is_runtime_read_key(const struct json_t *parts, int preset,
                                              const struct json_t *selection,
                                              const struct json_t *plugin) {
    if (preset) return 1;

    /* dotted = ".".join(parts) */
    char dotted[8192];
    dotted[0] = '\0';
    size_t used = 0;
    if (parts && parts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(parts) && used + 1 < sizeof(dotted); ++i) {
            const json_t *p = json_get(parts, i);
            const char *s = (p && p->type == JSON_STRING && p->str_val) ? p->str_val : "";
            if (i) {
                dotted[used++] = '.';
                if (used + 1 >= sizeof(dotted)) break;
            }
            size_t n = strlen(s);
            size_t room = sizeof(dotted) - 1 - used;
            if (n > room) n = room;
            memcpy(dotted + used, s, n);
            used += n;
            dotted[used] = '\0';
        }
    }

    if (crk_in_strings(dotted, selection)) return 1;
    json_t *core = crk_core_paths();
    int hit = crk_covered(dotted, core);
    json_free(core);
    if (hit) return 1;
    return crk_covered(dotted, plugin);
}
