/*
 * runtime_scope.c — C port of upstream Hermes agent/runtime_scope.py.
 *
 * PoP: w6_runtimescop_c06314_claim_fence @ agent/runtime_scope.py:claim_fence
 * PoP: w6_runtimescop_c06314_identity @ agent/runtime_scope.py:_identity
 *
 * Only the deterministic, side-effect-free helpers are ported. The scope
 * binding/context-manager machinery depends on process-global hermes_scope
 * state and is not exercised by this differential gate.
 */
#include "noc_w6_runtimescop_c06314.h"
#include "hermes_json.h"

/* Fence requires type(x) is int and x > 0 for epoch and generation. */
static int positive_int(const json_t *v) {
    if (!v || v->type != JSON_NUMBER) return 0;
    if (v->num_val <= 0.0) return 0;
    if (v->num_val != (double)(long long)v->num_val) return 0;
    return 1;
}

json_t *w6_runtimescop_c06314_claim_fence(const json_t *row) {
    if (!row || row->type != JSON_OBJECT) return json_null();
    json_t *e = json_obj_get(row, "owner_epoch");
    json_t *g = json_obj_get(row, "generation");
    if (!positive_int(e) || !positive_int(g)) return json_null();
    json_t *out = json_object();
    json_set(out, "epoch", json_copy(e));
    json_set(out, "generation", json_copy(g));
    return out;
}

json_t *w6_runtimescop_c06314_identity(const json_t *scope) {
    json_t *out = json_array();
    json_append(out, json_string(json_get_str(scope, "tenant", "")));
    json_append(out, json_string(json_get_str(scope, "agent", "")));
    json_append(out, json_string(json_get_str(scope, "actor", "")));
    json_append(out, json_string(json_get_str(scope, "owner", "")));
    json_append(out, json_bool(json_get_bool(scope, "can_administer", false)));
    return out;
}

