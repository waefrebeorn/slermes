/*
 * route_identity.c — canonical C11 port of hermes_cli/route_identity.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_route_identity_should_clear_context_pin @ hermes_cli/route_identity.py:should_clear_context_pin */
int hermes_cli_route_identity_should_clear_context_pin(const char *arg) {
    /* Python: model mismatch or route mismatch; fail-closed True. Arg =
     * "configured_model\tactive_model\tmatch". */
    if (!arg || !*arg) { printf("1\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *cm = arg;
    const char *am = t1 ? t1 + 1 : "";
    int match = t2 && t2[1] == '1';
    if (cm[0] && strcmp(cm, am) != 0) { printf("1\n"); return 0; }
    if (!match) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: hermes_cli_route_identity_should_clear_context_pin_async @ hermes_cli/route_identity.py:should_clear_context_pin_async */
int hermes_cli_route_identity_should_clear_context_pin_async(const char *arg) {
    /* Python: async wrapper — offloads the sync should_clear_context_pin to
     * a worker thread via asyncio.to_thread.  C has no event loop, so this
     * is a direct delegation to the sync port (the cache-only resolution
     * chain in C contains no blocking disk I/O that needs offloading). */
    return hermes_cli_route_identity_should_clear_context_pin(arg);
}
