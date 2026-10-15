/* gate: port__hermes_cli_blueprint_cmd_py___manage_hint — traced copy of blueprint_cmd_manage_hint (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * blueprint_cmd_manage_hint(const char *surface);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s blueprint_cmd_manage_hint <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("blueprint_cmd_manage_hint");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = blueprint_cmd_manage_hint(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("blueprint_cmd_manage_hint");
    }
    return 0;
}
