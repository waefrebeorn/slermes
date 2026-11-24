/*
 * detached_results.c - C11 port of upstream
 * persistence/sqlite/conversation/_store/detached_results.py (gate w25_detachedre_4e5201).
 *
 * PoP: w25_detachedre_4e5201_delegation_delivery @ persistence/sqlite/conversation/_store/detached_results.py:append_delegation_delivery
 * PoP: w25_detachedre_4e5201_completion_note @ persistence/sqlite/conversation/_store/detached_results.py:append_completion_note
 * PoP: w25_detachedre_4e5201_constants @ persistence/sqlite/conversation/_store/detached_results.py:<match SQL constants>
 *
 * The writer transaction (``holder._execute_write``), the message row builder
 * and the uid minting are seams. What is ported is the decision that makes the
 * append idempotent: which display_kind the delivery becomes, which lineage
 * match SQL and parameters it is looked up by, and what metadata the completion
 * note carries.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_detachedre_4e5201.h"

#define DR_LINEAGE_MATCH_SQL        W25_DETACHEDRE_4E5201_LINEAGE_MATCH_SQL
#define DR_DELEGATION_MATCH_SQL     W25_DETACHEDRE_4E5201_DELEGATION_MATCH_SQL
#define DR_COMPLETION_MATCH_SQL     W25_DETACHEDRE_4E5201_COMPLETION_MATCH_SQL
#define DR_MISSING_DELEGATION_ID    "Delegation delivery requires a stable delegation_id"

/* Python truthiness of a JSON value. */
static int dr_truthy(const json_t *v) {
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

/* metadata.get(key, default): an explicit null is the value, not the default. */
static json_t *dr_get(const json_t *meta, const char *key, json_t *dflt) {
    json_t *v = meta && meta->type == JSON_OBJECT ? json_obj_get(meta, key) : NULL;
    return v ? json_copy(v) : dflt;
}

static json_t *dr_error(const char *kind, const char *message) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "error", json_string(kind));
    json_set(o, "message", json_string(message));
    return o;
}

json_t *w25_detachedre_4e5201_delegation_delivery(const json_t *req) {
    const json_t *meta = req ? json_obj_get(req, "metadata") : NULL;
    const json_t *delegation_id = meta && meta->type == JSON_OBJECT
        ? json_obj_get(meta, "delegation_id") : NULL;
    json_t *o, *params;
    int suppressed;

    if (!dr_truthy(delegation_id)) return dr_error("ValueError", DR_MISSING_DELEGATION_ID);
    suppressed = dr_truthy(meta ? json_obj_get(meta, "presentation_suppressed") : NULL);

    o = json_object();
    json_set(o, "ok", json_bool(1));
    json_set(o, "kind", json_string(suppressed ? "hidden" : "async_delegation_complete"));
    json_set(o, "match_sql", json_string(DR_DELEGATION_MATCH_SQL));
    params = json_array();
    json_append(params, json_copy(delegation_id));
    json_append(params, dr_get(meta, "delivery_notice", json_string("")));
    json_set(o, "params", params);
    json_set(o, "metadata", meta ? json_copy(meta) : json_object());
    return o;
}

json_t *w25_detachedre_4e5201_completion_note(const json_t *req) {
    const json_t *meta = req ? json_obj_get(req, "metadata") : NULL;
    const json_t *admission = req ? json_obj_get(req, "admission_id") : NULL;
    json_t *o = json_object(), *params = json_array(), *merged;

    merged = meta && meta->type == JSON_OBJECT ? json_copy(meta) : json_object();
    json_set(merged, "completion_admission", admission ? json_copy(admission) : json_null());

    json_set(o, "ok", json_bool(1));
    {
        const json_t *dk = req ? json_obj_get(req, "display_kind") : NULL;
        json_set(o, "kind", dk ? json_copy(dk) : json_string(""));
    }
    json_set(o, "match_sql", json_string(DR_COMPLETION_MATCH_SQL));
    json_append(params, admission ? json_copy(admission) : json_null());
    json_set(o, "params", params);
    json_set(o, "metadata", merged);
    return o;
}

json_t *w25_detachedre_4e5201_constants(void) {
    json_t *o = json_object();
    json_set(o, "lineage_match_sql", json_string(DR_LINEAGE_MATCH_SQL));
    json_set(o, "delegation_delivery_match", json_string(DR_DELEGATION_MATCH_SQL));
    json_set(o, "completion_admission_match", json_string(DR_COMPLETION_MATCH_SQL));
    json_set(o, "missing_delegation_id_message", json_string(DR_MISSING_DELEGATION_ID));
    return o;
}
