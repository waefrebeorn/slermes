/* gate: agent_shell_hooks_py___locked_update_approvals — traced copy of shk_locked_update_approvals */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shk_locked_update_approvals(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("shk_locked_update_approvals");
    int result = shk_locked_update_approvals(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shk_locked_update_approvals");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shk_locked_update_approvals <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
