#ifndef NOC_W18_PLUGINSCMD_9AA05E_H
#define NOC_W18_PLUGINSCMD_9AA05E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: pluginscmd_looks_like_catalog_name @ hermes_cli/plugins_cmd_catalog.py:looks_like_catalog_name */
int pluginscmd_looks_like_catalog_name(const char *identifier);
/* PoP: pluginscmd_at_catalog_pin @ hermes_cli/plugins_cmd_catalog.py:at_catalog_pin */
int pluginscmd_at_catalog_pin(const struct json_t *sidecar, const char *entry_sha);
/* PoP: pluginscmd_normalized_platforms @ hermes_cli/plugins_cmd_catalog.py:normalized_platforms
 * Sorted JSON array of alias-normalized platform names. */
struct json_t *pluginscmd_normalized_platforms(const struct json_t *platforms);
/* PoP: pluginscmd_added @ hermes_cli/plugins_cmd_catalog.py:_added
 * Sorted JSON array of newly-added identifiers for one surface. */
struct json_t *pluginscmd_added(const char *surface, const struct json_t *old_set,
                                const struct json_t *new_set);
/* PoP: pluginscmd_surface_delta @ hermes_cli/plugins_cmd_catalog.py:surface_delta */
struct json_t *pluginscmd_surface_delta(const struct json_t *old_map, const struct json_t *new_map);
/* PoP: pluginscmd_surface_delta_lines @ hermes_cli/plugins_cmd_catalog.py:surface_delta_lines */
struct json_t *pluginscmd_surface_delta_lines(const struct json_t *delta);
/* PoP: pluginscmd_pin_label @ hermes_cli/plugins_cmd_catalog.py:pin_label */
char *pluginscmd_pin_label(const char *version, const char *sha);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_PLUGINSCMD_9AA05E_H */
