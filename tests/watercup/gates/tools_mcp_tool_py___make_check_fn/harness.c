/* gate: tools_mcp_tool_py___make_check_fn — traced copy of mct_make_check_fn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool mct_make_check_fn(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mct_make_check_fn");
    bool result = mct_make_check_fn(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mct_make_check_fn");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_make_check_fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
