/* gate: port__hermes_cli_commands_py___requires_argument — traced copy of commands_requires_argument (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool commands_requires_argument(const char *args_hint);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s commands_requires_argument <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("commands_requires_argument");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = commands_requires_argument(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("commands_requires_argument");
    }
    return 0;
}
