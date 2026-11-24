/* noc_w19_plugin_a94384.h - public C API for agent/vault_backends/plugin.py port. */
#ifndef NOC_W19_PLUGIN_A94384_H
#define NOC_W19_PLUGIN_A94384_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Every method below answers an outcome envelope:
 *   {"status":"ok","value":<v>}   or   {"status":"error","message":"<fixed text>"}
 * The plugin's own error text never survives: an exception becomes the fixed
 * core-owned sentence, exactly as PluginBackendError does in the live module.
 * The plugin collaborator is injected as a JSON spec:
 *   {"list_items":{"raise":b,"items":[meta...]}, "get_meta":{"raise":b,"value":meta|null},
 *    "resolve_password":{"raise":b,"value":<json>}, "resolve_otp":{...},
 *    "resolve_secret":{"raise":b,"value":{...}}, "fill_context":{"raise":b,"enter_raise":b,
 *    "exit_raise":b}}
 */

/* PoP: w19_plugin_a94384_text @ agent/vault_backends/plugin.py:_text
 * An exact str copy, or {"invalid":true} where the live code returns _INVALID. */
json_t *w19_plugin_a94384_text(json_t *value);

/* PoP: w19_plugin_a94384_materialize @ agent/vault_backends/plugin.py:_materialize
 * A core-owned meta copy, or {"invalid":true} where the live code returns _INVALID. */
json_t *w19_plugin_a94384_materialize(json_t *meta);

/* PoP: w19_plugin_a94384_owns @ agent/vault_backends/plugin.py:PluginLoginBackend.owns */
int w19_plugin_a94384_owns(const char *name, const char *handle);

/* PoP: w19_plugin_a94384_list_items @ agent/vault_backends/plugin.py:PluginLoginBackend.list_items */
json_t *w19_plugin_a94384_list_items(const char *name, json_t *plugin);
/* PoP: w19_plugin_a94384_get_meta @ agent/vault_backends/plugin.py:PluginLoginBackend.get_meta */
json_t *w19_plugin_a94384_get_meta(const char *name, json_t *plugin, const char *handle);
/* PoP: w19_plugin_a94384_resolve_password @ agent/vault_backends/plugin.py:PluginLoginBackend.resolve_password */
json_t *w19_plugin_a94384_resolve_password(const char *name, json_t *plugin, const char *handle);
/* PoP: w19_plugin_a94384_resolve_otp @ agent/vault_backends/plugin.py:PluginLoginBackend.resolve_otp */
json_t *w19_plugin_a94384_resolve_otp(const char *name, json_t *plugin, const char *handle);
/* PoP: w19_plugin_a94384_resolve_secret @ agent/vault_backends/plugin.py:PluginLoginBackend.resolve_secret */
json_t *w19_plugin_a94384_resolve_secret(const char *name, json_t *plugin, const char *handle);
/* PoP: w19_plugin_a94384_fill_context @ agent/vault_backends/plugin.py:PluginLoginBackend.fill_context */
json_t *w19_plugin_a94384_fill_context(const char *name, json_t *plugin);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_PLUGIN_A94384_H */
