/* gate: tools_browser_tool_py___session_has_expired — traced copy of browser_tool_session_has_expired */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int browser_tool_session_has_expired(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("browser_tool_session_has_expired");
    int result = browser_tool_session_has_expired(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_tool_session_has_expired");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_tool_session_has_expired <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
