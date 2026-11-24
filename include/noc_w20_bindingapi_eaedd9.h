#ifndef NOC_W20_BINDINGAPI_EAEDD9_H
#define NOC_W20_BINDINGAPI_EAEDD9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/authority/_binding_api.py (gate w20_bindingapi_eaedd9).
 *
 * The write ladder is ported against a scripted store: the caller hands the
 * state_meta map, the sessions rows and the gateway_routing rows as JSON and
 * gets the post-write snapshot (or the refusal/raise) back. SQLite's own
 * semantics are scripted by the oracle with a real in-memory database.
 */

/* PoP: w20_bindingapi_eaedd9_bind @ persistence/sqlite/authority/_binding_api.py:bind_api
 * `bind_api`'s validation ladder + writes. Request keys:
 *   epoch, runtime_epoch, session_id, route, storage_source, title,
 *   declared_key, caller_key, source {user_id}, entry, receipt, now,
 *   gate {reason} | null, walked_from, fence_ok (bool),
 *   state_meta {k:v}, sessions [{id,source,title,session_key,...}],
 *   routing [{session_key, updated_at, entry}]
 * Answer: {"status":"ok","value":{epoch,state_meta,sessions,routing}} |
 *         {"status":"refused","reason":...} |
 *         {"status":"raised","type":...,"message":...}. */
json_t *w20_bindingapi_eaedd9_bind(const json_t *request);

/* PoP: w20_bindingapi_eaedd9_receipt @ persistence/sqlite/authority/_binding_api.py:api_binding_receipt
 * The stored binding receipt for `session_id` out of a state_meta map, decoded,
 * else null. */
json_t *w20_bindingapi_eaedd9_receipt(const json_t *state_meta, const char *session_id);

/* PoP: w20_bindingapi_eaedd9_declared_session @ persistence/sqlite/authority/_binding_api.py:declared_session
 * The session a declared conversation key is registered to in a state_meta map,
 * else null. */
json_t *w20_bindingapi_eaedd9_declared_session(const json_t *state_meta, const char *key);

/* PoP: w20_bindingapi_eaedd9_heal_replacement @ persistence/sqlite/authority/_binding_api.py:_heal_replacement
 * True when the route's holder has no binding of its own (the caller may take
 * the route back), False when it does. */
int w20_bindingapi_eaedd9_heal_replacement(const json_t *state_meta, const char *holder);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_BINDINGAPI_EAEDD9_H */
