/* gate: hermes_cli_web_git_py___entry_staged — traced copy of entry_staged */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int entry_staged(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("entry_staged");
    int result = entry_staged(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("entry_staged");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s entry_staged <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
