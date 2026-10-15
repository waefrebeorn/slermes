/* gate: port__hermes_cli_kanban_decompose_py___resolve_default_assignee — traced copy of kdb_resolve_default_assignee (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * kdb_resolve_default_assignee(const char *kanban_cfg_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s kdb_resolve_default_assignee <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("kdb_resolve_default_assignee");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = kdb_resolve_default_assignee(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("kdb_resolve_default_assignee");
    }
    return 0;
}
