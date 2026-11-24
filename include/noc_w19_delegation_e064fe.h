#ifndef NOC_W19_DELEGATION_E064FE_H
#define NOC_W19_DELEGATION_E064FE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_delegation_e064fe_delegation_payload @ persistence/workers/delegations.py:delegation_payload
 * The operation payload a verb argument stands for. `kind` names the Python argument
 * shape the JSON cannot carry:
 *   "payload"    -> WorkerDelegationPayload(payload=value): the payload as sent
 *   "completion" -> WorkerDelegationCompletion: {"event","result"} while delivery
 *                   is "pending" (the worker omits the key), else its three fields
 *   "dispatch"   -> WorkerDelegationDispatch fields key for key
 *   "child"      -> WorkerDelegationChild fields key for key
 *   "claim"      -> WorkerDelegationClaim fields key for key
 *   "record"     -> any other dataclass: its fields key for key
 *   anything else -> a bare `delegation_id`: {"delegation_id": value} */
struct json_t *w19_delegation_e064fe_delegation_payload(const char *kind, struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_DELEGATION_E064FE_H */
