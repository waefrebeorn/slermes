#ifndef NOC_W20_LOCALMIGRA_5DCDB5_H
#define NOC_W20_LOCALMIGRA_5DCDB5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of persistence/sqlite/authority/_local_migration.py (gate
 * w20_localmigra_5dcdb5):
 *
 *   bind_legacy_target(holder, conn, receipt, owner)
 *
 * The guard ladder (receipt identity, row presence, legacy ownership, an
 * already imported binding, an existing compression chain step, idle, a live
 * turn lease, an already migrated key) and the two statements it then writes
 * are reproduced here. Everything that needs a live sqlite connection or the
 * filesystem owner rule is injected: the scene object carries what each
 * collaborator answered.
 */

/* BINDING_PREFIX ('gateway.history_control.v1.') / LEGACY_PREFIX
 * ('gateway.local_legacy.v1:'). */
const char *w20_localmigra_5dcdb5_binding_prefix(void);
const char *w20_localmigra_5dcdb5_legacy_prefix(void);

/* PoP: w20_localmigra_5dcdb5_canonical_dumps @ persistence/core/canonical.py:canonical_dumps
 * json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).
 * The authority's one encoding; the stored meta/blob columns use it. */
json_t *w20_localmigra_5dcdb5_canonical_dumps(const json_t *value);

/* PoP: w20_localmigra_5dcdb5_canonical_json @ persistence/core/canonical.py:canonical_json
 * The same encoding for a dict only: {"value": text}, else
 * {"refusal": "invalid_params"} (canonical_json's AuthorityRefusal). */
json_t *w20_localmigra_5dcdb5_canonical_json(const json_t *value);

/* PoP: w20_localmigra_5dcdb5_bind_legacy_target @ persistence/sqlite/authority/_local_migration.py:bind_legacy_target
 *
 * scene keys (each is what the live collaborator answered):
 *   "row"         sessions row object, or null      (SELECT * FROM sessions)
 *   "imported"    state_meta binding value, or null (BINDING_PREFIX + target)
 *   "binding_ok"  bool  -- session_rules.legacy_local.binding_names
 *   "validate"    "ok" or a refusal reason -- validate_legacy_row
 *   "chain_step"  bool  -- _CHAIN_STEP_SQL returned a row
 *   "idle"        "ok" or a refusal reason -- require_idle
 *   "lease_root"  string returned by _session_turn_lease_key_on_conn
 *   "lease"       bool  -- a live turn lease on that root
 *   "legacy_key"  bool  -- LEGACY_PREFIX + target already in state_meta
 *   "client_row"  bool  -- session_rules.legacy_local.client_history_row
 *   "now"         time.time() the lease expiry is compared against
 *
 * Answer: {"refusal": reason} | {"statements": [{"sql":..., "params":[...]}]}
 *        | {"raised": "KeyError", "message": "'entry'"} for a receipt that is
 *          missing a key the Python body reads positionally. */
json_t *w20_localmigra_5dcdb5_bind_legacy_target(const json_t *receipt,
                                                 const json_t *scene);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_LOCALMIGRA_5DCDB5_H */
