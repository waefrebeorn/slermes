/* gate: hermes_cli_main_py__fileno — traced copy of main_tee_init */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_tee_init(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_tee_init");
    int result = main_tee_init(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_tee_init");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_tee_init <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
