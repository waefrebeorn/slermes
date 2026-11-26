/* gate: hermes_cli_session_recovery_py___same_filesystem — traced copy of sr_same_filesystem */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int sr_same_filesystem(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("sr_same_filesystem");
    int result = sr_same_filesystem(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sr_same_filesystem");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sr_same_filesystem <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
