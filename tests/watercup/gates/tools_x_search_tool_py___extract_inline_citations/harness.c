/* gate: tools_x_search_tool_py___extract_inline_citations — traced copy of tools_x_search_tool_u_extract_inline_citations */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tools_x_search_tool_u_extract_inline_citations(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tools_x_search_tool_u_extract_inline_citations");
    int result = tools_x_search_tool_u_extract_inline_citations(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tools_x_search_tool_u_extract_inline_citations");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tools_x_search_tool_u_extract_inline_citations <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
