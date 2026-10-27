/* gate: port__hermes_cli_console_engine_py___contains_shell_syntax — traced copy of console_engine_contains_shell_syntax (int_str_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int console_engine_contains_shell_syntax(const char *line, const char *tokens_csv);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s console_engine_contains_shell_syntax <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("console_engine_contains_shell_syntax");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = console_engine_contains_shell_syntax(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("console_engine_contains_shell_syntax");
    }
    return 0;
}
