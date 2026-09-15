/* gate: tools_mcp_oauth_py__force_interactive_oauth — traced copy of mcpo_force_interactive_oauth */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_force_interactive_oauth(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_force_interactive_oauth");
    int result = mcpo_force_interactive_oauth(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_force_interactive_oauth");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_force_interactive_oauth <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
