/* gate: port__hermes_cli_update_cmd_py___purge_stale_hermes_modules — traced copy of uc_purge_stale_hermes_modules (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void uc_purge_stale_hermes_modules(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_purge_stale_hermes_modules <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_purge_stale_hermes_modules");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    uc_purge_stale_hermes_modules();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_purge_stale_hermes_modules");
    }
    return 0;
}
