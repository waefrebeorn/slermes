/*
 * kanban_tools_schemas.c — C port of upstream Hermes tools/kanban_tools_schemas.py.
 *
 * PoP: w14_kanbantool_ff0170_prop @ tools/kanban_tools_schemas.py:_prop
 * PoP: w14_kanbantool_ff0170_board_schema_prop @ tools/kanban_tools_schemas.py:_board_schema_prop
 * PoP: w14_kanbantool_ff0170_schema @ tools/kanban_tools_schemas.py:_schema
 *
 * _DESC_BOARD is byte-frozen upstream text (the arrow is UTF-8 e2 86 92).
 */
#include "noc_w14_kanbantool_ff0170.h"
#include "hermes_json.h"

#include <stdlib.h>

#define W14KTS_DESC_BOARD \
    "Kanban board slug to target. When omitted, the call resolves the " \
    "active board the usual way: HERMES_KANBAN_DB env \xe2\x86\x92 " \
    "HERMES_KANBAN_BOARD env \xe2\x86\x92 the 'current' symlink under the kanban " \
    "home \xe2\x86\x92 'default'. Pass an explicit slug only when the caller (e.g. " \
    "a Telegram routing layer) needs to override the env-pinned active " \
    "board for this one call."

static struct json_t *clone_or_null(const struct json_t *v) {
    return v ? json_copy(v) : json_null();
}

struct json_t *w14_kanbantool_ff0170_prop(const struct json_t *type_,
                                          const struct json_t *description) {
    struct json_t *obj = json_object();
    if (!obj) return NULL;
    json_set(obj, "type", clone_or_null(type_));
    json_set(obj, "description", clone_or_null(description));
    return obj;
}

struct json_t *w14_kanbantool_ff0170_board_schema_prop(void) {
    struct json_t *t = json_string("string");
    struct json_t *d = json_string(W14KTS_DESC_BOARD);
    struct json_t *out = w14_kanbantool_ff0170_prop(t, d);
    json_free(t);
    json_free(d);
    return out;
}

struct json_t *w14_kanbantool_ff0170_schema(const struct json_t *name,
                                            const struct json_t *description,
                                            const struct json_t *properties,
                                            const struct json_t *required) {
    struct json_t *schema = json_object();
    struct json_t *params = json_object();
    struct json_t *props = json_object();
    struct json_t *board;
    if (!schema || !params || !props) {
        json_free(schema); json_free(params); json_free(props);
        return NULL;
    }

    if (json_is_object(properties)) {
        for (size_t i = 0; i < json_object_size(properties); i++) {
            const char *k = json_object_get_key_at(properties, i);
            struct json_t *v = json_object_get_at(properties, i);
            if (k) json_set(props, k, v ? json_copy(v) : json_null());
        }
    }
    board = w14_kanbantool_ff0170_board_schema_prop();
    json_set(props, "board", board);   /* {**properties, "board": ...} overrides */

    json_set(params, "type", json_string("object"));
    json_set(params, "properties", props);
    json_set(params, "required", clone_or_null(required));

    json_set(schema, "name", clone_or_null(name));
    json_set(schema, "description", clone_or_null(description));
    json_set(schema, "parameters", params);
    return schema;
}
