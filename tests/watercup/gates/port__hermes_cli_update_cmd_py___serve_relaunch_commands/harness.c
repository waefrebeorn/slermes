/* gate: port__hermes_cli_update_cmd_py___serve_relaunch_commands — traced copy of _serve_relaunch_commands (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _serve_relaunch_commands(const char *entries);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _serve_relaunch_commands <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_serve_relaunch_commands");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _serve_relaunch_commands(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_serve_relaunch_commands");
    }
    return 0;
}
