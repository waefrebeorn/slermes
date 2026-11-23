#ifndef NOC_W9_TURNFACADE_6BC551_H
#define NOC_W9_TURNFACADE_6BC551_H

#include "hermes_json.h"

/* agent/turn_facade_lease.py — durable turn-lease identity helpers. */
/* reader_present: a store/callable reader was available (i.e. the read was attempted).
 * reader_result: the value it returned (numeric -> committed expiry, else fallback). */
double  w9_turnfacade_6bc551_committed_lease_expiry(json_t *reader_result, int reader_present, double floor);
json_t *w9_turnfacade_6bc551_current_session_id(json_t *agent_session_id, const char *fallback);

#endif /* NOC_W9_TURNFACADE_6BC551_H */
