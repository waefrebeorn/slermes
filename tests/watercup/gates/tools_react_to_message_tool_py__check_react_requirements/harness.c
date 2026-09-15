/* gate: tools_react_to_message_tool_py__check_react_requirements — traced copy of react_check_requirements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool react_check_requirements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("react_check_requirements");
    bool result = react_check_requirements(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("react_check_requirements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s react_check_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
