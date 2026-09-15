/* gate: tools_approval_py__request_tool_approval — traced copy of appr_request_tool_approval */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int appr_request_tool_approval(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("appr_request_tool_approval");
    int result = appr_request_tool_approval(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("appr_request_tool_approval");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s appr_request_tool_approval <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
