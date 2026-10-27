/* gate: port__hermes_cli_update_cmd_py___record_npm_lockfile_hash — traced copy of uc_record_npm_lockfile_hash (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void uc_record_npm_lockfile_hash(const char *project_root, const char *hermes_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s uc_record_npm_lockfile_hash <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("uc_record_npm_lockfile_hash");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    uc_record_npm_lockfile_hash();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("uc_record_npm_lockfile_hash");
    }
    return 0;
}
