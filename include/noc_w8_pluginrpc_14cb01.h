#ifndef NOC_W8_PLUGINRPC_14CB01_H
#define NOC_W8_PLUGINRPC_14CB01_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_pluginrpc_14cb01_plugin_rpc_handler @ hermes_cli/plugin_rpc.py:plugin_rpc_handler
 * `table` is the injected _plugin_rpc_methods mapping. Returns a copy of the
 * mapped JSON value, or JSON null for non-str/unprefixed/unknown methods.
 * PLUGIN_EVENT_PREFIX ("plugin.") is a literal from plugin_events.py. */
struct json_t *w8_pluginrpc_14cb01_plugin_rpc_handler(const struct json_t *method,
                                                     const struct json_t *table);

/* PoP: w8_pluginrpc_14cb01_register_rpc @ hermes_cli/plugin_rpc.py:register_rpc
 * `callable_handler` is the injected callable() result. With the injected
 * _register_entry seam, a valid registration returns
 * {"bucket":"rpc_method","method":"plugin.<id>.<name>"}; an invalid handler or
 * name returns {"error":<message>} (the Python exception text). */
struct json_t *w8_pluginrpc_14cb01_register_rpc(const char *plugin_id, const char *name,
                                               int callable_handler);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_PLUGINRPC_14CB01_H */
