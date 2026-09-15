/* gate: tools_mcp_tool_py___resolve_server_lazy — traced copy of mct_resolve_server_lazy */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mct_resolve_server_lazy(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mct_resolve_server_lazy");
    int result = mct_resolve_server_lazy(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mct_resolve_server_lazy");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_resolve_server_lazy <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
