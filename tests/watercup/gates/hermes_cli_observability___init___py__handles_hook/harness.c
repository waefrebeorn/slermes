/* gate: hermes_cli_observability___init___py__handles_hook — traced copy of obs_handles_hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int obs_handles_hook(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("obs_handles_hook");
    int result = obs_handles_hook(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("obs_handles_hook");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s obs_handles_hook <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
