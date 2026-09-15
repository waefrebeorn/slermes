/* gate: cli_py___is_backslash_line_continuation — traced copy of cli_is_backslash_line_continuation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cli_is_backslash_line_continuation(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_is_backslash_line_continuation");
    bool result = cli_is_backslash_line_continuation(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_is_backslash_line_continuation");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_is_backslash_line_continuation <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
