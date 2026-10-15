/* gate: port__hermes_cli_setup_py___profile_name_from_hermes_home — traced copy of stp_profile_name_from_hermes_home (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * stp_profile_name_from_hermes_home(const char *hermes_home_path);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s stp_profile_name_from_hermes_home <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("stp_profile_name_from_hermes_home");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = stp_profile_name_from_hermes_home(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("stp_profile_name_from_hermes_home");
    }
    return 0;
}
