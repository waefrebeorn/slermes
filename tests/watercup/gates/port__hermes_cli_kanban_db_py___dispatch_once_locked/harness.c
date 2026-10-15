/* gate: port__hermes_cli_kanban_db_py___dispatch_once_locked — traced copy of kdb_dispatch_once_locked (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * kdb_dispatch_once_locked(const char *state_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_dispatch_once_locked <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kdb_dispatch_once_locked");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = kdb_dispatch_once_locked(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kdb_dispatch_once_locked");
    }
    return 0;
}
