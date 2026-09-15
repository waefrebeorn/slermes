/* gate: hermes_cli_console_engine_py__register — traced copy of coe_register */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int coe_register(const char *a, const char *b);

static void trace_one(const char *a, const char *b) {
    WCUP_BEGIN("coe_register");
    int result = coe_register(a, b);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("coe_register");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s coe_register <a> <b>\n", argv[0]); return 2; }
    trace_one(argv[1], argv[2]);
    return 0;
}
