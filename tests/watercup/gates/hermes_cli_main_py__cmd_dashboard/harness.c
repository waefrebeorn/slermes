/* gate: hermes_cli_main_py__cmd_dashboard — traced copy of mn_cmd_dashboard */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int mn_cmd_dashboard(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("mn_cmd_dashboard");
    int result = mn_cmd_dashboard(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("mn_cmd_dashboard");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s mn_cmd_dashboard <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
