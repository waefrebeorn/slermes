/*
 * port_connectors_contract.c — Port of tools/connectors/contract.py:
 * `resolves` (whether a target state counts as "resolved") and `allowed`
 * (which Actor may cause a given state move). Base resolved set is
 * {connected, skipped}; for plugin/skill targets a `failed` row is also
 * resolved (the host ran the install already).
 */

#include "port_connectors_contract.h"

#include <string.h>

/* PoP: ctc_resolves @ tools/connectors/contract.py:resolves */
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

/* (kind, from) -> {to: the only actor allowed to cause it}. plugin and skill
 * rows install like an MCP entry, so they reuse the mcp edges (the live module
 * copies them in at import time). */
static const struct {
    const char *kind;
    const char *from;
    const char *to;
    const char *actor;
} CTC_EDGES[] = {
    {"connector", "pending",   "initiated", "backend_watcher"},
    {"connector", "pending",   "failed",    "backend_watcher"},
    {"connector", "pending",   "skipped",   "user"},
    {"connector", "initiated", "connected", "backend_watcher"},
    {"connector", "initiated", "failed",    "backend_watcher"},
    {"connector", "initiated", "expired",   "clock"},
    {"connector", "initiated", "skipped",   "user"},
    {"connector", "failed",    "initiated", "user"},
    {"connector", "failed",    "skipped",   "user"},
    {"connector", "expired",   "initiated", "user"},
    {"connector", "expired",   "skipped",   "user"},
    {"mcp",       "pending",   "initiated", "backend_watcher"},
    {"mcp",       "pending",   "failed",    "backend_watcher"},
    {"mcp",       "pending",   "skipped",   "user"},
    {"mcp",       "initiated", "connected", "backend_watcher"},
    {"mcp",       "initiated", "failed",    "backend_watcher"},
    {"mcp",       "initiated", "skipped",   "user"},
    {"mcp",       "failed",    "initiated", "user"},
    {"mcp",       "failed",    "skipped",   "user"},
};

/* PoP: ctc_allowed @ tools/connectors/contract.py:allowed */
const char *ctc_allowed(const char *kind, const char *current, const char *to)
{
    if (!kind || !current || !to)
        return NULL;
    const char *k = kind;
    if (!strcmp(kind, "plugin") || !strcmp(kind, "skill"))
        k = "mcp";
    size_t n = sizeof(CTC_EDGES) / sizeof(CTC_EDGES[0]);
    for (size_t i = 0; i < n; i++) {
        if (!strcmp(CTC_EDGES[i].kind, k) &&
            !strcmp(CTC_EDGES[i].from, current) &&
            !strcmp(CTC_EDGES[i].to, to))
            return CTC_EDGES[i].actor;
    }
    return NULL;
}
