/* gate: hermes_cli_update_cmd_py___has_upstream_remote — traced copy of uc_has_upstream_remote */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int uc_has_upstream_remote(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("uc_has_upstream_remote");
    int result = uc_has_upstream_remote(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("uc_has_upstream_remote");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_has_upstream_remote <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
