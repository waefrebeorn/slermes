/* gate: agent_verify_hooks_py__max_verify_nudges — traced copy of agent_verify_hooks_max_verify_nudges */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_verify_hooks_max_verify_nudges(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_verify_hooks_max_verify_nudges");
    int result = agent_verify_hooks_max_verify_nudges(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_verify_hooks_max_verify_nudges");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_verify_hooks_max_verify_nudges <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
