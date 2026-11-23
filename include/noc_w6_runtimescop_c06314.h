#ifndef NOC_W6_RUNTIMESCOP_C06314_H
#define NOC_W6_RUNTIMESCOP_C06314_H

#include "hermes_json.h"

/* C port of the pure value helpers in agent/runtime_scope.py. */

/* PoP: w6_runtimescop_c06314_claim_fence @ agent/runtime_scope.py:claim_fence
 * Fence(owner_epoch, generation) validates both as positive ints (type is int),
 * raising ValueError otherwise. Returns {"epoch":..,"generation":..} or null. */
json_t *w6_runtimescop_c06314_claim_fence(const json_t *row);

/* PoP: w6_runtimescop_c06314_identity @ agent/runtime_scope.py:_identity
 * Returns [tenant, agent, actor, owner, can_administer] from a scope-shaped
 * JSON object. */
json_t *w6_runtimescop_c06314_identity(const json_t *scope);

#endif /* NOC_W6_RUNTIMESCOP_C06314_H */
