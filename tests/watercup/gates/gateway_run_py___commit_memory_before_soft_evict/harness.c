/* gate: gateway_run_py___commit_memory_before_soft_evict — traced copy of grun_u_commit_memory_before_soft_evict */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int grun_u_commit_memory_before_soft_evict(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("grun_u_commit_memory_before_soft_evict");
    int result = grun_u_commit_memory_before_soft_evict(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("grun_u_commit_memory_before_soft_evict");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s grun_u_commit_memory_before_soft_evict <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
