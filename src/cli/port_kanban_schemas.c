/*
 * port_kanban_schemas.c — Port of tools/kanban_tools_schemas.py::_prop: the tiny
 * JSON-schema property builder. Returns {"type": type, "description": desc}.
 */

#include "port_kanban_schemas.h"

#include <stdlib.h>
#include <string.h>

/* Deep copy a json node via serialize/reparse (libjson has no deep-copy API). */
static json_t *kts_clone(const json_t *n)
{
    char *ser = json_serialize(n);
    json_t *copy;
    if (!ser) return NULL;
    copy = json_parse(ser, NULL);
    free(ser);
    return copy;
}

/* PoP: kts_prop @ tools/kanban_tools_schemas.py:_prop */
json_t *kts_prop(const char *type_, const char *description)
{
    json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "type", json_string(type_ ? type_ : ""));
    json_set(obj, "description", json_string(description ? description : ""));
    return obj;
}

#define KTS_DESC_BOARD \
    "Kanban board slug to target. When omitted, the call resolves the " \
    "active board the usual way: HERMES_KANBAN_DB env \xe2\x86\x92 " \
    "HERMES_KANBAN_BOARD env \xe2\x86\x92 the 'current' symlink under the kanban " \
    "home \xe2\x86\x92 'default'. Pass an explicit slug only when the caller (e.g. " \
    "a Telegram routing layer) needs to override the env-pinned active " \
    "board for this one call."

/* PoP: kts_schema @ tools/kanban_tools_schemas.py:_schema */
json_t *kts_schema(const char *name, const char *description,
                   const char *properties_json, const char *required_json)
{
    json_t *schema = json_object();
    json_t *params = json_object();
    json_t *props = properties_json ? json_parse(properties_json, NULL) : json_object();
    json_t *board = kts_prop("string", KTS_DESC_BOARD);
    json_t *required = required_json ? json_parse(required_json, NULL) : json_array();
    if (!schema || !params || !board) { json_free(schema); json_free(params); json_free(board); return NULL; }
    json_set(params, "type", json_string("object"));
    json_set(params, "properties", json_object());
    /* merge caller properties into params.properties, then add board */
    json_t *pp = json_obj_get(params, "properties");
    if (props && props->type == JSON_OBJECT) {
        size_t i;
        for (i = 0; i < props->c.count; i++) {
            if (props->c.keys && props->c.items)
                json_set(pp, props->c.keys[i], kts_clone(props->c.items[i]));
        }
    }
    json_set(pp, "board", board);
    json_set(params, "required", kts_clone(required));
    json_set(schema, "name", json_string(name ? name : ""));
    json_set(schema, "description", json_string(description ? description : ""));
    json_set(schema, "parameters", params);
    json_free(props);
    json_free(required);
    return schema;
}
