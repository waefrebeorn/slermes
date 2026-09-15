/* gate: agent_moa_loop_py__build_moa_facade — traced copy of moa_build_moa_facade */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int moa_build_moa_facade(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("moa_build_moa_facade");
    int result = moa_build_moa_facade(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("moa_build_moa_facade");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s moa_build_moa_facade <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
