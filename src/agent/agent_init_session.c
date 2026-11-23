/*
 * agent_init_session.c — faithful C11 port of agent/agent_init_session.py.
 * Gate w10_agentinit_2749c4.
 *
 * PoP: w10_agentinit_2749c4_writer_interface @ agent/agent_init_session.py:writer_interface
 * PoP: w10_agentinit_2749c4_agent_interface @ agent/agent_init_session.py:agent_interface
 * PoP: w10_agentinit_2749c4_stand_in_interface @ agent/agent_init_session.py:stand_in_interface
 * PoP: w10_agentinit_2749c4_publish_session_id @ agent/agent_init_session.py:_publish_session_id
 */
#include "hermes_json.h"
#include "noc_w10_agentinit_2749c4.h"
#include <string.h>

static int is_none(const json_t *v) { return !v || v->type == JSON_NULL; }

/* Python truthiness for JSON values. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

json_t *w10_agentinit_2749c4_writer_interface(const json_t *persistence, const json_t *store) {
    /* persistence if store is not None and persistence is not None and not persistence.read_only */
    if (is_none(store) || is_none(persistence)) return NULL;
    if (py_truthy(json_obj_get(persistence, "read_only"))) return NULL;
    return json_copy(persistence);
}

json_t *w10_agentinit_2749c4_agent_interface(const json_t *agent) {
    if (is_none(agent)) return NULL;
    return w10_agentinit_2749c4_writer_interface(json_obj_get(agent, "persistence"),
                                                  json_obj_get(agent, "_session_db"));
}

json_t *w10_agentinit_2749c4_stand_in_interface(const json_t *session_db,
        int registered, int is_session_db) {
    if (!registered || !is_session_db || is_none(session_db)) return NULL;
    if (py_truthy(json_obj_get(session_db, "read_only"))) return NULL;
    json_t *st = json_obj_get(session_db, "_store");
    return st ? json_copy(st) : NULL;
}

const char *w10_agentinit_2749c4_publish_session_id(const char *session_id,
        int bridge_ok, int delegated_child) {
    (void)session_id;
    if (bridge_ok) return "contextvar";
    if (delegated_child) return "skip";
    return "env";
}
