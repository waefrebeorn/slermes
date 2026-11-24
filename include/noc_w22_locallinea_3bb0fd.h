#ifndef NOC_W22_LOCALLINEA_3BB0FD_H
#define NOC_W22_LOCALLINEA_3BB0FD_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/conversation/_store/local_lineage.py
 * (gate w22_locallinea_3bb0fd).
 *
 * The module reads a sqlite connection, so the rows are a seam here: every
 * entry point takes a `db` document holding exactly the rows the SQL in the
 * source would have returned.
 *
 *   db = {
 *     "sessions":    [{"id": "s1", "end_reason": "compression"|"session_reset"|null,
 *                      "parent_session_id": "s0"|null, "model_config": "<json text>"|null,
 *                      "chat_id": "s1", "user_id": "u1", "session_key": "route"}],
 *     "chain":       {"s1": "s2"},      # _CHAIN_STEP_SQL: parent id -> child row id
 *     "routing":     {"route": "<entry_json text>"},
 *     "state_meta":  {"gateway.local_policy.v1:s1": "<receipt json text>"},
 *     "legacy_ids":  ["s1"]             # ids with a gateway.local_legacy.v1: row
 *   }
 *
 * A session row's absent field reads as NULL, as a sqlite row's would. Two
 * other modules are seams: `local_session_id()` (the id a bound local session
 * must carry) and `legacy_lineage_root()`, so the caller passes their results
 * as `local_id` and `root`.
 *
 * Ported:
 *   validate_local_lineage -- the receipt/lineage validation, including the
 *                             reset vs compression continuation rules.
 *   _routed_entry          -- a route's entry while it still targets a tip.
 *   advance_local_target   -- the noop/refuse decision, the receipt mutation
 *                             (entry + lineage) and the commit it implies.
 *   POLICY_PREFIX / LEGACY_PREFIX constants.
 *
 * _commit_local_target's three UPDATE statements are the write seam; the
 * commit result reports the receipt and the routing entry those writes carry.
 */

/* PoP: w22_locallinea_3bb0fd_policy_prefix @ persistence/sqlite/conversation/_store/local.py:POLICY_PREFIX */
const char *w22_locallinea_3bb0fd_policy_prefix(void);

/* PoP: w22_locallinea_3bb0fd_legacy_prefix @ persistence/sqlite/conversation/_store/local_migration.py:LEGACY_PREFIX */
const char *w22_locallinea_3bb0fd_legacy_prefix(void);

/* PoP: w22_locallinea_3bb0fd_validate @ persistence/sqlite/conversation/_store/local_lineage.py:validate_local_lineage
 * -> {"status": "ok", "tip": <lineage[-1]>}
 *  | {"status": "refused", "reason": "storage_unavailable"}
 *  | {"status": "raised", "type": "KeyError"|"TypeError"|"ValueError"|"AttributeError"} */
json_t *w22_locallinea_3bb0fd_validate(const json_t *receipt, const char *local_id,
                                       const char *root, const json_t *db);

/* PoP: w22_locallinea_3bb0fd_routed_entry @ persistence/sqlite/conversation/_store/local_lineage.py:_routed_entry
 * -> {"status": "ok", "value": <routing entry>|null} | {"status": "raised", "type": ...} */
json_t *w22_locallinea_3bb0fd_routed_entry(const char *route, const char *tip,
                                           const json_t *db);

/* PoP: w22_locallinea_3bb0fd_advance @ persistence/sqlite/conversation/_store/local_lineage.py:advance_local_target
 * -> {"action": "noop"}
 *  | {"status": "refused", "reason": "storage_unavailable"|"admission_conflict"}
 *  | {"action": "commit", "receipt": {...}, "routed": {...}}
 * (a raised KeyError/TypeError/ValueError inside the source's try becomes
 *  storage_unavailable, and a raised AuthorityRefusal keeps its reason). */
json_t *w22_locallinea_3bb0fd_advance(const char *parent_session_id,
                                      const char *child_session_id,
                                      const json_t *entry, const json_t *db,
                                      const char *local_id, const char *root);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_LOCALLINEA_3BB0FD_H */
