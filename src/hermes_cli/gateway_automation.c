/*
 * gateway_automation.c — faithful C11 port of hermes_cli/gateway_automation.py
 * (gate w17_gatewayaut_143263).
 *
 * PoP: w17_gatewayaut_143263_decode @ hermes_cli/gateway_automation.py:_decode
 * PoP: w17_gatewayaut_143263_gateway_connection @ hermes_cli/gateway_automation.py:gateway_connection
 *
 * The persisted-state envelope markers (__state__ / __gate__) are unwrapped
 * recursively; GoalGate.from_dict is injected by the oracle as a deterministic
 * {"__from_gate__": payload} seam so the branch is differentially testable.
 */
#include "noc_w17_gatewayaut_143263.h"

#include <string.h>

json_t *w17_gatewayaut_143263_decode(const json_t *value) {
    json_t *out;
    size_t i, n;
    if (!value) return json_null();
    if (value->type == JSON_OBJECT) {
        n = json_object_size(value);
        if (n == 1) {
            const char *key = json_object_get_key_at(value, 0);
            const json_t *child = json_object_get_at(value, 0);
            if (key && !strcmp(key, "__state__")) return child ? json_copy(child) : json_null();
            if (key && !strcmp(key, "__gate__")) {
                out = json_object();
                json_set(out, "__from_gate__", child ? json_copy(child) : json_null());
                return out;
            }
        }
        out = json_object();
        for (i = 0; i < n; ++i) {
            const char *k = json_object_get_key_at(value, i);
            if (!k) continue;
            json_set(out, k, w17_gatewayaut_143263_decode(json_object_get_at(value, i)));
        }
        return out;
    }
    if (value->type == JSON_ARRAY) {
        out = json_array();
        for (i = 0; i < value->c.count; ++i)
            json_append(out, w17_gatewayaut_143263_decode(json_get(value, i)));
        return out;
    }
    return json_copy(value);
}

json_t *w17_gatewayaut_143263_gateway_connection(int is_gateway_store, int session_open,
                                                 const char *connection) {
    if (is_gateway_store && session_open)
        return connection ? json_string(connection) : json_null();
    return json_null();
}
