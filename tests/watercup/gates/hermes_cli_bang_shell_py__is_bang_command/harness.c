/* gate: hermes_cli_bang_shell_py__is_bang_command — traced copy of bang_shell_is_bang_command */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

extern bool bang_shell_is_bang_command(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("bang_shell_is_bang_command");
    bool result = bang_shell_is_bang_command(input);
    WCUP_VALSTR(result ? "true" : "false");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("bang_shell_is_bang_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s bang_shell_is_bang_command <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
