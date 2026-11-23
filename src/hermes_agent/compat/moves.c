/*
 * moves.c — real C port of upstream Hermes hermes_agent/compat/moves.py constant tables.
 * PoP: cn_9a0d4bbb84_constants @ hermes_agent/compat/moves.py:<module-constants>
 * Auto-generated from literal top-level constants; gated against live Python.
 */
#include "hermes_json.h"
#include "noc_cn_9a0d4bbb84.h"

json_t *cn_9a0d4bbb84_constants(void) {
    json_t *out = json_object();
    json_set(out, "MOVES", ({ json_t *__o = json_object();
        json_set(__o, "agent.message_content", json_string("session_rules.turns"));
        json_set(__o, "tools.plugin_guard", json_string("hermes_cli.plugins_install_scan"));
      __o; }));
    return out;
}
