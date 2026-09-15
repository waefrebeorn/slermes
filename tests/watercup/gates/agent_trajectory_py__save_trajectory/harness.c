/* gate: agent_trajectory_py__save_trajectory — traced copy of trj_save_trajectory */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int trj_save_trajectory(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("trj_save_trajectory");
    int result = trj_save_trajectory(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("trj_save_trajectory");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s trj_save_trajectory <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
