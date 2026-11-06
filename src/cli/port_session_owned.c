/*
 * port_session_owned.c — session_owned from gateway/session_connections.py.
 *
 * owner = params.get('owner') if isinstance(params, dict) else None
 * return isinstance(owner, dict) and owner.get('type') == 'session'
 */

#include "port_session_owned.h"
#include "hermes_json.h"

#include <string.h>

/* PoP: session_owned @ gateway/session_connections.py:session_owned */
int sco_session_owned(const char *params_json)
{
    json_t *params = params_json ? json_parse(params_json, NULL) : NULL;
    json_t *owner, *type;
    int r = 0;
    if (!params || params->type != JSON_OBJECT) {
        if (params) json_free(params);
        return 0;
    }
    owner = json_obj_get(params, "owner");
    if (!owner || owner->type != JSON_OBJECT) {
        json_free(params);
        return 0;
    }
    type = json_obj_get(owner, "type");
    if (type && type->type == JSON_STRING && type->str_val &&
        strcmp(type->str_val, "session") == 0)
        r = 1;
    json_free(params);
    return r;
}
