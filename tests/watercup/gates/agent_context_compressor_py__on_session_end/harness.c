/* gate: agent_context_compressor_py__on_session_end — traced copy of cc_on_session_end */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int cc_on_session_end(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cc_on_session_end");
    int result = cc_on_session_end(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cc_on_session_end");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cc_on_session_end <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
