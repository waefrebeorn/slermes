/* gate: hermes_cli_session_recovery_py___same_filesystem — traced copy of sr_same_filesystem */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sr_same_filesystem(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("sr_same_filesystem");
    int result = sr_same_filesystem(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sr_same_filesystem");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sr_same_filesystem <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
