#ifndef NOC_W11_AUTOMATION_A6FFEC_H
#define NOC_W11_AUTOMATION_A6FFEC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_automation_a6ffec_descriptor @ session_rules/automation.py:automation_descriptor */
struct json_t *w11_automation_a6ffec_descriptor(const struct json_t *payload);

/* PoP: w11_automation_a6ffec_folded_payload @ session_rules/automation.py:folded_payload */
struct json_t *w11_automation_a6ffec_folded_payload(const struct json_t *payload);

/* PoP: w11_automation_a6ffec_goal_contract @ session_rules/automation.py:GoalContract.from_dict
 * Returns from_dict(data).to_dict(). */
struct json_t *w11_automation_a6ffec_goal_contract(const struct json_t *data);

/* PoP: w11_automation_a6ffec_goal_contract_empty @ session_rules/automation.py:GoalContract.is_empty */
int w11_automation_a6ffec_goal_contract_empty(const struct json_t *data);

/* PoP: w11_automation_a6ffec_goal_gate @ session_rules/automation.py:GoalGate.from_dict
 * Returns from_dict(data).to_dict() (asdict). */
struct json_t *w11_automation_a6ffec_goal_gate(const struct json_t *data);

/* PoP: w11_automation_a6ffec_heartbeat_state @ session_rules/automation.py:HeartbeatState.from_json
 * Returns asdict(from_json(raw)). */
struct json_t *w11_automation_a6ffec_heartbeat_state(const char *raw);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_AUTOMATION_A6FFEC_H */
