/* gate: tools_mcp_oauth_py__remove — traced copy of mco_remove */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mco_remove(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("mco_remove");
    int result = mco_remove(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mco_remove");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mco_remove <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
