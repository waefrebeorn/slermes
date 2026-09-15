/* gate: tools_approval_py__has_blocking_approval — traced copy of apr2_has_blocking_approval */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool apr2_has_blocking_approval(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("apr2_has_blocking_approval");
    bool result = apr2_has_blocking_approval(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("apr2_has_blocking_approval");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s apr2_has_blocking_approval <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
