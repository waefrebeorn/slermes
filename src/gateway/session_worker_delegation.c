/*
 * session_worker_delegation.c -- C11 port of the closed delegation-ledger
 * decisions of upstream Hermes gateway/session_worker_delegation.py.
 *
 * PoP: w22_sessionwor_930a87_plan @ gateway/session_worker_delegation.py:persist_worker_delegation
 * PoP: w22_sessionwor_930a87_probe @ gateway/session_worker_delegation.py:_probe
 * PoP: w22_sessionwor_930a87_delivery_verb @ gateway/session_worker_delegation.py:_DELIVERY
 * PoP: w22_sessionwor_930a87_request_verdict @ gateway/session_worker_delegation.py:worker_delegation_request
 *
 * The storage handle (`store.worker_executions`), the `WorkerDelegationPayload`
 * wrapper, psutil/get_process_start_time, `_claim`/`_verify`/
 * `authority_persistence` and the completion-queue publication are live seams
 * and stay upstream; the C answers which verb a closed operation runs, on which
 * kwargs, and which refusal a request earns. Pure C11, ASCII, no VLA.
 */
#include "hermes_json.h"
#include "noc_w22_sessionwor_930a87.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *name;        /* operation.removeprefix('delegation.') */
    const char *verb;        /* the store method it runs */
    const char *payload_arg; /* the kwarg carrying WorkerDelegationPayload */
    int returns_value;       /* the store's answer is passed through, not {'value': None} */
    int needs_probe;         /* dispatch probes the worker's process first */
} swd930a87_ledger_row;

static const swd930a87_ledger_row SWD930A87_LEDGER[] = {
    {"dispatch",    "dispatch_delegation",         "dispatch",      0, 1},
    {"complete",    "complete_delegation",         "completion",    0, 0},
    {"child",       "record_delegation_child",     "child",         0, 0},
    {"read",        "read_delegation",             "delegation_id", 1, 0},
    {"unscheduled", "withdraw_delegation",         "delegation_id", 0, 0},
    {"claim",       "claim_delegation_delivery",   "claim",         1, 0},
    {"ack",         "ack_delegation_delivery",     "claim",         1, 0},
    {"drop",        "drop_delegation_delivery",    "claim",         1, 0},
    {"defer",       "defer_delegation_delivery",   "claim",         1, 0},
    {"release",     "release_delegation_delivery", "claim",         1, 0},
};

#define SWD930A87_ROWS (sizeof(SWD930A87_LEDGER) / sizeof(SWD930A87_LEDGER[0]))
#define SWD930A87_RECEIPT_N 7
#define SWD930A87_SCOPE_N 11

/* Receipt fields, in the order `dict(epoch=..., execution_id=..., ...)` builds. */
static const char *const SWD930A87_RECEIPT[SWD930A87_RECEIPT_N] = {
    "epoch", "execution_id", "session_id", "generation", "sequence",
    "worker_pid", "worker_birth",
};

/* `_SCOPE | {'epoch', 'sequence', 'operation', 'payload'}` (gateway.session_worker._SCOPE). */
static const char *const SWD930A87_SCOPE[SWD930A87_SCOPE_N] = {
    "birth", "execution_id", "generation", "pid", "profile_id", "secret",
    "session_id", "epoch", "sequence", "operation", "payload",
};

static const swd930a87_ledger_row *swd930a87_row_for(const char *operation) {
    const char *name;
    size_t i;
    if (!operation) return NULL;
    if (strncmp(operation, "delegation.", 11) != 0) return NULL;
    name = operation + 11;
    for (i = 0; i < SWD930A87_ROWS; i++) {
        if (strcmp(SWD930A87_LEDGER[i].name, name) == 0) return &SWD930A87_LEDGER[i];
    }
    return NULL;
}

const char *w22_sessionwor_930a87_delivery_verb(const char *name) {
    size_t i;
    if (!name) return NULL;
    for (i = 0; i < SWD930A87_ROWS; i++) {
        if (strcmp(SWD930A87_LEDGER[i].name, name) == 0 &&
            strstr(SWD930A87_LEDGER[i].verb, "_delegation_delivery") != NULL) {
            return SWD930A87_LEDGER[i].verb;
        }
    }
    return NULL;
}

json_t *w22_sessionwor_930a87_probe(const json_t *probe) {
    json_t *out = json_object();
    const char *started_at = NULL;
    int alive = 1, birth_equal = 1;
    if (!out) return NULL;
    if (probe && probe->type == JSON_OBJECT) {
        json_t *a = json_obj_get(probe, "alive");
        json_t *b = json_obj_get(probe, "birth_equal");
        if (a && a->type == JSON_BOOL) alive = a->bool_val ? 1 : 0;
        if (b && b->type == JSON_BOOL) birth_equal = b->bool_val ? 1 : 0;
        started_at = json_get_str(probe, "started_at", NULL);
    }
    if (!alive) {
        json_set(out, "owner_started_at", json_null());
        json_set(out, "refusal", json_string("worker_not_live"));
    } else if (!birth_equal) {
        json_set(out, "owner_started_at", json_null());
        json_set(out, "refusal", json_string("permission_denied"));
    } else {
        json_set(out, "owner_started_at", started_at ? json_string(started_at) : json_null());
        json_set(out, "refusal", json_null());
    }
    return out;
}

json_t *w22_sessionwor_930a87_plan(const char *operation, const json_t *receipt,
                                   const json_t *probe) {
    const swd930a87_ledger_row *row;
    json_t *out, *rcpt, *extra;
    size_t i;
    if (!receipt || receipt->type != JSON_OBJECT) return NULL;
    for (i = 0; i < SWD930A87_RECEIPT_N; i++) {
        /* an absent receipt field is the upstream TypeError */
        if (json_obj_get(receipt, SWD930A87_RECEIPT[i]) == NULL) return NULL;
    }
    row = swd930a87_row_for(operation);
    out = json_object();
    if (!out) return NULL;
    if (!row) {
        json_set(out, "ok", json_bool(0));
        json_set(out, "reason", json_string("invalid_params"));
        return out;
    }
    rcpt = json_object();
    if (!rcpt) { json_free(out); return NULL; }
    for (i = 0; i < SWD930A87_RECEIPT_N; i++) {
        json_set(rcpt, SWD930A87_RECEIPT[i],
                 json_copy(json_obj_get(receipt, SWD930A87_RECEIPT[i])));
    }
    extra = json_object();
    if (!extra) { json_free(rcpt); json_free(out); return NULL; }
    if (row->needs_probe) {
        json_t *p = w22_sessionwor_930a87_probe(probe);
        if (p) {
            json_set(extra, "owner_started_at", json_copy(json_obj_get(p, "owner_started_at")));
            json_set(extra, "probe_refused", json_copy(json_obj_get(p, "refusal")));
            json_free(p);
        }
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "verb", json_string(row->verb));
    json_set(out, "payload_arg", json_string(row->payload_arg));
    json_set(out, "returns_value", json_bool(row->returns_value));
    json_set(out, "receipt", rcpt);
    json_set(out, "extra", extra);
    return out;
}

const char *w22_sessionwor_930a87_request_verdict(const json_t *params_keys, int has_adopt) {
    size_t i, j, seen = 0;
    int hit[SWD930A87_SCOPE_N];
    if (!params_keys || params_keys->type != JSON_ARRAY) return "invalid_params";
    for (i = 0; i < SWD930A87_SCOPE_N; i++) hit[i] = 0;
    for (i = 0; i < params_keys->c.count; i++) {
        json_t *k = params_keys->c.items[i];
        int matched = 0;
        if (!k || k->type != JSON_STRING) return "invalid_params";
        for (j = 0; j < SWD930A87_SCOPE_N; j++) {
            if (!strcmp(k->str_val ? k->str_val : "", SWD930A87_SCOPE[j])) {
                matched = 1;
                if (!hit[j]) { hit[j] = 1; seen++; }
                break;
            }
        }
        if (!matched) return "invalid_params";
    }
    if (seen != SWD930A87_SCOPE_N) return "invalid_params";
    if (!has_adopt) return "permission_denied";
    return "ok";
}
