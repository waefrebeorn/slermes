/*
 * noc_w30_runtimepro_33ebc6_p2.h — C surface for upstream
 * hermes_cli/runtime_provider_custom.py, gate w30_runtimepro_33ebc6_p2.
 *
 * Identity recovery for a bare ``custom`` pick (base URL -> ``custom:<name>``,
 * model id -> entry, session/canonical healing) plus the runtime-derivation
 * decisions: extras merging, direct-API aliasing, credential precedence and the
 * OpenCode-family api_mode re-derivation.
 *
 * Same protocol as p1: one request object in, one computed value out; every
 * external answer (credential pool, host-gated env keys, entry lookup, managed
 * local endpoint, models-module helpers) is a field of the request.
 */
#ifndef NOC_W30_RUNTIMEPRO_33EBC6_P2_H
#define NOC_W30_RUNTIMEPRO_33EBC6_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

json_t *w30_runtimepro_33ebc6_p2_dispatch(const json_t *req, int *known);

size_t w30_runtimepro_33ebc6_p2_count(void);

#ifdef __cplusplus
}
#endif
#endif
