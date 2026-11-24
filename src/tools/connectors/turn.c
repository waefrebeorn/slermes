/*
 * turn.c - C11 port of upstream tools/connectors/turn.py
 * (gate w24_turn_6ea05a).
 *
 * PoP: w24_turn_6ea05a_agent_surface @ tools/connectors/turn.py:agent_connection_surface
 * PoP: w24_turn_6ea05a_tool_drops @ tools/connectors/turn.py:side_agent_tool_drops
 * PoP: w24_turn_6ea05a_surface_scope @ tools/connectors/turn.py:scoped_connection_surface
 * PoP: w24_turn_6ea05a_constants @ tools/connectors/turn.py:<module constants>
 *
 * Upstream keeps the current surface in a ContextVar and reads attributes off
 * live agent objects.  Both are seams: here an agent is the value pair
 * (connection_callback, side_agent) and the surface is a plain value threaded
 * through the walk, so a scoped surface is decided without touching a context
 * or a live object.  What is ported is the decision itself -- the cardinality
 * order (card beats side beats link), the drop set, and the set/restore
 * semantics of a scope.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_turn_6ea05a.h"

/* Python truthiness of a JSON value; NULL is an absent attribute (None). */
static int tn_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w24_turn_6ea05a_agent_surface(const json_t *agent) {
    /* getattr(agent, "connection_callback", None) is not None  ->  card.
     * A present-but-falsy callback (0, "", []) is still not None: card wins. */
    const json_t *cb = json_obj_get(agent, "connection_callback");
    if (cb && cb->type != JSON_NULL) return json_string(W24_TURN_6EA05A_CARD);
    if (tn_truthy(json_obj_get(agent, "side_agent"))) return json_string(W24_TURN_6EA05A_SIDE);
    return json_string(W24_TURN_6EA05A_LINK);
}

json_t *w24_turn_6ea05a_tool_drops(const json_t *agent) {
    json_t *out = json_array();
    if (tn_truthy(json_obj_get(agent, "side_agent")))
        json_append(out, json_string("manage_connections"));
    return out;
}

/* One walk of the op list.  ``cur`` is the surface value in scope (borrowed);
 * NULL means the var holds None.  A scope sets the var to its "surface" value
 * and the caller's value is what remains after the block, exactly like the
 * token set/reset pair upstream. */
static json_t *tn_walk(const json_t *ops, const json_t *cur) {
    json_t *out = json_array();
    size_t i, n;

    if (!ops || ops->type != JSON_ARRAY) return out;
    n = json_len(ops);
    for (i = 0; i < n; i++) {
        const json_t *op = json_get(ops, i);
        const char *kind = op && op->type == JSON_OBJECT ? json_get_str(op, "op", "") : "";
        if (!strcmp(kind, "get")) {
            /* connection_surface() reads the var: None when a scope set None. */
            json_append(out, cur ? json_copy(cur) : json_null());
        } else if (!strcmp(kind, "scope")) {
            json_t *row = json_object();
            json_set(row, "inside", tn_walk(json_obj_get(op, "ops"), json_obj_get(op, "surface")));
            /* the scope restores the value it found on entry */
            json_set(row, "after", cur ? json_copy(cur) : json_null());
            json_append(out, row);
        } else {
            json_append(out, json_string("UNKNOWN_OP"));
        }
    }
    return out;
}

json_t *w24_turn_6ea05a_surface_scope(const json_t *ops) {
    json_t *def = json_string(W24_TURN_6EA05A_LINK);
    json_t *out = tn_walk(ops, def);
    json_free(def);
    return out;
}

json_t *w24_turn_6ea05a_constants(void) {
    json_t *o = json_object();
    json_t *drops = json_array();
    json_set(o, "card", json_string(W24_TURN_6EA05A_CARD));
    json_set(o, "side", json_string(W24_TURN_6EA05A_SIDE));
    json_set(o, "link", json_string(W24_TURN_6EA05A_LINK));
    json_set(o, "default_surface", json_string(W24_TURN_6EA05A_LINK));
    json_append(drops, json_string("manage_connections"));
    json_set(o, "side_agent_tool_drops", drops);
    return o;
}
