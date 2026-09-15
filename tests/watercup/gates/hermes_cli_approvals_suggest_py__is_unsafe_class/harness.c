/* gate: hermes_cli_approvals_suggest_py__is_unsafe_class — traced copy of approval_is_unsafe_class */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool approval_is_unsafe_class(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("approval_is_unsafe_class");
    bool result = approval_is_unsafe_class(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("approval_is_unsafe_class");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s approval_is_unsafe_class <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
