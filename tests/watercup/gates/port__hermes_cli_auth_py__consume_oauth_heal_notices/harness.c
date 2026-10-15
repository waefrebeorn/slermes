/* gate: port__hermes_cli_auth_py__consume_oauth_heal_notices — traced copy of consume_oauth_heal_notices (charptr_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * consume_oauth_heal_notices(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s consume_oauth_heal_notices <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("consume_oauth_heal_notices");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = consume_oauth_heal_notices();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%p", (void*)&result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("consume_oauth_heal_notices");
    }
    return 0;
}
