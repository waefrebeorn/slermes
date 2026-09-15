/* gate: hermes_cli_console_engine_py___contains_shell_syntax — traced copy of console_engine_contains_shell_syntax */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int console_engine_contains_shell_syntax(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("console_engine_contains_shell_syntax");
    int result = console_engine_contains_shell_syntax(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("console_engine_contains_shell_syntax");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s console_engine_contains_shell_syntax <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
