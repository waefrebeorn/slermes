#ifndef NOC_AUXPROF_H
#define NOC_AUXPROF_H
/*
 * Public C API for the port of agent/auxiliary_profile_gates.py (gate auxprof).
 * All exported symbols are module-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/* Recursive check for a reasoning wire control in a profile payload. */
int auxprof_contains_profile_reasoning_fields(const json_t *value);

/*
 * Decision core of profile_forwards_max_tokens.  LIMITATION: the live Python
 * function resolves the provider through a plugin/HOME-layer registry that a
 * pure C port cannot query.  Callers must pass the ALREADY-RESOLVED profile
 * facts: profile_known=0 models the "no profile / exception" path (False);
 * otherwise forwards_max_tokens is the provider profile's flag.  No provider
 * names or behaviours are fabricated here.
 */
int auxprof_profile_forwards_max_tokens(int profile_known, int forwards_max_tokens);

#endif /* NOC_AUXPROF_H */
