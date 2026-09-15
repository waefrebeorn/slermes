/* gate: hermes_cli_kanban_diagnostics_py__severity_at_or_above — traced copy of kanban_severity_at_or_above */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int kanban_severity_at_or_above(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("kanban_severity_at_or_above");
    int result = kanban_severity_at_or_above(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("kanban_severity_at_or_above");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kanban_severity_at_or_above <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
