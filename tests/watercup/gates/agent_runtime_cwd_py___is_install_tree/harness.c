/* gate: agent_runtime_cwd_py___is_install_tree — traced copy of agent_runtime_cwd_u_is_install_tree */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int agent_runtime_cwd_u_is_install_tree(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("agent_runtime_cwd_u_is_install_tree");
    int result = agent_runtime_cwd_u_is_install_tree(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("agent_runtime_cwd_u_is_install_tree");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s agent_runtime_cwd_u_is_install_tree <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
