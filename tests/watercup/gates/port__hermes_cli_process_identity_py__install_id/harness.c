/* gate: port__hermes_cli_process_identity_py__install_id — traced copy of process_identity_install_id (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * process_identity_install_id(const char *project_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s process_identity_install_id <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("process_identity_install_id");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = process_identity_install_id(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("process_identity_install_id");
    }
    return 0;
}
