/* gate: hermes_cli_npm_engine_py__is_ebadengine — traced copy of ne_is_ebadengine */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool ne_is_ebadengine(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("ne_is_ebadengine");
    bool result = ne_is_ebadengine(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ne_is_ebadengine");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ne_is_ebadengine <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
