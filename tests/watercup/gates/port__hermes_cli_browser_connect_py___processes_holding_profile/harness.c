/* gate: port__hermes_cli_browser_connect_py___processes_holding_profile — traced copy of _processes_holding_profile (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _processes_holding_profile(const char *profile_dir);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _processes_holding_profile <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_processes_holding_profile");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _processes_holding_profile(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_processes_holding_profile");
    }
    return 0;
}
