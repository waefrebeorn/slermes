/* gate: port__hermes_cli_auth_py__nous_token_has_billing_scope — traced copy of ath_nous_token_has_billing_scope (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool ath_nous_token_has_billing_scope(const char *token_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s ath_nous_token_has_billing_scope <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("ath_nous_token_has_billing_scope");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = ath_nous_token_has_billing_scope(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("ath_nous_token_has_billing_scope");
    }
    return 0;
}
