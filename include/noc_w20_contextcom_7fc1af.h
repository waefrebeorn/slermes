#ifndef NOC_W20_CONTEXTCOM_7FC1AF_H
#define NOC_W20_CONTEXTCOM_7FC1AF_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of agent/context_compressor_durable.py (gate w20_contextcom_7fc1af).
 *
 * DurableStateMixin's best-effort reads and writes of a bound session row's
 * compressor state.  The store/connection objects are injected through the
 * request objects below (a scripted writer handle, a scripted session store),
 * so the decision logic -- verb resolution, the never-raise/always-default
 * contract, the coercion and the max(default, value) clamp -- is real C.
 */

/* PoP: w20_contextcom_7fc1af_verb_table @ agent/context_compressor_durable.py:_DURABLE_VERBS
 * The storage-interface verb each durable method runs on a writer handle:
 * {"<method>": {"repository": "<repo>", "verb": "<verb>"}, ...}. */
json_t *w20_contextcom_7fc1af_verb_table(void);

/* PoP: w20_contextcom_7fc1af_resolve_verb @ agent/context_compressor_durable.py:_durable_verb
 * `(repository, verb)` when a writer handle is bound, else the store's own
 * method.  Request: {"method": str, "store": "interface"|"none",
 * "session_db": "has"|"missing", "session_db_has_method": bool}.
 * Answer: {"via": "store"|"session_db"|"none", "repository": str|null,
 *          "verb": str|null, "callable": bool}
 *   - a writer handle plus a method outside _DURABLE_VERBS raises KeyError
 *     (Python's dict lookup), reported as {"raised": "KeyError"}. */
json_t *w20_contextcom_7fc1af_resolve_verb(const json_t *req);

/* PoP: w20_contextcom_7fc1af_durable_read @ agent/context_compressor_durable.py:_durable_read
 * Request: {"method", "label", "coerce": "int"|"float"|"str"|"none",
 *           "default": number, "args": [...], "stored": <json>,
 *           "error": str|null, "session_id": str|null,
 *           "store": "interface"|"none", "session_db": "has"|"missing"}.
 * `stored` is what the resolved getter returns; `error` (when non-null) is the
 * exception type it raises instead.  Answer: {"found": bool, "value": ...}
 * -- (False, default) whenever no read happened, the read raised, the session
 * is unbound, the getter is absent, or the coercion itself raised; (True, None)
 * when the row held a non-scalar; else (True, max(default, coerce(stored))). */
json_t *w20_contextcom_7fc1af_durable_read(const json_t *req);

/* PoP: w20_contextcom_7fc1af_durable_write @ agent/context_compressor_durable.py:_durable_write
 * Same request keys (no coerce/default/stored).  Answer: {"ok": bool} --
 * true only when an unbound-session-free write reached a callable setter and
 * did not raise. */
json_t *w20_contextcom_7fc1af_durable_write(const json_t *req);

/* PoP: w20_contextcom_7fc1af_load_durable @ agent/context_compressor_durable.py:_load_durable
 * A read, then `self.<attr> = default if value is None else value` when found.
 * Request adds "initial": the attribute's current value.
 * Answer: {"value": <the attribute after the load>}. */
json_t *w20_contextcom_7fc1af_load_durable(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CONTEXTCOM_7FC1AF_H */
