/* gate: hermes_cli_uninstall_py___is_default_hermes_home — traced copy of uninst_u_is_default_hermes_home */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uninst_u_is_default_hermes_home(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uninst_u_is_default_hermes_home");
    int result = uninst_u_is_default_hermes_home(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uninst_u_is_default_hermes_home");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uninst_u_is_default_hermes_home <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
