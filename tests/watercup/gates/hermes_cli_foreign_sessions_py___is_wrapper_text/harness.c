/* gate: hermes_cli_foreign_sessions_py___is_wrapper_text — traced copy of foreign_sessions_is_wrapper_text */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int foreign_sessions_is_wrapper_text(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("foreign_sessions_is_wrapper_text");
    int result = foreign_sessions_is_wrapper_text(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("foreign_sessions_is_wrapper_text");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s foreign_sessions_is_wrapper_text <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
