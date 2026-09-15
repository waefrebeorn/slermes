/* gate: agent_trajectory_py__has_incomplete_scratchpad — traced copy of trj_has_incomplete_scratchpad */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool trj_has_incomplete_scratchpad(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("trj_has_incomplete_scratchpad");
    bool result = trj_has_incomplete_scratchpad(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("trj_has_incomplete_scratchpad");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s trj_has_incomplete_scratchpad <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
