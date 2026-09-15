/* gate: tools_tool_search_py___safe_float — traced copy of tools_tool_search_u_safe_float */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_tool_search_u_safe_float(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_tool_search_u_safe_float");
    int result = tools_tool_search_u_safe_float(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_tool_search_u_safe_float");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_tool_search_u_safe_float <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
