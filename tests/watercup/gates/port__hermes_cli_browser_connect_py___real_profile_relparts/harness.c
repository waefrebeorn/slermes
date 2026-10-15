/* gate: port__hermes_cli_browser_connect_py___real_profile_relparts — traced copy of _real_profile_relparts (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * _real_profile_relparts(const char *browser);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _real_profile_relparts <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_real_profile_relparts");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = _real_profile_relparts(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_real_profile_relparts");
    }
    return 0;
}
