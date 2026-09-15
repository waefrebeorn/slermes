/* gate: hermes_cli_commands_py___requires_argument — traced copy of commands_requires_argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool commands_requires_argument(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("commands_requires_argument");
    bool result = commands_requires_argument(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("commands_requires_argument");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s commands_requires_argument <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
