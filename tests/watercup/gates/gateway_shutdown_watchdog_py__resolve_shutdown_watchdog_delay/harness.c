/* gate: gateway_shutdown_watchdog_py__resolve_shutdown_watchdog_delay — traced copy of gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay");
    int result = gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_shutdown_watchdog_resolve_shutdown_watchdog_delay <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
