/* gate: port__tools_skill_ledger_py__reset_ledger_actor — traced copy of reset_ledger_actor (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern void reset_ledger_actor(int token);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s reset_ledger_actor <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("reset_ledger_actor");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    reset_ledger_actor();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("reset_ledger_actor");
    }
    return 0;
}
