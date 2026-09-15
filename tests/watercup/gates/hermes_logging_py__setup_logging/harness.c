/* gate: hermes_logging_py__setup_logging — traced copy of hlog_setup_logging */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int hlog_setup_logging(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("hlog_setup_logging");
    int result = hlog_setup_logging(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("hlog_setup_logging");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s hlog_setup_logging <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
