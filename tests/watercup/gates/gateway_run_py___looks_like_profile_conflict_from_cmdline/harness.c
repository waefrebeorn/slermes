/* gate: gateway_run_py___looks_like_profile_conflict_from_cmdline — traced copy of gw_looks_like_profile_conflict_from_cmdline */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_looks_like_profile_conflict_from_cmdline(const char *a0, const char *a1);

static void trace_one(const char **in, int n, int idx) {
    WCUP_BEGIN("gw_looks_like_profile_conflict_from_cmdline");
    int result = gw_looks_like_profile_conflict_from_cmdline(in[(idx + 0) % n], in[(idx + 1) % n]);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_looks_like_profile_conflict_from_cmdline");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_looks_like_profile_conflict_from_cmdline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one((const char **)(argv + 2), argc - 2, i - 2);
    }
    return 0;
}
