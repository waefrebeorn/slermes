/* gate: hermes_cli_web_git_py___untracked_insertions — traced copy of untracked_insertions */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int untracked_insertions(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("untracked_insertions");
    int result = untracked_insertions(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("untracked_insertions");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s untracked_insertions <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
