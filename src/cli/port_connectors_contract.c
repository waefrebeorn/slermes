/*
 * port_connectors_contract.c — Port of tools/connectors/contract.py::resolves:
 * whether a target state counts as "resolved" (done, no longer holds the turn
 * open). Base resolved set is {connected, skipped}; for plugin/skill targets a
 * `failed` row is also resolved (the host ran the install already).
 */

#include "port_connectors_contract.h"

#include <string.h>

/* PoP: resolves @ tools/connectors/contract.py:resolves */
int ctc_resolves(const char *kind, const char *state)
{
    if (!state)
        return 0;
    int connected = !strcmp(state, "connected");
    int skipped = !strcmp(state, "skipped");
    if (connected || skipped)
        return 1;
    if (kind && (kind[0] == 'p' /* plugin */ || kind[0] == 's' /* skill */))
        return !strcmp(state, "failed");
    /* connector / mcp / unknown kind: failed is still open */
    return 0;
}
