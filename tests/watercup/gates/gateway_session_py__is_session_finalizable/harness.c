/* gate: gateway_session_py__is_session_finalizable — traced copy of is_session_finalizable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool is_session_finalizable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("is_session_finalizable");
    bool result = is_session_finalizable(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_session_finalizable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_session_finalizable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
