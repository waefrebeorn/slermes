/* gate: real__agent_copilot_acp_client_py___jsonrpc_error — GAP sentinel for _jsonrpc_error */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

/* No C port exists — this is a REAL_GAP function */
static const char *gap_sentinel(const char *input) {
    (void)input;
    return "_jsonrpc_error";
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _jsonrpc_error <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_jsonrpc_error");
        const char *result = gap_sentinel(argv[i]);
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_jsonrpc_error");
    }
    return 0;
}
