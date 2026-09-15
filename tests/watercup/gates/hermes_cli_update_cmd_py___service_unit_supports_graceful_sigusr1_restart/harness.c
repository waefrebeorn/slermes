/* gate: hermes_cli_update_cmd_py___service_unit_supports_graceful_sigusr1_restart — traced copy of _service_unit_supports_graceful_sigusr1_restart */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _service_unit_supports_graceful_sigusr1_restart(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_service_unit_supports_graceful_sigusr1_restart");
    int result = _service_unit_supports_graceful_sigusr1_restart(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_service_unit_supports_graceful_sigusr1_restart");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _service_unit_supports_graceful_sigusr1_restart <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
