/* gate: hermes_cli_gateway_windows_py__is_startup_entry_installed — traced copy of gw_is_startup_entry_installed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_is_startup_entry_installed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_is_startup_entry_installed");
    int result = gw_is_startup_entry_installed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_is_startup_entry_installed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_is_startup_entry_installed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
