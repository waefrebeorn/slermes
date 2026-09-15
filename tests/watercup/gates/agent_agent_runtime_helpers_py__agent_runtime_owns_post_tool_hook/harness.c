/* gate: agent_agent_runtime_helpers_py__agent_runtime_owns_post_tool_hook — traced copy of arh_agent_runtime_owns_post_tool_hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool arh_agent_runtime_owns_post_tool_hook(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("arh_agent_runtime_owns_post_tool_hook");
    bool result = arh_agent_runtime_owns_post_tool_hook(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("arh_agent_runtime_owns_post_tool_hook");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s arh_agent_runtime_owns_post_tool_hook <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
