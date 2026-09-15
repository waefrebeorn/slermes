/* gate: gateway_shutdown_watchdog_py___schedule — traced copy of gateway_shutdown_watchdog_u_schedule */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gateway_shutdown_watchdog_u_schedule(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gateway_shutdown_watchdog_u_schedule");
    int result = gateway_shutdown_watchdog_u_schedule(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gateway_shutdown_watchdog_u_schedule");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gateway_shutdown_watchdog_u_schedule <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
