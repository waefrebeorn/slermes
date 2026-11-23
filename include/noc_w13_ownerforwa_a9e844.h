#ifndef NOC_W13_OWNERFORWA_A9E844_H
#define NOC_W13_OWNERFORWA_A9E844_H

struct json_t;
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: ownerforw_forwardable @ tui_gateway/owner_forward.py:forwardable
 * Pure predicate. `refusal` is an object with optional "ledger_status"; returns bool. */
struct json_t *ownerforw_forwardable(const struct json_t *refusal, const struct json_t *session,
                                     const struct json_t *text, const struct json_t *params);

/* PoP: ownerforw_discover_owner @ tui_gateway/owner_forward.py:discover_owner
 * Seamed discovery: {"endpoint": <v>} on ready, else {"error": <str>}. */
struct json_t *ownerforw_discover_owner(const struct json_t *state, const struct json_t *endpoint);

/* PoP: ownerforw_payload_for @ tui_gateway/owner_forward.py:_payload_for
 * Seamed EVENTS contract: `contract_fields` null (no contract) or object mapping
 * event_type -> array of allowed fields (null means contract.payload is None).
 * `valid` is the model_validate outcome (JSON bool). Returns shaped object or null. */
struct json_t *ownerforw_payload_for(const char *event_type, const struct json_t *payload,
                                     const struct json_t *contract_fields, const struct json_t *valid);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_OWNERFORWA_A9E844_H */
