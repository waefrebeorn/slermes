/*
 * sessions_owner.c — real C11 port of upstream hermes_cli/sessions_owner.py.
 *
 * PoP: sessowner_report_owner_failure_message @ hermes_cli/sessions_owner.py:report_owner_failure
 * PoP: sessowner_prune_rpc_timeout             @ hermes_cli/sessions_owner.py:PRUNE_RPC_TIMEOUT
 * PoP: sessowner_live_owner_endpoint           @ hermes_cli/sessions_owner.py:live_owner
 *
 * The upstream module's remaining functions are network/async I/O boundaries
 * (gateway discovery and the session.prune RPC); the deterministic pieces that
 * can be exercised by a differential gate are ported here. Self-contained.
 */
#include "noc_sessowner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PRUNE_RPC_TIMEOUT = 600 */
int sessowner_prune_rpc_timeout(void) { return 600; }

/* PoP: sessowner_live_owner_endpoint @ hermes_cli/sessions_owner.py:live_owner */
const char *sessowner_live_owner_endpoint(const char *state, const char *endpoint) {
    if (state && !strcmp(state, "ready")) return endpoint;
    return NULL;
}

/* PoP: sessowner_report_owner_failure_message @ hermes_cli/sessions_owner.py:report_owner_failure */
char *sessowner_report_owner_failure_message(const char *exc) {
    const char *prefix = "Error: the gateway did not complete the prune (";
    const char *suffix = "). Run `hermes sessions prune --dry-run` "
                         "with the same filters to see what is left.";
    const char *e = exc ? exc : "";
    size_t n = strlen(prefix) + strlen(e) + strlen(suffix) + 1;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%s%s%s", prefix, e, suffix);
    return out;
}

