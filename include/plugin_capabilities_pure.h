#ifndef HERMES_PLUGIN_CAPABILITIES_PURE_H
#define HERMES_PLUGIN_CAPABILITIES_PURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure, deterministic helpers ported from hermes_cli/plugin_capabilities.py.
 * Config is passed as a JSON string. All char* returns are malloc'd (free()). */

int   pc_is_valid_capability(const char *id);
char *pc_parse_declared_capabilities(const char *raw_json, const char *plugin_name);
char *pc_capability_set_hash(const char *caps_json);
char *pc_granted_capabilities(const char *plugin_id, const char *config_json);
int   pc_legacy_gate_set(const char *entry_json, const char *capability);
int   pc_plugin_capability_granted(const char *plugin_id, const char *capability,
                                    const char *config_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PLUGIN_CAPABILITIES_PURE_H */
