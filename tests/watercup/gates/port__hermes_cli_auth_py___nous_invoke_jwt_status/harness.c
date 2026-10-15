/* gate: port__hermes_cli_auth_py___nous_invoke_jwt_status — traced copy of auth_nous_invoke_jwt_status (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern char * auth_nous_invoke_jwt_status(const char *token, const char *scope, const char *expires_at, int min_ttl_seconds);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_nous_invoke_jwt_status <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_nous_invoke_jwt_status");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    char * result = auth_nous_invoke_jwt_status((int)strtol(argv[i], NULL, 10), (int)strtol(argv[2 + (i - 2 + 1) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 2) % 5], NULL, 10), (int)strtol(argv[2 + (i - 2 + 3) % 5], NULL, 10));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];\n    snprintf(buf, sizeof(buf), "%d", result);\n    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_nous_invoke_jwt_status");
    }
    return 0;
}
