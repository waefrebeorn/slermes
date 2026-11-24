#ifndef NOC_W16_PLUGINSINJ_E56FEA_H
#define NOC_W16_PLUGINSINJ_E56FEA_H

#include "hermes_json.h"

/* C port of hermes_cli/plugins_injection.py::inject_plugin_message.
 *
 * The Python function dispatches through collaborator objects; the C port takes
 * their observable state explicitly. It returns a JSON object
 *   {"ok": bool, "route": str, "msg": str, "target": object|null}
 * where `route` names the path actually taken and `target` is the gateway
 * injection target (null on every non-gateway path). Caller owns the result
 * (json_free).
 *
 * origin_kind is derived from the origin node itself:
 *   NULL -> absent (None), JSON_OBJECT -> a Mapping, anything else -> invalid.
 */
json_t *w16_pluginsinj_e56fea_inject(
    const char *content,
    const char *role,
    int has_cli,
    int agent_running,
    const char *session_key,
    json_t *origin,
    int has_gw_injector,
    int has_tui_injector,
    int injection_allowed,
    int tui_result,
    int tui_raises,
    int gw_result,
    int gw_raises,
    const char *calling_session_key,
    const char *plugin_home);

#endif
