/*
 * noc_w21_methodstoo_16216e.h -- per-server rows and tool ownership of
 * installed portable plugins (port of
 * tui_gateway/methods_tools_plugin_servers.py).
 *
 * PoP: w21_methodstoo_16216e_server_identity @ tui_gateway/methods_tools_plugin_servers.py:_server_identity
 * PoP: w21_methodstoo_16216e_portable_server_tools @ tui_gateway/methods_tools_plugin_servers.py:_portable_server_tools
 * PoP: w21_methodstoo_16216e_plugin_tool_ownership @ tui_gateway/methods_tools_plugin_servers.py:_plugin_tool_ownership
 *
 * The module resolves its collaborators lazily out of server.py globals
 * (``_tools_mod(...)``): the plugin loader, the MCP client's ``_core``
 * ledgers, the toolsets table and config.yaml. Those are config/network seams,
 * so the port takes them as one injected state object. The set algebra, the
 * connect-time ``${VAR}`` identity comparison and the name-collision rule are
 * the module's own logic and are what the gate pins.
 */
#ifndef NOC_W21_METHODSTOO_16216E_H
#define NOC_W21_METHODSTOO_16216E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _server_identity(cfg): [command, [args...], url] after ${VAR} expansion.
 * A non-dict cfg reads as {}, exactly like `cfg if isinstance(cfg, dict)`.
 * *env* is the secret-scope view `_interpolate_env_vars` resolves against:
 * {"VAR": "value"}; an unset/empty reference keeps its literal ${...}. */
json_t *w21_methodstoo_16216e_server_identity(const json_t *cfg, const json_t *env);

/* _portable_server_tools(manager, native) with the ledger injection:
 * state = {"portable_servers": {server: owner}, "declared": {server: cfg},
 *          "native": {server: cfg} | any, "registered": {server: [tool]},
 *          "lazy": {server: [tool]}, "env": {VAR: value}}
 * -> {owner: [tool...]} (each list sorted, owners in first-seen order). */
json_t *w21_methodstoo_16216e_portable_server_tools(const json_t *state);

/* _plugin_tool_ownership() with the same injection plus
 * {"plugins": {key: [tool]}, "core_tools": [tool], "toolsets": {name: {"tools": [tool]}}}
 * -> {owner: [tool...]} (sorted); a tool two owners list belongs to neither. */
json_t *w21_methodstoo_16216e_plugin_tool_ownership(const json_t *state);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W21_METHODSTOO_16216E_H */
