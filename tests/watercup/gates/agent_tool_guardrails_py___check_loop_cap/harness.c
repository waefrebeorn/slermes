/* gate: agent_tool_guardrails_py___check_loop_cap — traced copy of guarrd_check_loop_cap */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int guarrd_check_loop_cap(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("guarrd_check_loop_cap");
    int result = guarrd_check_loop_cap(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("guarrd_check_loop_cap");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s guarrd_check_loop_cap <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
