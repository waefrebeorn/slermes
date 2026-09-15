/* gate: hermes_cli_container_boot_py___is_legacy_gateway_run_request — traced copy of hermes_cli_container_boot_u_is_legacy_gateway_run_request */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_container_boot_u_is_legacy_gateway_run_request(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_container_boot_u_is_legacy_gateway_run_request");
    int result = hermes_cli_container_boot_u_is_legacy_gateway_run_request(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_container_boot_u_is_legacy_gateway_run_request");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_container_boot_u_is_legacy_gateway_run_request <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
