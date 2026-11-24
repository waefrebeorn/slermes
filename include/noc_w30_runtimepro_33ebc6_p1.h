/*
 * noc_w30_runtimepro_33ebc6_p1.h — C surface for upstream
 * hermes_cli/runtime_provider_custom.py, gate w30_runtimepro_33ebc6_p1.
 *
 * Provider-spec normalisation, base-URL/route identity, config scanning of
 * ``providers:`` and the legacy ``custom_providers:`` list, the shared field
 * lifting, and the named-entry lookup that both the switch and the runtime use.
 *
 * Every function takes one request object and returns the computed value:
 * the harness feeds a JSON line per case and prints the result. External answers
 * (config file, credential store, provider registry, compatible-entry view,
 * plugin transport registry) arrive as fields of that object — nothing here
 * reads a file, a keyring or the network.
 */
#ifndef NOC_W30_RUNTIMEPRO_33EBC6_P1_H
#define NOC_W30_RUNTIMEPRO_33EBC6_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Dispatch one request ({"func": <name>, ...args}); *known is 0 for an
 * unknown func. Returns an owned json_t (never NULL). */
json_t *w30_runtimepro_33ebc6_p1_dispatch(const json_t *req, int *known);

/* Number of ported symbols in this gate's dispatch table. */
size_t w30_runtimepro_33ebc6_p1_count(void);

#ifdef __cplusplus
}
#endif
#endif
