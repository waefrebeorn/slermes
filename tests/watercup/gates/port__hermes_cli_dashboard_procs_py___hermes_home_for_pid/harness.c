/* gate: port__hermes_cli_dashboard_procs_py___hermes_home_for_pid — traced copy of dashboard_procs_hermes_home_for_pid (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * dashboard_procs_hermes_home_for_pid(int pid);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s dashboard_procs_hermes_home_for_pid <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("dashboard_procs_hermes_home_for_pid");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = dashboard_procs_hermes_home_for_pid(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("dashboard_procs_hermes_home_for_pid");
    }
    return 0;
}
