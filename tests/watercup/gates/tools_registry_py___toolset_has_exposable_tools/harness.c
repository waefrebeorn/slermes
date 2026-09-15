/* gate: tools_registry_py___toolset_has_exposable_tools — traced copy of toolset_has_exposable_tools */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool toolset_has_exposable_tools(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("toolset_has_exposable_tools");
    bool result = toolset_has_exposable_tools(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("toolset_has_exposable_tools");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s toolset_has_exposable_tools <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
