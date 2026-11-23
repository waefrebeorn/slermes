/*
 * runtime_contract.c — C port of upstream Hermes gateway/runtime_contract.py.
 * PoP: rtcontract_next_start_action @ gateway/runtime_contract.py:next_start_action
 *
 * Faithful decision order: update_paused wins first, then the explicit state
 * table, then the unknown-state error, then service start, then spawn.
 */
#include "noc_rtcontract.h"

#include <string.h>

const char *rtcontract_next_start_action(const char *state,
                                         int installed_service,
                                         int service_may_start,
                                         int update_paused)
{
    if (update_paused) {
        return "wait-update";
    }
    if (state && !strcmp(state, "ready"))        return "attach";
    if (state && !strcmp(state, "starting"))     return "wait-owner";
    if (state && !strcmp(state, "draining"))     return "wait-owner";
    if (state && !strcmp(state, "incompatible")) return "reject-version";
    if (state && !strcmp(state, "inaccessible")) return "reject-access";
    if (state && !strcmp(state, "conflict"))     return "reject-conflict";

    /* Unknown states (including the empty string) are not "absent". */
    if (!state || strcmp(state, "absent")) {
        return NULL; /* upstream: raise ValueError("unknown runtime state") */
    }
    if (service_may_start) {
        return "wait-service";
    }
    return installed_service ? "start-service" : "spawn-unmanaged";
}
