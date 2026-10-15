/* gate: port__hermes_cli_auth_py___nous_jwt_expires_at — traced copy of auth_nous_jwt_expires_at (charptr_simple) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * auth_nous_jwt_expires_at(const char *token, const char *fallback_expires_at);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_nous_jwt_expires_at <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_nous_jwt_expires_at");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = auth_nous_jwt_expires_at(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? result : "");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_nous_jwt_expires_at");
    }
    return 0;
}
