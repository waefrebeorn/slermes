/* gate: gateway_session_py__recover_interrupted_turns — traced copy of session_recover_interrupted_turns */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int session_recover_interrupted_turns(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("session_recover_interrupted_turns");
    int result = session_recover_interrupted_turns(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("session_recover_interrupted_turns");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s session_recover_interrupted_turns <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
