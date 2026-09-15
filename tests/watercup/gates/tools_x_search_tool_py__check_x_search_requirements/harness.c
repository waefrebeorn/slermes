/* gate: tools_x_search_tool_py__check_x_search_requirements — traced copy of tools_x_search_tool_check_x_search_requirements */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_x_search_tool_check_x_search_requirements(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_x_search_tool_check_x_search_requirements");
    int result = tools_x_search_tool_check_x_search_requirements(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_x_search_tool_check_x_search_requirements");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_x_search_tool_check_x_search_requirements <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
