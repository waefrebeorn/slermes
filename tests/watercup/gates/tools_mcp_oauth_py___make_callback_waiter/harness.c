/* gate: tools_mcp_oauth_py___make_callback_waiter — traced copy of mcpo_u_make_callback_waiter */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mcpo_u_make_callback_waiter(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mcpo_u_make_callback_waiter");
    int result = mcpo_u_make_callback_waiter(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mcpo_u_make_callback_waiter");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mcpo_u_make_callback_waiter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
