/* gate: tools_mcp_tool_py___handle_session_expired_and_retry — traced copy of mct_handle_session_expired_and_retry */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mct_handle_session_expired_and_retry(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("mct_handle_session_expired_and_retry");
    int result = mct_handle_session_expired_and_retry(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mct_handle_session_expired_and_retry");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mct_handle_session_expired_and_retry <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
