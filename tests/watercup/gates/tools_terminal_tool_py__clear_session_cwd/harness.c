/* gate: tools_terminal_tool_py__clear_session_cwd — traced copy of ttm_clear_session_cwd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ttm_clear_session_cwd(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ttm_clear_session_cwd");
    int result = ttm_clear_session_cwd(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ttm_clear_session_cwd");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ttm_clear_session_cwd <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
