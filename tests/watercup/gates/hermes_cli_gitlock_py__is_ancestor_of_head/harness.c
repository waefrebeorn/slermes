/* gate: hermes_cli_gitlock_py__is_ancestor_of_head — traced copy of is_ancestor_of_head */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int is_ancestor_of_head(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("is_ancestor_of_head");
    int result = is_ancestor_of_head(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("is_ancestor_of_head");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s is_ancestor_of_head <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
