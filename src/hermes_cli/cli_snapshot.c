/*
 * cli_snapshot.c — C port of upstream Hermes hermes_cli/cli_snapshot.py.
 *
 * PoP: w9_clisnapshot_337c7c_rejoin_history @ hermes_cli/cli_snapshot.py:_rejoin_cli
 *
 * Only the deterministic conversation-history rebuild is ported; gateway
 * handoff / owner pause / store reconnect procedures are intentionally excluded.
 */
#include "noc_w9_clisnapshot_337c7c.h"
#include "hermes_json.h"
#include <string.h>

/* Python truthiness for the JSON values fixtures can carry. */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}

struct json_t *w9_clisnapshot_337c7c_rejoin_history(const struct json_t *messages) {
    json_t *out = json_array();
    if (!messages || messages->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < messages->c.count; i++) {
        json_t *msg = json_get(messages, i);
        if (!msg || msg->type != JSON_OBJECT) continue;
        json_t *archived = json_obj_get(msg, "archived");
        if (py_truthy(archived)) continue;              /* if not message.get("archived") */
        json_t *kept = json_object();
        for (size_t k = 0; k < json_object_size(msg); k++) {
            const char *key = json_object_get_key_at(msg, k);
            if (!key || !strcmp(key, "row_id")) continue; /* {k:v ... if k != "row_id"} */
            json_t *val = json_object_get_at(msg, k);
            json_set(kept, key, json_copy(val));
        }
        json_append(out, kept);
    }
    return out;
}
