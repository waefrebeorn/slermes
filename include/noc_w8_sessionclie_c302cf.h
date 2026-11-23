#ifndef NOC_W8_SESSIONCLIE_C302CF_H
#define NOC_W8_SESSIONCLIE_C302CF_H

/*
 * Public C API for the port of gateway/session_client.py (gate w8_sessionclie_c302cf).
 * All exported symbols are sessionclie_-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/* PoP: sessionclie_client_problem @ gateway/session_client.py:client_problem
 * Returns the validation error string, or JSON null when the label is valid. */
json_t *sessionclie_client_problem(const json_t *value);

/* PoP: sessionclie_policy_client @ gateway/session_client.py:policy_client
 * `policy` is JSON null (no policy) or a string carrying policy.request_json.
 * Returns a copy of the client label object, or JSON null. */
json_t *sessionclie_policy_client(const json_t *policy);

#endif /* NOC_W8_SESSIONCLIE_C302CF_H */
