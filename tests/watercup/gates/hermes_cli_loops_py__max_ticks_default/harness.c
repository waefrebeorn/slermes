/* gate: hermes_cli_loops_py__max_ticks_default — traced copy of loops_max_ticks_default */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int loops_max_ticks_default(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("loops_max_ticks_default");
    int result = loops_max_ticks_default(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("loops_max_ticks_default");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s loops_max_ticks_default <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
