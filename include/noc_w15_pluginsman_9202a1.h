#ifndef NOC_W15_PLUGINSMAN_9202A1_H
#define NOC_W15_PLUGINSMAN_9202A1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* C11 port of the pure manifest-normalization helpers in
 * hermes_cli/plugins_manifest.py (gate w15_pluginsman_9202a1).
 * All char* returns are malloc'd and caller-owned unless noted. */

/* PoP: w15_pluginsman_9202a1_portable_slug @ hermes_cli/plugins_manifest.py:_portable_slug */
char *w15_pluginsman_9202a1_portable_slug(const char *key);
/* PoP: w15_pluginsman_9202a1_portable_mcp_server_name @ hermes_cli/plugins_manifest.py:portable_mcp_server_name */
char *w15_pluginsman_9202a1_portable_mcp_server_name(const char *key,
                                                    const char *server);
/* PoP: w15_pluginsman_9202a1_display_author @ hermes_cli/plugins_manifest.py:_display_author */
char *w15_pluginsman_9202a1_display_author(struct json_t *value);
/* PoP: w15_pluginsman_9202a1_config_key_path @ hermes_cli/plugins_manifest.py:_config_key_path */
char *w15_pluginsman_9202a1_config_key_path(const char *item);
/* PoP: w15_pluginsman_9202a1_manifest_config_keys @ hermes_cli/plugins_manifest.py:manifest_config_keys */
struct json_t *w15_pluginsman_9202a1_manifest_config_keys(struct json_t *data);
/* PoP: w15_pluginsman_9202a1_detect_kind_from_source @ hermes_cli/plugins_manifest.py:_detect_kind_from_source
 * Returns a borrowed static string or NULL. */
const char *w15_pluginsman_9202a1_detect_kind_from_source(const char *source);
/* PoP: w15_pluginsman_9202a1_version_satisfies @ hermes_cli/plugins_manifest.py:version_satisfies */
int w15_pluginsman_9202a1_version_satisfies(const char *spec, const char *current);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_PLUGINSMAN_9202A1_H */
