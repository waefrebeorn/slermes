/*
 * route_identity.c — canonical C11 port of hermes_cli/route_identity.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Ported should_clear_context_pin from src/cli/route_identity_flow.c. */
extern int rid_should_clear_context_pin_cstr(const char *configured_model,
                                             const char *active_model, int routes_match);

/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_route_identity_should_clear_context_pin @ hermes_cli/route_identity.py:should_clear_context_pin */
int hermes_cli_route_identity_should_clear_context_pin(const char *arg) {
    int out = 1;
    if (arg && *arg) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", arg);
        char *t1 = strchr(buf, '\t');
        char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
        int match = t2 && t2[1] == '1';
        if (t1) *t1 = '\0';
        if (t2) *t2 = '\0';
        const char *cm = buf;
        const char *am = t1 ? t1 + 1 : "";
        out = rid_should_clear_context_pin_cstr(cm, am, match);
    }
    printf("%d\n", out ? 1 : 0);
    return out;
}

/* PoP: hermes_cli_route_identity_should_clear_context_pin_async @ hermes_cli/route_identity.py:should_clear_context_pin_async */
int hermes_cli_route_identity_should_clear_context_pin_async(const char *arg) {
    /* Python: async wrapper — offloads the sync should_clear_context_pin to
     * a worker thread via asyncio.to_thread.  C has no event loop, so this
     * is a direct delegation to the sync port (the cache-only resolution
     * chain in C contains no blocking disk I/O that needs offloading). */
    return hermes_cli_route_identity_should_clear_context_pin(arg);
}
