/* gate: hermes_cli_agent_plugins_py___inside — traced copy of ap_inside */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int ap_inside(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("ap_inside");
    int result = ap_inside(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("ap_inside");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ap_inside <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
