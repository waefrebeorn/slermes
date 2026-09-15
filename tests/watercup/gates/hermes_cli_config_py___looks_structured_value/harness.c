/* gate: hermes_cli_config_py___looks_structured_value — traced copy of _looks_structured_value */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _looks_structured_value(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_looks_structured_value");
    int result = _looks_structured_value(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_looks_structured_value");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _looks_structured_value <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
