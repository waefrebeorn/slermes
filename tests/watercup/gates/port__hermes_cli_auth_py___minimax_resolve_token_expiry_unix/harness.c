/* gate: port__hermes_cli_auth_py___minimax_resolve_token_expiry_unix — traced copy of _minimax_resolve_token_expiry_unix (scalar_multi) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern double _minimax_resolve_token_expiry_unix(long expired_in, double now_unix);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s _minimax_resolve_token_expiry_unix <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("_minimax_resolve_token_expiry_unix");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    double result = _minimax_resolve_token_expiry_unix((long)strtol(argv[i], NULL, 10), strtod(argv[2 + (i - 2 + 1) % 5], NULL));
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("_minimax_resolve_token_expiry_unix");
    }
    return 0;
}
