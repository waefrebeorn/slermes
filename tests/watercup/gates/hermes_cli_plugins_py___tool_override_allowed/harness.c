/* gate: hermes_cli_plugins_py___tool_override_allowed — traced copy of plug_tool_override_allowed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool plug_tool_override_allowed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("plug_tool_override_allowed");
    bool result = plug_tool_override_allowed(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("plug_tool_override_allowed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s plug_tool_override_allowed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
