/* gate: gateway_run_py___looks_like_profile_conflict_from_cmdline — traced copy of gw_looks_like_profile_conflict_from_cmdline */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_looks_like_profile_conflict_from_cmdline(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_looks_like_profile_conflict_from_cmdline");
    int result = gw_looks_like_profile_conflict_from_cmdline(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_looks_like_profile_conflict_from_cmdline");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_looks_like_profile_conflict_from_cmdline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
