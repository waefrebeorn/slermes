/* gate: agent_lsp_manager_py___mark_broken_for_file — traced copy of lspm_mark_broken_for_file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int lspm_mark_broken_for_file(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("lspm_mark_broken_for_file");
    int result = lspm_mark_broken_for_file(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("lspm_mark_broken_for_file");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s lspm_mark_broken_for_file <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
