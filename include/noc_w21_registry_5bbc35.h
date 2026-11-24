/*
 * noc_w21_registry_5bbc35.h -- local JSON registry of approved remote meet
 * nodes (port of plugins/google_meet/node/registry.py).
 *
 * PoP: w21_registry_5bbc35_default_path @ plugins/google_meet/node/registry.py:_default_path
 * PoP: w21_registry_5bbc35_get @ plugins/google_meet/node/registry.py:NodeRegistry.get
 * PoP: w21_registry_5bbc35_add @ plugins/google_meet/node/registry.py:NodeRegistry.add
 * PoP: w21_registry_5bbc35_remove @ plugins/google_meet/node/registry.py:NodeRegistry.remove
 * PoP: w21_registry_5bbc35_list_all @ plugins/google_meet/node/registry.py:NodeRegistry.list_all
 * PoP: w21_registry_5bbc35_resolve @ plugins/google_meet/node/registry.py:NodeRegistry.resolve
 *
 * The file is real: `read_json` (missing/unreadable/malformed -> None,
 * utf-8-sig) and `atomic_json_write` (indent 2, owner-only mode) are performed
 * on disk. The clock is the one seam -- `added_at` comes from the caller so the
 * differential stays deterministic.
 */
#ifndef NOC_W21_REGISTRY_5BBC35_H
#define NOC_W21_REGISTRY_5BBC35_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _default_path() with the HERMES_HOME value injected (the env seam):
 * Path(hermes_home) / "workspace" / "meetings" / "nodes.json", or the platform
 * default when the injected value is empty. Returns a string. */
json_t *w21_registry_5bbc35_default_path(const json_t *hermes_home_env);

/* Each op takes the registry file path. Results:
 *   get     -> the {name, ...entry} dict, or null
 *   add     -> {"status":"ok"} or {"status":"error","type":"ValueError","message":...}
 *   remove  -> bool
 *   list_all-> [ {name, ...entry} ... ] sorted by name
 *   resolve -> entry or null (sole node only when *chrome_node* is falsy) */
json_t *w21_registry_5bbc35_get(const char *path, const char *name);
json_t *w21_registry_5bbc35_add(const char *path, const json_t *name,
                               const json_t *url, const json_t *token, double now);
json_t *w21_registry_5bbc35_remove(const char *path, const char *name);
json_t *w21_registry_5bbc35_list_all(const char *path);
json_t *w21_registry_5bbc35_resolve(const char *path, const json_t *chrome_node);

/* The raw `nodes` map as the file holds it ({} when missing/malformed); exposed
 * so the harness can report the on-disk state after a write. */
json_t *w21_registry_5bbc35_load_nodes(const char *path);

/* The whole registry document as parsed from disk (null when missing or
 * malformed), so the gate can compare the exact file the port wrote. */
json_t *w21_registry_5bbc35_read_document(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_REGISTRY_5BBC35_H */
