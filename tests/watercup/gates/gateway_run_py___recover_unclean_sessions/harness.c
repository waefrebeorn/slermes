/* gate: gateway_run_py___recover_unclean_sessions — traced copy of gw_recover_unclean_sessions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int gw_recover_unclean_sessions(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_recover_unclean_sessions");
    int result = gw_recover_unclean_sessions(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_recover_unclean_sessions");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_recover_unclean_sessions <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
