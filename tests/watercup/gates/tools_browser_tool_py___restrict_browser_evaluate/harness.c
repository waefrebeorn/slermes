/* gate: tools_browser_tool_py___restrict_browser_evaluate — traced copy of tools_browser_tool_u_restrict_browser_evaluate */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_browser_tool_u_restrict_browser_evaluate(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_browser_tool_u_restrict_browser_evaluate");
    int result = tools_browser_tool_u_restrict_browser_evaluate(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_browser_tool_u_restrict_browser_evaluate");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_browser_tool_u_restrict_browser_evaluate <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
