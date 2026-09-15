/* gate: tools_mcp_oauth_manager_py__remove — traced copy of mom_remove */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mom_remove(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("mom_remove");
    int result = mom_remove(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mom_remove");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mom_remove <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
