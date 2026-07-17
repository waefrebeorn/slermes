/*
 * plugin_manifest.h — pure data-model port of hermes_cli/plugins.py
 *                       (PluginManifest + _parse_manifest logic)
 *
 * Faithful C port of the *pure* manifest-parsing surface of the plugin
 * system: given a parsed manifest dictionary (provided as JSON — a
 * superset-compatible encoding of the YAML that the Python loader reads) plus
 * the plugin directory name / prefix / source, produce a validated
 * PluginManifest with the same derived fields the Python code computes:
 *
 *   - name           (manifest "name" or the directory name)
 *   - key            (prefix/dir-name, or name when no prefix)
 *   - kind           (lowercased; invalid -> "standalone", with the
 *                    model-provider / exclusive auto-coercion applied via an
 *                    injected detector callback so this module stays pure)
 *   - requires_env   (normalized list of {name, extra_json})
 *
 * Opaque struct, minimal includes, C11. The YAML->JSON step is the host's
 * responsibility; everything downstream is real logic.
 */

#ifndef HERMES_PLUGIN_MANIFEST_H
#define HERMES_PLUGIN_MANIFEST_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A single requires_env entry: a bare name, or a {name: ...} dict whose extra
 * keys are preserved verbatim as JSON. */
typedef struct {
    char *name;        /* always set (malloc'd) */
    char *extra_json;  /* NULL unless the entry was a dict; the whole dict
                        * re-serialized as JSON (incl. "name") */
} plugin_req_env_t;

typedef struct plugin_manifest_t plugin_manifest_t;

/* Injected detector: given the plugin directory name, return one of the
 * canonical kinds ("standalone", "backend", "exclusive", "platform",
 * "model-provider") or NULL to leave the kind as already derived. Mirrors the
 * Python heuristic that scans __init__.py for register_memory_provider /
 * register_provider + ProviderProfile — the host owns filesystem reads, so it
 * performs them inside this callback. May be NULL. */
typedef const char *(*plugin_kind_detector_t)(const char *plugin_dir_name);

/* Valid plugin kinds (mirrors _VALID_PLUGIN_KINDS). */
bool plugin_manifest_kind_is_valid(const char *kind);

/* Parse `json` (a JSON object encoding the manifest dict) into a manifest.
 * `plugin_dir_name` is the on-disk directory (used as the name fallback and
 * in the derived key). `prefix` may be NULL/"" (flat plugin) or a category
 * path (nested plugin -> "prefix/dir-name"). `source` is "user", "project",
 * or "entrypoint". `detector` may be NULL.
 * Returns NULL on parse failure (caller frees with plugin_manifest_free). */
plugin_manifest_t *plugin_manifest_parse(const char *json,
                                         const char *plugin_dir_name,
                                         const char *prefix,
                                         const char *source,
                                         const plugin_kind_detector_t detector);

void plugin_manifest_free(plugin_manifest_t *m);

/* Accessors (all borrowed; valid until the manifest is freed). */
const char *plugin_manifest_name(const plugin_manifest_t *m);
const char *plugin_manifest_version(const plugin_manifest_t *m);
const char *plugin_manifest_description(const plugin_manifest_t *m);
const char *plugin_manifest_author(const plugin_manifest_t *m);
const char *plugin_manifest_source(const plugin_manifest_t *m);
const char *plugin_manifest_path(const plugin_manifest_t *m);
const char *plugin_manifest_kind(const plugin_manifest_t *m);
const char *plugin_manifest_key(const plugin_manifest_t *m);

/* requires_env */
size_t plugin_manifest_req_env_count(const plugin_manifest_t *m);
const plugin_req_env_t *plugin_manifest_req_env(const plugin_manifest_t *m, size_t i);

/* provides_* */
size_t plugin_manifest_provides_tools_count(const plugin_manifest_t *m);
const char *plugin_manifest_provides_tool(const plugin_manifest_t *m, size_t i);
size_t plugin_manifest_provides_hooks_count(const plugin_manifest_t *m);
const char *plugin_manifest_provides_hook(const plugin_manifest_t *m, size_t i);

/* Serialize back to a JSON object (round-trip; mirrors PluginManifest fields).
 * Caller frees. */
char *plugin_manifest_to_json(const plugin_manifest_t *m);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PLUGIN_MANIFEST_H */
