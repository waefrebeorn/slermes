/* gate: hermes_cli_update_cmd_py___git_is_trampoline — traced copy of _git_is_trampoline */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int _git_is_trampoline(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("_git_is_trampoline");
    int result = _git_is_trampoline(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_git_is_trampoline");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _git_is_trampoline <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
