/* gate: gateway_run_py___goal_still_active_for_session — traced copy of gw_goal_still_active_for_session */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool gw_goal_still_active_for_session(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("gw_goal_still_active_for_session");
    bool result = gw_goal_still_active_for_session(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("gw_goal_still_active_for_session");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s gw_goal_still_active_for_session <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
