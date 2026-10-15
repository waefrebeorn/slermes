/* gate: port__hermes_cli_cli_billing_mixin_py___billing_buy_flow — traced copy of cbm_billing_buy_flow (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern int cbm_billing_buy_flow(const char *state_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cbm_billing_buy_flow <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cbm_billing_buy_flow");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = cbm_billing_buy_flow(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cbm_billing_buy_flow");
    }
    return 0;
}
