/* gate: hermes_cli_uninstall_py__remove_portable_tooling_windows — traced copy of uninst_remove_portable_tooling_windows */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uninst_remove_portable_tooling_windows(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uninst_remove_portable_tooling_windows");
    int result = uninst_remove_portable_tooling_windows(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uninst_remove_portable_tooling_windows");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uninst_remove_portable_tooling_windows <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
