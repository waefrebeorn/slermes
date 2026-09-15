/* gate: gateway_run_py___commit_then_release_soft — traced copy of grun_u_commit_then_release_soft */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_commit_then_release_soft(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_commit_then_release_soft");
    int result = grun_u_commit_then_release_soft(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_commit_then_release_soft");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_commit_then_release_soft <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
