/* gate: agent_verification_stop_py___workspace_has_runnable_recipe — traced copy of verification_stop_workspace_has_runnable_recipe */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool verification_stop_workspace_has_runnable_recipe(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("verification_stop_workspace_has_runnable_recipe");
    bool result = verification_stop_workspace_has_runnable_recipe(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("verification_stop_workspace_has_runnable_recipe");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s verification_stop_workspace_has_runnable_recipe <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
