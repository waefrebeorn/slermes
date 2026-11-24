#ifndef NOC_W22_SESSIONWOR_930A87_H
#define NOC_W22_SESSIONWOR_930A87_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of gateway/session_worker_delegation.py (gate w22_sessionwor_930a87).
 *
 * The module is the closed delegation-ledger sibling of `worker.persist`: one
 * ledger verb of the profile's storage handle, run on the worker's payload as
 * sent. The store handle, the payload dataclass and the worker's verified
 * registration are live seams; what is portable (and what this C carries) is
 * the closed operation set, the verb each operation runs, the receipt shape,
 * the probe classification and the request's shape/capability gates.
 */

/* PoP: w22_sessionwor_930a87_plan @ gateway/session_worker_delegation.py:persist_worker_delegation
 * The ledger call `persist_worker_delegation` makes for one `operation`:
 *   {"ok": true, "verb": <store method>, "payload_arg": <kwarg carrying the payload>,
 *    "returns_value": bool, "receipt": {<the seven receipt fields>}, "extra": {...}}
 * or {"ok": false, "reason": "invalid_params"}.
 *
 * `operation` must be a "delegation.<name>" string (Python's
 * `removeprefix('delegation.')` behind `isinstance(str) and startswith`).
 * `receipt` supplies the seven receipt fields (epoch, execution_id, session_id,
 * generation, sequence, worker_pid, worker_birth) verbatim; the answer carries
 * exactly those keys, in source order. `extra` holds the dispatch-only probe
 * answers (owner_started_at, probe_refused) and is {} for every other verb.
 * Returns NULL when `receipt` is not an object or lacks a receipt field (the
 * upstream TypeError path). */
json_t *w22_sessionwor_930a87_plan(const char *operation, const json_t *receipt,
                                   const json_t *probe);

/* PoP: w22_sessionwor_930a87_probe @ gateway/session_worker_delegation.py:_probe
 * `_probe`'s answer from the process facts it reads: `alive` is
 * `psutil.Process(worker_pid)` not raising psutil.Error, `birth_equal` is
 * `process_identity.worker_birth(proc) == worker_birth`, `started_at` is
 * `get_process_start_time(worker_pid)`.
 *   {"owner_started_at": started_at | null, "refusal": null | "worker_not_live" |
 *    "permission_denied"}
 * "worker_not_live" wins whenever the process is gone (the source's try wraps
 * the identity compare too). A NULL `probe` reads as a live process whose birth
 * matches. */
json_t *w22_sessionwor_930a87_probe(const json_t *probe);

/* PoP: w22_sessionwor_930a87_delivery_verb @ gateway/session_worker_delegation.py:_DELIVERY
 * The ledger verb for a delivery operation name (claim/ack/drop/defer/release),
 * or NULL when the name is not a delivery verb. */
const char *w22_sessionwor_930a87_delivery_verb(const char *name);

/* PoP: w22_sessionwor_930a87_request_verdict @ gateway/session_worker_delegation.py:worker_delegation_request
 * The two refusals the hook raises before any store work:
 *   "invalid_params"    -- `set(params) != _SCOPE | {'epoch','sequence',
 *                          'operation','payload'}` (an order-insensitive set
 *                          compare; a non-array `params_keys` is not a set);
 *   "permission_denied" -- the actor lacks 'worker:adopt';
 *   "ok"                -- both pass (`_verify` and the store stay upstream). */
const char *w22_sessionwor_930a87_request_verdict(const json_t *params_keys, int has_adopt);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_SESSIONWOR_930A87_H */
