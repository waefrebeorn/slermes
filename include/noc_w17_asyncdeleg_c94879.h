#ifndef NOC_W17_ASYNCDELEG_C94879_H
#define NOC_W17_ASYNCDELEG_C94879_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/async_delegation_worker.py.
 * Pure payload construction of WorkerDelegationLedger's routing methods.
 * Each function returns {"op": <durable op>, "payload": <payload>}, i.e. the
 * value the live ledger returns from the store's _apply call. */

json_t *w17_asyncdeleg_c94879_complete(const json_t *event, const json_t *result, const json_t *delivery);
json_t *w17_asyncdeleg_c94879_child(const char *delegation_id, const json_t *entry);
json_t *w17_asyncdeleg_c94879_read(const char *delegation_id);
json_t *w17_asyncdeleg_c94879_claim(const char *delegation_id, const char *claim_id);
json_t *w17_asyncdeleg_c94879_ack(const char *delegation_id, const char *claim_id);
json_t *w17_asyncdeleg_c94879_defer(const char *delegation_id, const char *claim_id);
json_t *w17_asyncdeleg_c94879_release(const char *delegation_id, const char *claim_id);
json_t *w17_asyncdeleg_c94879_drop(const char *delegation_id, const char *claim_id);
json_t *w17_asyncdeleg_c94879_unscheduled(const char *delegation_id);

#endif /* NOC_W17_ASYNCDELEG_C94879_H */
