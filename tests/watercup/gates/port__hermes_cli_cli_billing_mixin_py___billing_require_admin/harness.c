/* gate: port__hermes_cli_cli_billing_mixin_py___billing_require_admin — traced copy of cbm_billing_require_admin (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"


extern bool cbm_billing_require_admin(const char *state_json);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cbm_billing_require_admin <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("cbm_billing_require_admin");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = cbm_billing_require_admin(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cbm_billing_require_admin");
    }
    return 0;
}
