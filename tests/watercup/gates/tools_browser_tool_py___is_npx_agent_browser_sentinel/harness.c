/* gate: tools_browser_tool_py___is_npx_agent_browser_sentinel — traced copy of browser__is_npx_agent_browser_sentinel */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int browser__is_npx_agent_browser_sentinel(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser__is_npx_agent_browser_sentinel");
    int result = browser__is_npx_agent_browser_sentinel(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser__is_npx_agent_browser_sentinel");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser__is_npx_agent_browser_sentinel <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
