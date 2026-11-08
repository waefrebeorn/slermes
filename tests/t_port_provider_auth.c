/*
 * t_port_provider_auth.c — exhaustive verification harness for the C-side
 * provider-auth registry (lib/libproviderauth). Iterates EVERY entry via
 * provider_auth_iterate() and emits one JSON line per entry:
 *   {"name": <key>, "auth_type": <enum int>}
 * The Python oracle (tests/sta_oracle_provider_auth.py) enumerates the LIVE
 * hermes_cli.auth.PROVIDER_REGISTRY and emits the same shape; the runner
 * diffs them, so any missing/extra/renamed key or wrong auth_type in the C
 * table is caught.
 */

#include "provider_auth.h"

#include <stdio.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    size_t idx = 0;
    const char *name = NULL;
    provider_auth_type_t type = PROVIDER_AUTH_UNKNOWN;
    while (provider_auth_iterate(&idx, &name, &type)) {
        /* JSON-string the name (registry keys may carry '-' etc). */
        printf("{\"name\":\"%s\",\"auth_type\":%d}\n",
               name ? name : "",
               (int)type);
    }
    return 0;
}
