/* gate: hermes_cli_commands_py__is_interrupt_then_dispatch — traced copy of cli_commands_is_interrupt_then_dispatch */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool cli_commands_is_interrupt_then_dispatch(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("cli_commands_is_interrupt_then_dispatch");
    bool result = cli_commands_is_interrupt_then_dispatch(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("cli_commands_is_interrupt_then_dispatch");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_commands_is_interrupt_then_dispatch <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
