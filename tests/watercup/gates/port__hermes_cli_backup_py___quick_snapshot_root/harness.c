/* gate: port__hermes_cli_backup_py___quick_snapshot_root — traced copy of backup_quick_snapshot_root (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * backup_quick_snapshot_root(const char *hermes_home);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s backup_quick_snapshot_root <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("backup_quick_snapshot_root");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = backup_quick_snapshot_root(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("backup_quick_snapshot_root");
    }
    return 0;
}
