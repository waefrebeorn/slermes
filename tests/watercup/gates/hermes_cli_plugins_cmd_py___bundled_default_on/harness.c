/* gate: hermes_cli_plugins_cmd_py___bundled_default_on — traced copy of _bundled_default_on */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool _bundled_default_on(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_bundled_default_on");
    bool result = _bundled_default_on(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_bundled_default_on");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _bundled_default_on <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
