/* gate: hermes_cli_session_lost_and_found_py___cli_supports_recover — traced copy of laf_cli_supports_recover */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int laf_cli_supports_recover(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("laf_cli_supports_recover");
    int result = laf_cli_supports_recover(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("laf_cli_supports_recover");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s laf_cli_supports_recover <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
