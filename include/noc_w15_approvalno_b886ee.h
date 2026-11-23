#ifndef NOC_W15_APPROVALNO_B886EE_H
#define NOC_W15_APPROVALNO_B886EE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_approvalno_b886ee_current @ tools/approval_notify_lease.py:current
 * `current_lease_key` is an explicit seam for the ContextVar-held lease key
 * (JSON null == no lease installed). 1 when current(session_key) is not None. */
int w15_approvalno_b886ee_current(const char *session_key, const char *current_lease_key);

/* PoP: w15_approvalno_b886ee_deliverable @ tools/approval_notify_lease.py:NotifyLease.deliverable
 * `gateway_has_notifier` stands in for approval._gateway_notify_cbs[session_key]. */
int w15_approvalno_b886ee_deliverable(int active, int has_callback, int gateway_has_notifier);

/* PoP: w15_approvalno_b886ee_revoke @ tools/approval_notify_lease.py:revoke_session_locked
 * `lease_keys` is a JSON array of per-lease session keys (all born active).
 * Returns a NEW JSON array of booleans: each lease's `active` after revoking
 * `session_key`. Caller frees. */
struct json_t *w15_approvalno_b886ee_revoke(const struct json_t *lease_keys,
                                             const char *session_key);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_APPROVALNO_B886EE_H */
