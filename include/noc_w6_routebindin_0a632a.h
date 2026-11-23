#ifndef NOC_W6_ROUTEBINDIN_0A632A_H
#define NOC_W6_ROUTEBINDIN_0A632A_H

#include "hermes_json.h"

/* C port of the client-resolution decision of agent/route_binding.py:
 * resolve_route_entry_client. The heavyweight collaborators (credential pool,
 * client factory, api-mode hint, model normalization, secret lookup) are the
 * same deterministic seams injected in the oracle; this function reproduces
 * the wrapper's control flow around them. */

/* PoP: w6_routebindin_0a632a_resolve_route_entry_client @ agent/route_binding.py:resolve_route_entry_client
 * Returns {"client": <str|null>, "model": <str>, "api_mode_explicit": <bool>,
 *          "api_mode": <str>, "pool_choice": <bool>}. */
json_t *w6_routebindin_0a632a_resolve_route_entry_client(const json_t *entry,
        const char *provider, const char *model, int raw_codex);

#endif /* NOC_W6_ROUTEBINDIN_0A632A_H */
