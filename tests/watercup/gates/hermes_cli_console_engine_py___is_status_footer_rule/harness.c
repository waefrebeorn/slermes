/* gate: hermes_cli_console_engine_py___is_status_footer_rule — traced copy of console_engine_is_status_footer_rule */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int console_engine_is_status_footer_rule(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("console_engine_is_status_footer_rule");
    int result = console_engine_is_status_footer_rule(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("console_engine_is_status_footer_rule");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s console_engine_is_status_footer_rule <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
