/* gate: port__hermes_cli_pairing_py___cmd_revoke — traced copy of par_cmd_revoke (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * par_cmd_revoke(const char *platform, const char *user_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s par_cmd_revoke <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("par_cmd_revoke");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = par_cmd_revoke(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("par_cmd_revoke");
    }
    return 0;
}
