/* gate: port__cli_py___prune_orphaned_branches — traced copy of cli_prune_orphaned_branches (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void cli_prune_orphaned_branches(const char *repo_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cli_prune_orphaned_branches <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cli_prune_orphaned_branches");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    cli_prune_orphaned_branches(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cli_prune_orphaned_branches");
    }
    return 0;
}
