#ifndef NOC_W7_SESSIONOPEN_A7CCCC_H
#define NOC_W7_SESSIONOPEN_A7CCCC_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w7_sessionopen_a7cccc_reason @ gateway/session_open.py:_reason
 * 'create' when the conversation has no messages yet, else 'resume'.
 * receipt_ok/receipt_target stand in for store.bindings.receipt(session_id);
 * row_exists/message_count stand in for store.sessions.get(target). */
struct json_t *w7_sessionopen_a7cccc_reason(const char *session_id,
                                             int receipt_ok, const char *receipt_target,
                                             int row_exists, const char *message_count);

/* PoP: w7_sessionopen_a7cccc_gate_fire_opens @ gateway/session_open.py:_Gate.fire
 * With an unfired gate, fire() sets delivery.opened iff policy is not None and
 * not policy.ignore_user_config. */
int w7_sessionopen_a7cccc_gate_fire_opens(int policy_present, int ignore_user_config);

/* PoP: w7_sessionopen_a7cccc_settled_extra_wait @ gateway/session_open.py:settled
 * The extra turn wait beyond _UNBOUNDED_TURN_WAIT_S (5.0) when callback_timeout
 * is longer: callback_timeout - 5.0, else 0.0. */
double w7_sessionopen_a7cccc_settled_extra_wait(double callback_timeout);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_SESSIONOPEN_A7CCCC_H */
