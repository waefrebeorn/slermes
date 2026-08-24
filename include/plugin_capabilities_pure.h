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

/* Return the stored consent hash for plugin_id (malloc'd), or NULL when
 * absent/corrupt. (PoP: .../plugin_capabilities.py:consent_hash) */
char *pc_consent_hash(const char *plugin_id, const char *config_json);

/* Capabilities declared but not yet granted — malloc'd JSON array string.
 * (PoP: .../plugin_capabilities.py:pending_capabilities) */
char *pc_pending_capabilities(const char *plugin_id, const char *declared_json,
                              const char *config_json);

/* True (1) when the declared set differs from the consented hash. No stored
 * consent counts as changed. (PoP: .../declared_set_changed) */
int   pc_declared_set_changed(const char *plugin_id, const char *declared_json,
                              const char *config_json);

/* Pure core of record_consent: applies the consent decision to config_json
 * (merging grants sorted/valid, writing the consent record + UTC timestamp,
 * mirroring legacy allow_* gates) and returns the updated full-config JSON
 * string, malloc'd. Config load/save stays at the caller boundary. NULL on
 * unparseable config. (PoP: .../record_consent) */
char *pc_record_consent(const char *config_json, const char *plugin_id,
                        const char *granted_json, const char *declared_json);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_PLUGIN_CAPABILITIES_PURE_H */
