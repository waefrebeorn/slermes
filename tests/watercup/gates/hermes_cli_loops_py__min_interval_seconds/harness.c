/* gate: hermes_cli_loops_py__min_interval_seconds — traced copy of loops_min_interval_seconds */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int loops_min_interval_seconds(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("loops_min_interval_seconds");
    int result = loops_min_interval_seconds(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("loops_min_interval_seconds");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_min_interval_seconds <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
