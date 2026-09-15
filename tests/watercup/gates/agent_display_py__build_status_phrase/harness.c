/* gate: agent_display_py__build_status_phrase — traced copy of agent_display_build_status_phrase */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_display_build_status_phrase(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_display_build_status_phrase");
    int result = agent_display_build_status_phrase(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_display_build_status_phrase");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_display_build_status_phrase <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
