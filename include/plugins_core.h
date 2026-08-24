/* plugins_core.h — faithful C11 port of the pure-logic surface of
 * hermes_cli/plugins.py: system-prompt section framing, entry-point kind
 * classification, dependency-respecting load order (graphlib topo-sort),
 * portable-plugin namespacing, and the directory manifest scanner.
 *
 * Opaque handles, minimal includes, C11 only. No god header.
 */
#ifndef SLERMES_PLUGINS_CORE_H
#define SLERMES_PLUGINS_CORE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct json_t json_t;

/* ── system-prompt sections ───────────────────────────────────────────── */

/* PLUGIN_SECTIONS_START / PLUGIN_SECTIONS_END sentinels (borrowed). */
const char *plugins_sections_start_marker(void);
const char *plugins_sections_end_marker(void);

/* PoP: plugins_is_valid_system_prompt_section_id @ hermes_cli/plugins.py:is_valid_system_prompt_section_id
 * ^[a-z0-9][a-z0-9._-]{0,127}$ */
bool plugins_is_valid_system_prompt_section_id(const char *value);

/* PoP: plugins_format_system_prompt_section @ hermes_cli/plugins.py:format_system_prompt_section
 * Length-framed, auditable block. Caller frees. */
char *plugins_format_system_prompt_section(const char *section_id,
                                           const char *content);

/* One (id, content) section pair. */
typedef struct {
    const char *id;
    const char *content;
} plugins_section_t;

/* PoP: plugins_format_system_prompt_sections @ hermes_cli/plugins.py:format_system_prompt_sections
 * "" for an empty list, else the sentinel-wrapped container. Caller frees. */
char *plugins_format_system_prompt_sections(const plugins_section_t *sections,
                                            size_t n_sections);

/* ── kind detection ───────────────────────────────────────────────────── */

/* PoP: plugins_detect_kind_from_source @ hermes_cli/plugins.py:_detect_kind_from_source
 * "exclusive", "model-provider", or NULL (borrowed). */
const char *plugins_detect_kind_from_source(const char *source_text);

/* PoP: plugins_classify_entrypoint_value_kind @ hermes_cli/plugins.py:_classify_entrypoint_value_kind
 * Module part of "<module>:<attr>" resolved to source and scanned; falls back
 * to "standalone". source_reader may be NULL (then always "standalone"). */
const char *plugins_classify_entrypoint_value_kind(
    const char *value,
    char *(*source_reader)(const char *module_name));

/* PoP: plugins_portable_skill_namespace @ hermes_cli/plugins.py:_portable_skill_namespace
 * "agent-plugin-<slug>-<sha256[:8]>". Caller frees. */
char *plugins_portable_skill_namespace(const char *key);

/* ── load order (graphlib.TopologicalSorter parity) ───────────────────── */

/* One manifest as the load-order resolver sees it: its key, its display name,
 * and the plugin ids it requires (requires_plugins[].id). */
typedef struct {
    const char *key;
    const char *name;
    const char *const *requires;   /* NULL-terminated, may be NULL */
} plugins_dep_node_t;

/* PoP: plugins_resolve_plugin_load_order @ hermes_cli/plugins.py:resolve_plugin_load_order
 * Dependency-respecting order: when A requires B, B sorts first; ties break
 * alphabetically. A dependency cycle falls back to plain alphabetical order
 * for every plugin. Unresolvable / self dependencies are ignored (never
 * dropped from the order). Returns malloc'd array of borrowed key pointers. */
const char **plugins_resolve_plugin_load_order(const plugins_dep_node_t *nodes,
                                               size_t n_nodes,
                                               size_t *out_n);

/* ── directory manifest scanning ──────────────────────────────────────── */

typedef struct plugins_scan_result_t plugins_scan_result_t;

/* PoP: plugins_scan_directory @ hermes_cli/plugins.py:PluginManager._scan_directory
 * Reads plugin.yaml / plugin.yml / plugin.json manifests from the children of
 * `path`, honouring the flat + one-level-category layouts (depth cap 2) and
 * the top-level skip_names allow-list. Portable (plugin.json) packages are
 * marked portable and get a skill namespace. */
plugins_scan_result_t *plugins_scan_directory(const char *path,
                                              const char *source,
                                              const char *const *skip_names);

size_t plugins_scan_count(const plugins_scan_result_t *r);
const char *plugins_scan_key(const plugins_scan_result_t *r, size_t i);
const char *plugins_scan_name(const plugins_scan_result_t *r, size_t i);
const char *plugins_scan_path(const plugins_scan_result_t *r, size_t i);
const char *plugins_scan_source(const plugins_scan_result_t *r, size_t i);
const char *plugins_scan_kind(const plugins_scan_result_t *r, size_t i);
const char *plugins_scan_skill_namespace(const plugins_scan_result_t *r, size_t i);
bool plugins_scan_portable(const plugins_scan_result_t *r, size_t i);
void plugins_scan_free(plugins_scan_result_t *r);

/* PoP: plugins_collect_directory_manifests @ hermes_cli/plugins.py:PluginManager._collect_directory_manifests
 * bundled plugins (minus the four self-discovering categories), then
 * bundled/platforms, then ~/.hermes/plugins, then ./.hermes/plugins when
 * HERMES_ENABLE_PROJECT_PLUGINS is set — in that exact order. */
plugins_scan_result_t *plugins_collect_directory_manifests(void);

/* PoP: plugins_has_enabled_portable_mcp @ hermes_cli/plugins.py:PluginManager.has_enabled_portable_mcp
 * True when an enabled, non-disabled portable package ships an mcp.json.
 * `config` is the raw config as a JSON string (plugins.enabled / .disabled).
 * Fails closed under HERMES_SAFE_MODE. */
bool plugins_has_enabled_portable_mcp(const char *config_json);

/* PoP: plugins_toolset_keys_cache_path @ hermes_cli/plugins.py:_plugin_toolset_keys_cache_path
 * <hermes home>/cache/plugin_toolset_keys.json. Caller frees. */
char *plugins_toolset_keys_cache_path(void);

/* PoP: plugins_persist_toolset_keys @ hermes_cli/plugins.py:_persist_plugin_toolset_keys
 * Atomic (mkstemp + rename) write of {"toolset_keys": [...],
 * "portable_mcp": [...]}, both sorted unique. Best-effort: never raises. */
void plugins_persist_toolset_keys(const char *const *toolset_keys,
                                  size_t n_toolset_keys,
                                  const char *const *portable_mcp,
                                  size_t n_portable_mcp);

/* PoP: plugins_read_keys_cache @ hermes_cli/plugins.py:_read_plugin_keys_cache
 * The cache blob as a JSON string, or NULL when absent/unreadable/not an
 * object. Caller frees. */
char *plugins_read_keys_cache(void);

/* Read the persisted "toolset_keys" (or "portable_mcp") list from the cache.
 * Returns NULL when the key is absent or not a list of strings — mirroring the
 * isinstance guards in get_plugin_toolset_keys_nowait /
 * get_portable_mcp_server_names_nowait. Caller frees with plugins_free_list. */
char **plugins_cached_string_list(const char *field, size_t *out_n);

/* List the immediate subdirectories of `dir` that contain a SKILL.md.
 * Caller frees with plugins_free_list. */
char **plugins_list_skill_dirs(const char *dir, size_t *out_n);

void plugins_free_list(char **list, size_t n);

/* ── wave 8: plugin state / config pure helpers ───────────────────────── */

/* PoP: plugins_select_entry_point_group @ hermes_cli/plugins.py:_select_entry_point_group
 * `entry_points` is a JSON array of entry-point objects each carrying a
 * "group" string (the C stand-in for importlib.metadata's EntryPoints /
 * dict-of-groups). Returns a newly-allocated JSON array (caller frees) with
 * only the entries whose "group" matches. */
json_t *plugins_select_entry_point_group(const json_t *entry_points,
                                         const char *group);

/* PoP: plugins_plugin_data_namespace @ hermes_cli/plugins.py:_plugin_data_namespace
 * One Windows-safe directory component for plugin-owned data. Portable
 * Agent Plugin namespaces that already match the PLUGIN_DATA pattern pass
 * through; everything else goes through the portable namespace algorithm.
 * Caller frees. */
char *plugins_plugin_data_namespace(const char *plugin_id,
                                    const char *skill_namespace);

/* PoP: plugins_nested_plugin_value @ hermes_cli/plugins.py:_nested_plugin_value
 * Walk `root` (a JSON object tree) along `segments` (dot-split key path).
 * Returns the node at the path, or `default` when any step is missing or
 * not an object. Borrowed pointer. */
const json_t *plugins_nested_plugin_value(const json_t *root,
                                          const char *const *segments,
                                          size_t n_segments,
                                          const json_t *default_val);

/* PoP: plugins_nested_plugin_mapping @ hermes_cli/plugins.py:_nested_plugin_mapping
 * Build {segments[0]: {segments[1]: ... value}} — a fresh JSON object tree.
 * `value` is deep-copied into the leaf; caller frees the returned root. */
json_t *plugins_nested_plugin_mapping(const char *const *segments,
                                      size_t n_segments,
                                      const json_t *value);

/* PoP: plugins_state_quota_bytes @ hermes_cli/plugins.py:PluginState.quota_bytes
 * The per-plugin state quota: 10 MiB. */
size_t plugins_state_quota_bytes(void);

/* PoP: plugins_validate_state_key @ hermes_cli/plugins.py:PluginState._validate_key
 * True when key is 1-128 chars matching ^[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}$
 * and contains no "..". */
bool plugins_validate_state_key(const char *key);

/* PoP: plugins_setting_segment_valid @ hermes_cli/plugins.py:_PLUGIN_SETTING_SEGMENT_RE.fullmatch
 * ^[A-Za-z0-9][A-Za-z0-9_-]{0,127}$ */
bool plugins_setting_segment_valid(const char *segment);

/* PoP: plugins_relative_segments @ hermes_cli/plugins.py:_plugin_relative_segments
 * Validate and split a plugin-relative settings key on '.'. On success
 * returns a malloc'd array of malloc'd segments (*out_n set); on failure
 * returns NULL (Python raises ValueError). Reserved roots ("model",
 * "plugins", "security", "settings", case-insensitive), '/', '\\', empty
 * keys and invalid segments are rejected. Caller frees via
 * plugins_free_list. */
char **plugins_relative_segments(const char *key, size_t *out_n);

/* PoP: plugins_state_data_dir @ hermes_cli/plugins.py:PluginState.data_dir
 * <hermes home>/plugin-data/<data_namespace>. Caller frees. */
char *plugins_state_data_dir(const char *data_namespace);

/* PoP: plugins_state_path @ hermes_cli/plugins.py:PluginState.path
 * <data_dir>/state.json. Caller frees. */
char *plugins_state_path(const char *data_namespace);

/* PoP: plugins_state_lock_path @ hermes_cli/plugins.py:_locked_plugin_state.lock_path
 * Sibling lock file ".<name>.lock" next to `path` — atomic replacement
 * changes the target's inode, so the lock lives beside it. Caller frees. */
char *plugins_state_lock_path(const char *path);

/* ── wave 9: config schema, manifest v2, serialized replacement, origin ─ */

/* PoP: plg_validate_config_schema @ hermes_cli/plugins.py:validate_config_schema
 * Fresh JSON array of human-actionable warning strings (caller frees).
 * Never fails; non-mapping inputs yield an empty array (#64165). */
json_t *plg_validate_config_schema(const char *plugin_id,
                                   const json_t *schema,
                                   const json_t *settings);

/* PoP: plg_parse_manifest_v2_fields @ hermes_cli/plugins.py:_parse_manifest_v2_fields
 * Fresh JSON object of normalized PluginManifest kwargs (caller frees):
 * manifest_version, api_version (int|null), requires_plugins, config_schema,
 * python_dependencies, license, homepage, tags. Problems warn, never fail. */
json_t *plg_parse_manifest_v2_fields(const json_t *data, const char *key);

/* PoP: plg_serialized_replacement @ hermes_cli/plugins.py:_serialized_replacement
 * Run method(args) under the replacement-coordinator lock — one atomic
 * snapshot/write/lease transaction. Returns the callback's result. */
void *plg_serialized_replacement(void *(*method)(void *), void *args);

/* PoP: plg_resolve_module_origin @ hermes_cli/plugins.py:resolve_module_origin
 * Module source path without importing it, or NULL. Caller frees. */
char *plg_resolve_module_origin(const char *module_name);

#endif /* SLERMES_PLUGINS_CORE_H */
