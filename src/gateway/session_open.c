/*
 * session_open.c — faithful C11 port of upstream Hermes gateway/session_open.py.
 *
 * PoP: w7_sessionopen_a7cccc_reason @ gateway/session_open.py:_reason
 * PoP: w7_sessionopen_a7cccc_gate_fire_opens @ gateway/session_open.py:_Gate.fire
 * PoP: w7_sessionopen_a7cccc_settled_extra_wait @ gateway/session_open.py:settled
 */
#include "hermes_json.h"
#include "noc_w7_sessionopen_a7cccc.h"
#include <stdlib.h>
#include <string.h>

/* _UNBOUNDED_TURN_WAIT_S = 5.0. */
#define SO_UNBOUNDED_TURN_WAIT_S 5.0

struct json_t *w7_sessionopen_a7cccc_reason(const char *session_id,
                                             int receipt_ok, const char *receipt_target,
                                             int row_exists, const char *message_count) {
    const char *target = (receipt_ok && receipt_target && *receipt_target)
                         ? receipt_target : (session_id ? session_id : "");
    long long count = 0;
    if (message_count && *message_count) count = strtoll(message_count, NULL, 10);
    if (row_exists && count != 0)
        return json_string("resume");
    return json_string("create");
}

int w7_sessionopen_a7cccc_gate_fire_opens(int policy_present, int ignore_user_config) {
    return (policy_present && !ignore_user_config) ? 1 : 0;
}

double w7_sessionopen_a7cccc_settled_extra_wait(double callback_timeout) {
    if (callback_timeout > SO_UNBOUNDED_TURN_WAIT_S)
        return callback_timeout - SO_UNBOUNDED_TURN_WAIT_S;
    return 0.0;
}
