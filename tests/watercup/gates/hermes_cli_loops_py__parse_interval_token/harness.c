/* gate: hermes_cli_loops_py__parse_interval_token — traced copy of loops_parse_interval_token */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int loops_parse_interval_token(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("loops_parse_interval_token");
    int result = loops_parse_interval_token(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("loops_parse_interval_token");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_parse_interval_token <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
