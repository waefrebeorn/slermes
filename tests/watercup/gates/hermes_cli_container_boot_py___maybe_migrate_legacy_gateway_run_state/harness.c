/* gate: hermes_cli_container_boot_py___maybe_migrate_legacy_gateway_run_state — traced copy of hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te");
    int result = hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_cli_container_boot_u_maybe_migrate_legacy_gateway_run_te <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
