/* gate: hermes_cli_main_py___reexec_dependency_sync_off_windows_shim — traced copy of mn_reexec_dependency_sync_off_windows_shim */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mn_reexec_dependency_sync_off_windows_shim(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mn_reexec_dependency_sync_off_windows_shim");
    bool result = mn_reexec_dependency_sync_off_windows_shim(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mn_reexec_dependency_sync_off_windows_shim");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_reexec_dependency_sync_off_windows_shim <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
