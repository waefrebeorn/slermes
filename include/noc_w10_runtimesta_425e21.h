#ifndef NOC_W10_RUNTIMESTA_425E21_H
#define NOC_W10_RUNTIMESTA_425E21_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_runtimesta_425e21_committed @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * row.get("committed") or facts_digest != row["facts_before"]. `facts_digest`
 * is the injected _digest(runtime_facts_path(project)) value. Caller frees. */
json_t *w10_runtimesta_425e21_committed(const json_t *row, const char *facts_digest);

/* PoP: w10_runtimesta_425e21_plugin_paths_valid @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * Pure path-escape validation (lines 65-67); all path strings are injected
 * already-resolved values. Caller frees. */
json_t *w10_runtimesta_425e21_plugin_paths_valid(const char *home, const char *target,
                                                const char *backup, const char *metadata);

/* PoP: w10_runtimesta_425e21_config_path_valid @ hermes_cli/runtime_state.py:recover_publication
 * Pure config-path validation (line 108). Caller frees. */
json_t *w10_runtimesta_425e21_config_path_valid(const char *config, const char *home);

/* PoP: w10_runtimesta_425e21_path_relative_to @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * Path.is_relative_to for already-resolved POSIX path strings. Caller frees. */
json_t *w10_runtimesta_425e21_path_relative_to(const char *path, const char *parent);

/* PoP: w10_runtimesta_425e21_path_leaf @ hermes_cli/runtime_state.py:recover_publication
 * Path.name of a POSIX path string. Caller frees. */
json_t *w10_runtimesta_425e21_path_leaf(const char *path);

/* PoP: w10_runtimesta_425e21_path_parent @ hermes_cli/runtime_state.py:_recover_plugin_publication
 * Path.parent of a POSIX path string. Caller frees. */
json_t *w10_runtimesta_425e21_path_parent(const char *path);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_RUNTIMESTA_425E21_H */
