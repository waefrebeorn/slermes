/* gate: tools_terminal_tool_py__record_session_cwd — traced copy of tt_record_session_cwd */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int tt_record_session_cwd(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("tt_record_session_cwd");
    int result = tt_record_session_cwd(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tt_record_session_cwd");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s tt_record_session_cwd <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
