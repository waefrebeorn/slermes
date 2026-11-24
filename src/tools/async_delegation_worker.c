/*
 * async_delegation_worker.c — C port of upstream Hermes tools/async_delegation_worker.py.
 *
 * PoP: w17_asyncdeleg_c94879_complete @ tools/async_delegation_worker.py:WorkerDelegationLedger.complete
 * PoP: w17_asyncdeleg_c94879_child @ tools/async_delegation_worker.py:WorkerDelegationLedger.child
 * PoP: w17_asyncdeleg_c94879_read @ tools/async_delegation_worker.py:WorkerDelegationLedger.read
 * PoP: w17_asyncdeleg_c94879_claim @ tools/async_delegation_worker.py:WorkerDelegationLedger.claim
 * PoP: w17_asyncdeleg_c94879_ack @ tools/async_delegation_worker.py:WorkerDelegationLedger.ack
 * PoP: w17_asyncdeleg_c94879_defer @ tools/async_delegation_worker.py:WorkerDelegationLedger.defer
 * PoP: w17_asyncdeleg_c94879_release @ tools/async_delegation_worker.py:WorkerDelegationLedger.release
 * PoP: w17_asyncdeleg_c94879_drop @ tools/async_delegation_worker.py:WorkerDelegationLedger.drop
 * PoP: w17_asyncdeleg_c94879_unscheduled @ tools/async_delegation_worker.py:WorkerDelegationLedger.unscheduled
 */
#include "noc_w17_asyncdeleg_c94879.h"
#include <stdlib.h>
#include <string.h>

/* Wrap the durable op + payload exactly as the live ledger's store._apply
 * result is returned to its caller. */
static json_t *wrapped(const char *op, json_t *payload) {
    json_t *out = json_object();
    json_set(out, "op", json_string(op));
    json_set(out, "payload", payload);
    return out;
}
static json_t *copy_or_null(const json_t *v) {
    return v ? json_copy(v) : json_null();
}

json_t *w17_asyncdeleg_c94879_complete(const json_t *event, const json_t *result, const json_t *delivery) {
    json_t *payload = json_object();
    json_set(payload, "event", copy_or_null(event));
    json_set(payload, "result", copy_or_null(result));
    /* delivery != 'pending' -> include it verbatim (Python dict union) */
    if (delivery) {
        bool pending = (delivery->type == JSON_STRING && delivery->str_val
                        && !strcmp(delivery->str_val, "pending"));
        if (!pending) json_set(payload, "delivery", json_copy(delivery));
    }
    return wrapped("delegation.complete", payload);
}
json_t *w17_asyncdeleg_c94879_child(const char *delegation_id, const json_t *entry) {
    json_t *payload = json_object();
    json_set(payload, "delegation_id", json_string(delegation_id ? delegation_id : ""));
    json_set(payload, "entry", copy_or_null(entry));
    return wrapped("delegation.child", payload);
}
json_t *w17_asyncdeleg_c94879_read(const char *delegation_id) {
    json_t *payload = json_object();
    json_set(payload, "delegation_id", json_string(delegation_id ? delegation_id : ""));
    return wrapped("delegation.read", payload);
}
static json_t *claim_like(const char *op, const char *delegation_id, const char *claim_id) {
    json_t *payload = json_object();
    json_set(payload, "delegation_id", json_string(delegation_id ? delegation_id : ""));
    json_set(payload, "claim_id", json_string(claim_id ? claim_id : ""));
    return wrapped(op, payload);
}
json_t *w17_asyncdeleg_c94879_claim(const char *delegation_id, const char *claim_id) {
    return claim_like("delegation.claim", delegation_id, claim_id);
}
json_t *w17_asyncdeleg_c94879_ack(const char *delegation_id, const char *claim_id) {
    return claim_like("delegation.ack", delegation_id, claim_id);
}
json_t *w17_asyncdeleg_c94879_defer(const char *delegation_id, const char *claim_id) {
    return claim_like("delegation.defer", delegation_id, claim_id);
}
json_t *w17_asyncdeleg_c94879_release(const char *delegation_id, const char *claim_id) {
    return claim_like("delegation.release", delegation_id, claim_id);
}
json_t *w17_asyncdeleg_c94879_drop(const char *delegation_id, const char *claim_id) {
    return claim_like("delegation.drop", delegation_id, claim_id);
}
json_t *w17_asyncdeleg_c94879_unscheduled(const char *delegation_id) {
    json_t *payload = json_object();
    json_set(payload, "delegation_id", json_string(delegation_id ? delegation_id : ""));
    return wrapped("delegation.unscheduled", payload);
}
