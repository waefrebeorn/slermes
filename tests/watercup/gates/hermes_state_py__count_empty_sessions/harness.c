/* gate: hermes_state_py__count_empty_sessions — traced copy of ws_sessions_count_empty */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ws_sessions_count_empty(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ws_sessions_count_empty");
    int result = ws_sessions_count_empty(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ws_sessions_count_empty");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ws_sessions_count_empty <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
