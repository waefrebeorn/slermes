/*
 * port_kanban_schemas.c — Port of tools/kanban_tools_schemas.py::_prop: the tiny
 * JSON-schema property builder. Returns {"type": type, "description": desc}.
 */

#include "port_kanban_schemas.h"

#include <stdlib.h>
#include <string.h>

/* PoP: _prop @ tools/kanban_tools_schemas.py:_prop */
json_t *kts_prop(const char *type_, const char *description)
{
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "type", json_string(type_ ? type_ : ""));
    json_set(obj, "description", json_string(description ? description : ""));
    return obj;
}
