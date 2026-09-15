/* gate: hermes_cli_session_lost_and_found_py___lost_and_found_db_usable — traced copy of laf_lost_and_found_db_usable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int laf_lost_and_found_db_usable(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("laf_lost_and_found_db_usable");
    int result = laf_lost_and_found_db_usable(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("laf_lost_and_found_db_usable");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s laf_lost_and_found_db_usable <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
