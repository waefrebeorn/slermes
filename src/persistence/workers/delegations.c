/*
 * delegations.c - real C11 port of upstream Hermes persistence/workers/delegations.py
 *   module-level pure helper(s). The dataclass/Protocol declarations are types, not
 *   behaviour; the one function below is their whole executable surface.
 * PoP: w19_delegation_e064fe_delegation_payload @ persistence/workers/delegations.py:delegation_payload
 */
#include "hermes_json.h"
#include "noc_w19_delegation_e064fe.h"

static json_t *obj_get(json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* A dataclass field: `getattr(value, name)` - the JSON node, copied, or null when absent. */
static json_t *field_copy(json_t *o, const char *k) {
    json_t *v = obj_get(o, k);
    return v ? json_copy(v) : json_null();
}

static json_t *fields_record(json_t *v, const char *const *names) {
    json_t *out = json_object();
    for (int i = 0; names[i]; i++) json_set(out, names[i], field_copy(v, names[i]));
    return out;
}

/* Field order as declared on each frozen dataclass. */
static const char *const DISPATCH_FIELDS[] = {
    "delegation_id", "task", "dispatched_at", "session_key",
    "parent_session_id", "origin_ui_session_id", "origin_session_id", NULL
};
static const char *const CHILD_FIELDS[] = { "delegation_id", "entry", NULL };
static const char *const CLAIM_FIELDS[] = { "delegation_id", "claim_id", NULL };

json_t *w19_delegation_e064fe_delegation_payload(const char *kind, json_t *value) {
    const char *k = kind ? kind : "";
    if (!strcmp(k, "payload")) {
        /* isinstance(value, WorkerDelegationPayload) -> value.payload (any JSON, even non-mapping) */
        return value ? json_copy(value) : json_null();
    }
    if (!strcmp(k, "completion")) {
        /* WorkerDelegationCompletion(event, result, delivery="pending") */
        json_t *delivery = obj_get(value, "delivery");
        int pending = 1;
        if (delivery && delivery->type != JSON_NULL)
            pending = (delivery->type == JSON_STRING && !strcmp(delivery->str_val, "pending"));
        json_t *out = json_object();
        json_set(out, "event", field_copy(value, "event"));
        json_set(out, "result", field_copy(value, "result"));
        /* The worker sends `delivery` only for "delivered"; the default maps to the two-key payload. */
        if (!pending)
            json_set(out, "delivery", field_copy(value, "delivery"));
        return out;
    }
    if (!strcmp(k, "dispatch")) return fields_record(value, DISPATCH_FIELDS);
    if (!strcmp(k, "child")) return fields_record(value, CHILD_FIELDS);
    if (!strcmp(k, "claim")) return fields_record(value, CLAIM_FIELDS);
    if (!strcmp(k, "record")) {
        /* is_dataclass(value): a record's fields key for key; anything else is not one */
        if (value && value->type == JSON_OBJECT) return json_copy(value);
    }
    /* a bare `delegation_id` (or any argument that is not a record) */
    json_t *out = json_object();
    json_set(out, "delegation_id", value ? json_copy(value) : json_null());
    return out;
}
