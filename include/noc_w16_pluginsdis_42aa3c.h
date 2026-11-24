#ifndef NOC_W16_PLUGINSDIS_42AA3C_H
#define NOC_W16_PLUGINSDIS_42AA3C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_pluginsdis_42aa3c_is_skipped_plugin_dir @ hermes_cli/plugins_discovery.py:is_skipped_plugin_dir */
struct json_t *w16_pluginsdis_42aa3c_is_skipped_plugin_dir(const char *name);
/* PoP: w16_pluginsdis_42aa3c_select_entry_point_group @ hermes_cli/plugins_discovery.py:_select_entry_point_group */
struct json_t *w16_pluginsdis_42aa3c_select_entry_point_group(const struct json_t *entry_points,
                                                              const char *group);
/* PoP: w16_pluginsdis_42aa3c_resolve_manifest_winners @ hermes_cli/plugins_discovery.py:resolve_manifest_winners */
struct json_t *w16_pluginsdis_42aa3c_resolve_manifest_winners(const struct json_t *manifests);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_PLUGINSDIS_42AA3C_H */
