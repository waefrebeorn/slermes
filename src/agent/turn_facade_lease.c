/*
 * turn_facade_lease.c - C home for upstream Hermes agent/turn_facade_lease.py.
 *
 * PoP: w9_turnfacade_6bc551_committed_lease_expiry @ agent/turn_facade_lease.py:_committed_lease_expiry
 * PoP: w9_turnfacade_6bc551_current_session_id @ agent/turn_facade_lease.py:DurableTurnLease._current_session_id
 *
 * The store/db readers are scripted by the oracle (numeric value, non-numeric
 * value, or raised exception); the C seam receives the resolved reader result so
 * the committed-vs-floor decision is the deterministic part under test.
 */
#include "noc_w9_turnfacade_6bc551.h"
#include <string.h>

#define W9TFL_LEASE_TTL 300.0

double w9_turnfacade_6bc551_committed_lease_expiry(json_t *reader_result, int reader_present, double floor) {
    if (reader_present && reader_result) {
        /* Python: isinstance(committed, (int, float)) -> float(committed);
         * bool is an int subclass, so True/False are numeric here. */
        if (reader_result->type == JSON_NUMBER) return reader_result->num_val;
        if (reader_result->type == JSON_BOOL) return reader_result->bool_val ? 1.0 : 0.0;
    }
    return floor + W9TFL_LEASE_TTL;
}

json_t *w9_turnfacade_6bc551_current_session_id(json_t *agent_session_id, const char *fallback) {
    if (agent_session_id && agent_session_id->type == JSON_STRING &&
        agent_session_id->str_val && agent_session_id->str_val[0]) {
        return json_copy(agent_session_id);
    }
    return json_string(fallback ? fallback : "");
}
