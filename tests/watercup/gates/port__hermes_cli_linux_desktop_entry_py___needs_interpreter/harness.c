/* gate: port__hermes_cli_linux_desktop_entry_py___needs_interpreter — traced copy of _needs_interpreter (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool _needs_interpreter(const char *bin_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _needs_interpreter <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_needs_interpreter");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = _needs_interpreter(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_needs_interpreter");
    }
    return 0;
}
