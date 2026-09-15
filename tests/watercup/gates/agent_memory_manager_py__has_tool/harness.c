/* gate: agent_memory_manager_py__has_tool — traced copy of mgr_has_tool */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mgr_has_tool(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mgr_has_tool");
    bool result = mgr_has_tool(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mgr_has_tool");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mgr_has_tool <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
