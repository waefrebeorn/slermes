/* gate: port__hermes_cli_update_cmd_py___venv_foreign_owned_paths — traced copy of _venv_foreign_owned_paths (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void _venv_foreign_owned_paths(const char *venv_dir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _venv_foreign_owned_paths <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_venv_foreign_owned_paths");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    _venv_foreign_owned_paths(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_venv_foreign_owned_paths");
    }
    return 0;
}
