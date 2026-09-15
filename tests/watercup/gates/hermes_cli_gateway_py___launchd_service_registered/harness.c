/* gate: hermes_cli_gateway_py___launchd_service_registered — traced copy of gw_launchd_service_registered */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_launchd_service_registered(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_launchd_service_registered");
    bool result = gw_launchd_service_registered(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_launchd_service_registered");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_launchd_service_registered <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
