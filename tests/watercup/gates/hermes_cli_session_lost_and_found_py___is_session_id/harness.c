/* gate: hermes_cli_session_lost_and_found_py___is_session_id — traced copy of laf_is_session_id */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int laf_is_session_id(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("laf_is_session_id");
    int result = laf_is_session_id(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("laf_is_session_id");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s laf_is_session_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
