/*
 * plugins_discovery.c - C home for upstream Hermes hermes_cli/plugins_discovery.py.
 * PoP: w16_pluginsdis_42aa3c_is_skipped_plugin_dir @ hermes_cli/plugins_discovery.py:is_skipped_plugin_dir
 * PoP: w16_pluginsdis_42aa3c_select_entry_point_group @ hermes_cli/plugins_discovery.py:_select_entry_point_group
 * PoP: w16_pluginsdis_42aa3c_resolve_manifest_winners @ hermes_cli/plugins_discovery.py:resolve_manifest_winners
 *
 * Faithful ports of the pure directory-skip predicate, entry-point group
 * selection (dict + iterable-of-records forms), and later-source-wins key
 * resolution with the bundled-impostor guard. Filesystem scanning stays Python.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_pluginsdis_42aa3c.h"

static const char *const hermes_cli_plugins_discovery_CONST_ENTRY_POINTS_GROUP = "hermes_agent.plugins";
static const char *const hermes_cli_plugins_discovery_CONST_ENTRY_POINT_CAPABILITIES_GROUP = "hermes_agent.plugin_capabilities";
static const char *const hermes_cli_plugins_discovery_CONST_IN_BOTH_LISTS = "listed in both plugins.enabled and plugins.disabled \xe2\x80\x94 disabled wins";

static const char *const FOREIGN_HARNESS_DIRS[] = {
    ".claude-plugin", ".codex-plugin", ".cursor-plugin", ".devin-plugin", ".kimi-plugin"
};
#define NFOREIGN (sizeof(FOREIGN_HARNESS_DIRS) / sizeof(FOREIGN_HARNESS_DIRS[0]))

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

struct json_t *w16_pluginsdis_42aa3c_is_skipped_plugin_dir(const char *name) {
    if (!name) return json_bool(0);
    size_t n = strlen(name);
    int dunder = n >= 2 && name[0] == '_' && name[1] == '_' &&
                 name[n - 1] == '_' && name[n - 2] == '_';
    int foreign = 0;
    for (size_t i = 0; i < NFOREIGN; i++)
        if (!strcmp(name, FOREIGN_HARNESS_DIRS[i])) { foreign = 1; break; }
    return json_bool(dunder || foreign ? 1 : 0);
}

struct json_t *w16_pluginsdis_42aa3c_select_entry_point_group(const struct json_t *entry_points,
                                                              const char *group) {
    json_t *res = json_array();
    if (!entry_points) return res;
    if (entry_points->type == JSON_OBJECT) {
        json_t *v = json_obj_get(entry_points, group);
        if (v && v->type == JSON_ARRAY)
            for (size_t i = 0; i < v->c.count; i++) json_append(res, json_copy(v->c.items[i]));
        return res;
    }
    if (entry_points->type == JSON_ARRAY) {
        for (size_t i = 0; i < entry_points->c.count; i++) {
            json_t *ep = entry_points->c.items[i];
            if (!ep || ep->type != JSON_OBJECT) continue;
            const char *g = json_get_str(ep, "group", "");
            if (group && !strcmp(g, group)) json_append(res, json_copy(ep));
        }
    }
    return res;
}

/* basename of a pathlib-ish path (both separators), with trailing sep trimmed. */
static char *path_name(const char *path) {
    if (!path || !*path) return dupstr("");
    size_t n = strlen(path);
    while (n && (path[n - 1] == '/' || path[n - 1] == '\\')) n--;
    size_t start = n;
    while (start && path[start - 1] != '/' && path[start - 1] != '\\') start--;
    if (start == n) return dupstr("");
    return dupstr(path + start);
}

static const char *get_str(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return "";
    json_t *v = json_obj_get(obj, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* manifest_key(manifest) = manifest.key or manifest.name */
static char *manifest_key(const json_t *m) {
    const char *k = get_str(m, "key");
    if (k[0]) return dupstr(k);
    return dupstr(get_str(m, "name"));
}

static int in_str_set(const char *s, const char *const *set, size_t n) {
    if (!s || !*s) return 0;
    for (size_t i = 0; i < n; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

struct json_t *w16_pluginsdis_42aa3c_resolve_manifest_winners(const struct json_t *manifests) {
    json_t *winners = json_object();
    if (!manifests || manifests->type != JSON_ARRAY) return winners;
    for (size_t i = 0; i < manifests->c.count; i++) {
        json_t *m = manifests->c.items[i];
        if (!m || m->type != JSON_OBJECT) continue;
        char *key = manifest_key(m);

        json_t *shadowed = json_obj_get(winners, key);
        if (shadowed && !strcmp(get_str(shadowed, "source"), "bundled") &&
            in_str_set(get_str(m, "source"), (const char *const[]){"user", "project"}, 2)) {
            char *own = path_name(get_str(m, "path"));
            char *bund = path_name(get_str(shadowed, "path"));
            if (own[0] && bund[0] && strcmp(own, bund) != 0) {
                free(own); free(bund); free(key);
                continue;  /* impostor: skip, keep the bundled winner */
            }
            free(own); free(bund);
        }
        json_set(winners, key, json_copy(m));
        free(key);
    }
    return winners;
}
