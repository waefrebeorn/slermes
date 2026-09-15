/* gate: agent_shell_hooks_py__save_allowlist — traced copy of shk_save_allowlist */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int shk_save_allowlist(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("shk_save_allowlist");
    int result = shk_save_allowlist(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("shk_save_allowlist");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s shk_save_allowlist <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
