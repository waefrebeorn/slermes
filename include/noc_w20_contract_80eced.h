#ifndef NOC_W20_CONTRACT_80ECED_H
#define NOC_W20_CONTRACT_80ECED_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of tools/connectors/contract.py (gate w20_contract_80eced).
 *
 * The module is three str enums plus two pure tables: which states count as
 * resolved, and which actor may move a target from one state to another.
 * `operation.py` enforces the table; nothing here touches a store. */

/* PoP: w20_contract_80eced_resolves @ tools/connectors/contract.py:resolves
 * `state in _RESOLVED_BY_KIND.get(kind, RESOLVED_STATES)`.  A plain string
 * that names no state simply is not in the set, so it answers 0. */
int w20_contract_80eced_resolves(const char *kind, const char *state);

/* PoP: w20_contract_80eced_allowed @ tools/connectors/contract.py:allowed
 * `TRANSITIONS.get((kind, current), {}).get(to)` -> actor value or null. */
json_t *w20_contract_80eced_allowed(const char *kind, const char *current, const char *to);

/* PoP: w20_contract_80eced_transitions @ tools/connectors/contract.py:TRANSITIONS
 * {from_state: {to_state: actor}} for one kind, or null when the kind has no
 * edges.  Object key order follows the table's declaration order. */
json_t *w20_contract_80eced_transitions(const char *kind);

/* PoP: w20_contract_80eced_kinds @ tools/connectors/contract.py:KINDS */
json_t *w20_contract_80eced_kinds(void);

/* PoP: w20_contract_80eced_resolved_states @ tools/connectors/contract.py:RESOLVED_STATES
 * A frozenset upstream, so callers sort; emitted sorted here too. */
json_t *w20_contract_80eced_resolved_states(void);

/* PoP: w20_contract_80eced_enum_values @ tools/connectors/contract.py:TargetState
 * Values of TargetState / Actor / SettleReason in declaration order; null for
 * an unknown enum name. */
json_t *w20_contract_80eced_enum_values(const char *enum_name);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W20_CONTRACT_80ECED_H */
