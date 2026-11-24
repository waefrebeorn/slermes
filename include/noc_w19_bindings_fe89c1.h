#ifndef NOC_W19_BINDINGS_FE89C1_H
#define NOC_W19_BINDINGS_FE89C1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_bindings_fe89c1_new_receipt @ persistence/sqlite/authority/bindings.py:_new_receipt
 * The receipt dict the reservation stores (legacy_session_id / lineage only when set). */
json_t *w19_bindings_fe89c1_new_receipt(json_t *binding);

/* PoP: w19_bindings_fe89c1_api_binding @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.api_binding
 * The ApiBinding record of a stored receipt (NULL receipt -> null). */
json_t *w19_bindings_fe89c1_api_binding(json_t *receipt);

/* PoP: w19_bindings_fe89c1_binding_marks @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.binding_marks
 * {"policy":...,"legacy":...,"lineage_owner":null|owner}. */
json_t *w19_bindings_fe89c1_binding_marks(json_t *policy, json_t *legacy, json_t *owner);

/* PoP: w19_bindings_fe89c1_handoff_head @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.handoff_head
 * [owner|null, head]. */
json_t *w19_bindings_fe89c1_handoff_head(json_t *owner, json_t *head);

/* PoP: w19_bindings_fe89c1_legacy_adoption_scan @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.legacy_adoption_scan
 * {"session_ids":[ids...],"marked":[sorted marks]}. */
json_t *w19_bindings_fe89c1_legacy_adoption_scan(json_t *ids, json_t *marked);

/* PoP: w19_bindings_fe89c1_lineages @ persistence/sqlite/authority/bindings.py:SQLiteSessionBindingRepository.list_lineages
 * [{"owner":...,"lineage":[ids...]}] from the (owner, lineage) pairs the body found. */
json_t *w19_bindings_fe89c1_lineages(json_t *found);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_BINDINGS_FE89C1_H */
