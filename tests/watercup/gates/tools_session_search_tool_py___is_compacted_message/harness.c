/* gate: tools_session_search_tool_py___is_compacted_message — traced copy of tools_session_search_tool_u_is_compacted_message */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_session_search_tool_u_is_compacted_message(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_session_search_tool_u_is_compacted_message");
    int result = tools_session_search_tool_u_is_compacted_message(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_session_search_tool_u_is_compacted_message");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_session_search_tool_u_is_compacted_message <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
