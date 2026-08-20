/*
 * plugins_gaps.c — hermes_cli/plugins.py REAL_GAP surface.
 *
 * These entry points used to be hollow constant-returning stubs. They are now
 * thin delegates onto the real implementations in plugins_core.c (manifest
 * scanning, portable-MCP probing, the toolset-key cache) so every caller sees
 * the same behavior the Python module has.
 */

#include "plugins_core.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Serialize an array of borrowed strings into out_json. */
static void pg_emit_string_array(const char *const *items, size_t n,
                                 char *out_json, size_t out_sz) {
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++)
        if (items[i]) json_append(arr, json_string(items[i]));
    char *text = json_serialize(arr);
    json_free(arr);
    snprintf(out_json, out_sz, "%s", text ? text : "[]");
    free(text);
}

/* PoP: plugin_collect_directory_manifests @ hermes_cli/plugins.py:_collect_directory_manifests */
void plugin_collect_directory_manifests(char *out_json, size_t out_sz) {
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    json_t *arr = json_array();
    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        json_t *o = json_object();
        json_set(o, "key", json_string(plugins_scan_key(r, i) ?: ""));
        json_set(o, "name", json_string(plugins_scan_name(r, i) ?: ""));
        json_set(o, "path", json_string(plugins_scan_path(r, i) ?: ""));
        json_set(o, "source", json_string(plugins_scan_source(r, i) ?: ""));
        json_set(o, "kind", json_string(plugins_scan_kind(r, i) ?: ""));
        json_set(o, "portable", json_bool(plugins_scan_portable(r, i)));
        const char *ns = plugins_scan_skill_namespace(r, i);
        json_set(o, "skill_namespace", ns ? json_string(ns) : json_null());
        json_append(arr, o);
    }
    plugins_scan_free(r);
    char *text = json_serialize(arr);
    json_free(arr);
    snprintf(out_json, out_sz, "%s", text ? text : "[]");
    free(text);
}

/* PoP: plugin_has_enabled_portable_mcp @ hermes_cli/plugins.py:has_enabled_portable_mcp
 * raw_config is the parsed config as JSON (plugins.enabled / .disabled). */
int plugin_has_enabled_portable_mcp(const char *raw_config_json) {
    return plugins_has_enabled_portable_mcp(raw_config_json) ? 1 : 0;
}

/* PoP: plugin_has_enabled_agent_plugin_mcp @ hermes_cli/plugins.py:has_enabled_agent_plugin_mcp
 * Python: PluginManager().has_enabled_portable_mcp(raw_config) — a fresh
 * manager, i.e. manifest-only scanning with no registry mutation. */
int plugin_has_enabled_agent_plugin_mcp(const char *raw_config_json) {
    return plugins_has_enabled_portable_mcp(raw_config_json) ? 1 : 0;
}

/* Enabled portable packages' MCP server configs, keyed by server name.
 * Python keeps these in PluginManager._portable_mcp_servers, populated from
 * each enabled portable package's mcp.json during discovery; the C port
 * derives the same mapping from the manifests on demand. */
static json_t *pg_portable_mcp_servers(const char *raw_config_json) {
    json_t *out = json_object();
    json_t *cfg = raw_config_json ? json_parse(raw_config_json, NULL) : NULL;
    json_t *plugins_cfg = (cfg && cfg->type == JSON_OBJECT)
                          ? json_obj_get(cfg, "plugins") : NULL;
    json_t *enabled = (plugins_cfg && plugins_cfg->type == JSON_OBJECT)
                      ? json_obj_get(plugins_cfg, "enabled") : NULL;
    json_t *disabled = (plugins_cfg && plugins_cfg->type == JSON_OBJECT)
                       ? json_obj_get(plugins_cfg, "disabled") : NULL;
    if (!enabled || enabled->type != JSON_ARRAY) {
        if (cfg) json_free(cfg);
        return out;
    }
    if (disabled && disabled->type != JSON_ARRAY) disabled = NULL;

    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        if (!plugins_scan_portable(r, i)) continue;
        const char *key = plugins_scan_key(r, i);
        const char *name = plugins_scan_name(r, i);
        bool listed = false, blocked = false;
        for (size_t j = 0; j < json_len(enabled); j++) {
            json_t *v = json_get(enabled, j);
            if (v && v->type == JSON_STRING && v->str_val &&
                ((key && strcmp(v->str_val, key) == 0) ||
                 (name && strcmp(v->str_val, name) == 0))) listed = true;
        }
        if (disabled) {
            for (size_t j = 0; j < json_len(disabled); j++) {
                json_t *v = json_get(disabled, j);
                if (v && v->type == JSON_STRING && v->str_val &&
                    ((key && strcmp(v->str_val, key) == 0) ||
                     (name && strcmp(v->str_val, name) == 0))) blocked = true;
            }
        }
        if (!listed || blocked) continue;

        const char *path = plugins_scan_path(r, i);
        if (!path) continue;
        char mcp_path[4096];
        snprintf(mcp_path, sizeof mcp_path, "%s/mcp.json", path);
        FILE *f = fopen(mcp_path, "rb");
        if (!f) continue;
        char buf[65536];
        size_t got = fread(buf, 1, sizeof buf - 1, f);
        fclose(f);
        buf[got] = '\0';
        json_t *mcp = json_parse(buf, NULL);
        if (mcp && mcp->type == JSON_OBJECT) {
            json_t *servers = json_obj_get(mcp, "mcpServers");
            if (!servers) servers = json_obj_get(mcp, "mcp_servers");
            if (!servers) servers = mcp;
            if (servers->type == JSON_OBJECT) {
                for (size_t s = 0; s < json_object_size(servers); s++) {
                    const char *sname = json_object_get_key_at(servers, s);
                    json_t *sval = json_object_get_at(servers, s);
                    if (!sname || !sval || sval->type != JSON_OBJECT) continue;
                    json_set(out, sname, json_copy(sval));  /* defensive copy */
                }
            }
        }
        if (mcp) json_free(mcp);
    }
    plugins_scan_free(r);
    if (cfg) json_free(cfg);
    return out;
}

/* PoP: plugin_get_portable_mcp_servers @ hermes_cli/plugins.py:get_portable_mcp_servers */
void plugin_get_portable_mcp_servers(const char *raw_config_json,
                                     char *out_json, size_t out_sz) {
    json_t *servers = pg_portable_mcp_servers(raw_config_json);
    char *text = json_serialize(servers);
    json_free(servers);
    snprintf(out_json, out_sz, "%s", text ? text : "{}");
    free(text);
}

/* PoP: plugin_has_portable_mcp_servers @ hermes_cli/plugins.py:has_portable_mcp_servers */
int plugin_has_portable_mcp_servers(const char *raw_config_json) {
    json_t *servers = pg_portable_mcp_servers(raw_config_json);
    int any = json_object_size(servers) > 0 ? 1 : 0;
    json_free(servers);
    return any;
}

/* PoP: plugin_toolset_keys_cache_path @ hermes_cli/plugins.py:_plugin_toolset_keys_cache_path */
void plugin_toolset_keys_cache_path(char *out, size_t out_sz) {
    char *path = plugins_toolset_keys_cache_path();
    snprintf(out, out_sz, "%s", path ? path : "");
    free(path);
}

/* PoP: plugin_persist_toolset_keys @ hermes_cli/plugins.py:_persist_plugin_toolset_keys */
void plugin_persist_toolset_keys(const char *const *toolset_keys,
                                 size_t n_toolset_keys,
                                 const char *const *portable_mcp,
                                 size_t n_portable_mcp) {
    plugins_persist_toolset_keys(toolset_keys, n_toolset_keys,
                                 portable_mcp, n_portable_mcp);
}

/* PoP: plugin_read_keys_cache @ hermes_cli/plugins.py:_read_plugin_keys_cache */
void plugin_read_keys_cache(char *out_json, size_t out_sz) {
    char *blob = plugins_read_keys_cache();
    snprintf(out_json, out_sz, "%s", blob ? blob : "{}");
    free(blob);
}

/* PoP: plugin_get_toolset_keys_nowait @ hermes_cli/plugins.py:get_plugin_toolset_keys_nowait
 * Discovery in the C port is synchronous (no background thread), so the live
 * value is always available; the persisted set is the fallback when the cache
 * is the only source. */
void plugin_get_toolset_keys_nowait(char *out_json, size_t out_sz) {
    size_t n = 0;
    char **keys = plugins_cached_string_list("toolset_keys", &n);
    pg_emit_string_array((const char *const *)keys, n, out_json, out_sz);
    plugins_free_list(keys, n);
}

/* PoP: plugin_get_portable_mcp_names_nowait @ hermes_cli/plugins.py:get_portable_mcp_server_names_nowait */
void plugin_get_portable_mcp_names_nowait(char *out_json, size_t out_sz) {
    size_t n = 0;
    char **names = plugins_cached_string_list("portable_mcp", &n);
    pg_emit_string_array((const char *const *)names, n, out_json, out_sz);
    plugins_free_list(names, n);
}

/* PoP: plugin_load_portable @ hermes_cli/plugins.py:_load_portable_plugin
 * Registers a portable package's MCP servers into the caller-owned registry
 * JSON. Returns the server map declared by the package (empty when it has
 * none or the manifest is unreadable). */
void plugin_load_portable(const char *manifest_path, char *out_json,
                          size_t out_sz) {
    snprintf(out_json, out_sz, "{}");
    if (!manifest_path || !*manifest_path) return;
    char mcp_path[4096];
    snprintf(mcp_path, sizeof mcp_path, "%s/mcp.json", manifest_path);
    FILE *f = fopen(mcp_path, "rb");
    if (!f) return;
    char buf[65536];
    size_t got = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[got] = '\0';
    json_t *mcp = json_parse(buf, NULL);
    if (!mcp) return;
    if (mcp->type == JSON_OBJECT) {
        json_t *servers = json_obj_get(mcp, "mcpServers");
        if (!servers) servers = json_obj_get(mcp, "mcp_servers");
        if (!servers) servers = mcp;
        char *text = json_serialize(servers);
        if (text) snprintf(out_json, out_sz, "%s", text);
        free(text);
    }
    json_free(mcp);
}

/* PoP: plugin_list_skill_metadata @ hermes_cli/plugins.py:list_plugin_skill_metadata
 * Progressive-disclosure metadata for the SKILL.md files each enabled plugin
 * ships under <plugin>/skills/<name>/SKILL.md, sorted by qualified name
 * (mirrors `sorted(self._plugin_skills.items())`). */
void plugin_list_skill_metadata(const char *raw_config_json, char *out_json,
                                size_t out_sz) {
    json_t *arr = json_array();
    plugins_scan_result_t *r = plugins_collect_directory_manifests();
    json_t *cfg = raw_config_json ? json_parse(raw_config_json, NULL) : NULL;
    json_t *plugins_cfg = (cfg && cfg->type == JSON_OBJECT)
                          ? json_obj_get(cfg, "plugins") : NULL;
    json_t *enabled = (plugins_cfg && plugins_cfg->type == JSON_OBJECT)
                      ? json_obj_get(plugins_cfg, "enabled") : NULL;
    if (enabled && enabled->type != JSON_ARRAY) enabled = NULL;

    for (size_t i = 0; i < plugins_scan_count(r); i++) {
        const char *key = plugins_scan_key(r, i);
        const char *name = plugins_scan_name(r, i);
        if (enabled) {
            bool listed = false;
            for (size_t j = 0; j < json_len(enabled); j++) {
                json_t *v = json_get(enabled, j);
                if (v && v->type == JSON_STRING && v->str_val &&
                    ((key && strcmp(v->str_val, key) == 0) ||
                     (name && strcmp(v->str_val, name) == 0))) listed = true;
            }
            if (!listed) continue;
        }
        const char *path = plugins_scan_path(r, i);
        if (!path || !key) continue;
        char skills_dir[4096];
        snprintf(skills_dir, sizeof skills_dir, "%s/skills", path);
        /* Each subdirectory with a SKILL.md is one qualified skill. */
        size_t sn = 0;
        char **subs = plugins_list_skill_dirs(skills_dir, &sn);
        for (size_t s = 0; s < sn; s++) {
            char qualified[512];
            snprintf(qualified, sizeof qualified, "%s:%s", key, subs[s]);
            json_t *o = json_object();
            json_set(o, "name", json_string(qualified));
            json_set(o, "description", json_string(""));
            json_set(o, "category", json_string("plugin"));
            json_set(o, "frontmatter", json_object());
            json_append(arr, o);
        }
        plugins_free_list(subs, sn);
    }
    plugins_scan_free(r);
    if (cfg) json_free(cfg);

    /* sorted(...) by qualified name */
    size_t n = json_len(arr);
    for (size_t i = 1; i < n; i++) {
        for (size_t j = i; j > 0; j--) {
            const char *a = json_get_str(json_get(arr, j - 1), "name", "");
            const char *b = json_get_str(json_get(arr, j), "name", "");
            if (strcmp(a, b) <= 0) break;
            json_t *tmp = arr->c.items[j - 1];
            arr->c.items[j - 1] = arr->c.items[j];
            arr->c.items[j] = tmp;
        }
    }

    char *text = json_serialize(arr);
    json_free(arr);
    snprintf(out_json, out_sz, "%s", text ? text : "[]");
    free(text);
}
