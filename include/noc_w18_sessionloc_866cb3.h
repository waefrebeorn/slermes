#ifndef NOC_W18_SESSIONLOC_866CB3_H
#define NOC_W18_SESSIONLOC_866CB3_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionloc_866cb3_binding @ gateway/session_local_adoption.py:_binding
 *
 * Normalize a stored receipt dict into the SessionBinding comparison shape the
 * bindings verbs use. Field mapping and defaults mirror the Python:
 *   profile_id, principal_id, request_id, session_id, route, entry, policy
 * are copied verbatim;
 *   lineage = tuple(receipt.get('lineage', [receipt['session_id']]));
 *   legacy_session_id = receipt.get('legacy_session_id').
 * Returns a new object (caller frees).
 */
struct json_t *w18_sessionloc_866cb3_binding(const struct json_t *receipt);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_SESSIONLOC_866CB3_H */
