/*
 * lineage_adoption.c — real C11 port of the pure dict leaves of
 * lineage_adoption.py.
 *
 * PoP: lineageadop_result_dict  @ lineage_adoption.py:_result_dict
 * PoP: lineageadop_import_entry @ lineage_adoption.py:_import_entry
 *
 * The store/donor orchestration in adopt_session_lineage is driven by live
 * session-store objects and is not a JSON leaf. These two functions are the
 * deterministic result/entry shapers used by that orchestration.
 */
#include "noc_w5_lineageadop_be48a0.h"
#include <stdlib.h>
#include <string.h>

static json_t *la_get(json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return v ? json_copy(v) : json_null();
}

/*
 * PoP: lineageadop_result_dict @ lineage_adoption.py:_result_dict
 */
json_t *lineageadop_result_dict(json_t *result) {
    json_t *out = json_object();
    json_t *ok = json_obj_get(result, "ok");
    int is_ok = ok && ok->type == JSON_BOOL && ok->bool_val;
    json_set(out, "ok", la_get(result, "ok"));
    json_set(out, "imported", la_get(result, "imported"));
    json_set(out, "skipped", la_get(result, "skipped"));
    json_set(out, "detached", la_get(result, "detached"));
    if (is_ok) {
        json_set(out, "imported_ids", la_get(result, "imported_ids"));
        json_set(out, "skipped_ids", la_get(result, "skipped_ids"));
    }
    json_set(out, "errors", la_get(result, "errors"));
    return out;
}

/*
 * PoP: lineageadop_import_entry @ lineage_adoption.py:_import_entry
 */
json_t *lineageadop_import_entry(json_t *segment) {
    json_t *session = json_obj_get(segment, "session");
    json_t *messages = json_obj_get(segment, "messages");
    json_t *out = json_object();
    if (session && session->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(session); i++) {
            const char *k = json_object_get_key_at(session, i);
            if (!k) continue;
            json_set(out, k, json_copy(json_object_get_at(session, i)));
        }
    }
    /* {**session, "messages": ...}: the computed list always overrides any
     * "messages" key that came from session.as_dict(). */
    json_set(out, "messages", messages ? json_copy(messages) : json_array());
    return out;
}
