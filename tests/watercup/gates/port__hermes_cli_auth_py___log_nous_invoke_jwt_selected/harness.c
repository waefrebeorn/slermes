/* gate: port__hermes_cli_auth_py___log_nous_invoke_jwt_selected — traced copy of auth_log_nous_invoke_jwt_selected (void) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern void auth_log_nous_invoke_jwt_selected(const char *access_token, const char *sequence_id);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s auth_log_nous_invoke_jwt_selected <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("auth_log_nous_invoke_jwt_selected");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    auth_log_nous_invoke_jwt_selected(argv[i], argv[2 + (i - 2 + 1) % 5]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR("void");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("auth_log_nous_invoke_jwt_selected");
    }
    return 0;
}
