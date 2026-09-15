/* gate: hermes_state_py___has_lone_cjk_run — traced copy of hermes_state_has_lone_cjk_run */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool hermes_state_has_lone_cjk_run(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hermes_state_has_lone_cjk_run");
    bool result = hermes_state_has_lone_cjk_run(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hermes_state_has_lone_cjk_run");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hermes_state_has_lone_cjk_run <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
