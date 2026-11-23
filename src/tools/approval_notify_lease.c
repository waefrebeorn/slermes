/*
 * approval_notify_lease.c — C home for upstream Hermes tools/approval_notify_lease.py.
 *
 * PoP: w15_approvalno_b886ee_current @ tools/approval_notify_lease.py:current
 * PoP: w15_approvalno_b886ee_deliverable @ tools/approval_notify_lease.py:NotifyLease.deliverable
 * PoP: w15_approvalno_b886ee_revoke @ tools/approval_notify_lease.py:revoke_session_locked
 *
 * ContextVar/WeakSet state cannot cross the process boundary, so the oracle
 * injects the relevant state explicitly (current lease key, callback presence,
 * gateway notifier presence, lease key array). The exported functions mirror
 * the live module's decisions on that state.
 */
#include "noc_w15_approvalno_b886ee.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

int w15_approvalno_b886ee_current(const char *session_key, const char *current_lease_key) {
    if (!current_lease_key) return 0;           /* ContextVar default None */
    if (!session_key) session_key = "";
    return !strcmp(current_lease_key, session_key);
}

int w15_approvalno_b886ee_deliverable(int active, int has_callback, int gateway_has_notifier) {
    /* deliverable = not self.active or self._target_locked() is not None
     * _target_locked = None when inactive, else callback or gateway notifier. */
    if (!active) return 1;
    return (has_callback || gateway_has_notifier) ? 1 : 0;
}

struct json_t *w15_approvalno_b886ee_revoke(const struct json_t *lease_keys,
                                             const char *session_key) {
    json_t *out = json_array();
    if (!session_key) session_key = "";
    if (lease_keys && lease_keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < lease_keys->c.count; ++i) {
            const json_t *v = lease_keys->c.items[i];
            const char *key = (v && v->type == JSON_STRING) ? v->str_val : "";
            /* revoke_session_locked clears active for every lease of this session. */
            int active = strcmp(key, session_key) != 0;
            json_append(out, json_bool(active ? 1 : 0));
        }
    }
    return out;
}
