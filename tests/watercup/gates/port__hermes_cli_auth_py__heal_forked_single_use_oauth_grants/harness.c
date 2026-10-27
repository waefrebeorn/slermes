/* gate: port__hermes_cli_auth_py__heal_forked_single_use_oauth_grants — traced copy of heal_forked_single_use_oauth_grants (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void heal_forked_single_use_oauth_grants(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s heal_forked_single_use_oauth_grants <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("heal_forked_single_use_oauth_grants");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    heal_forked_single_use_oauth_grants();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("heal_forked_single_use_oauth_grants");
    }
    return 0;
}
