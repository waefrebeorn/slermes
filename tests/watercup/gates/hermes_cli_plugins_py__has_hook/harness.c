/* gate: hermes_cli_plugins_py__has_hook — traced copy of plug_has_hook */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool plug_has_hook(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("plug_has_hook");
    bool result = plug_has_hook(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("plug_has_hook");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s plug_has_hook <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
