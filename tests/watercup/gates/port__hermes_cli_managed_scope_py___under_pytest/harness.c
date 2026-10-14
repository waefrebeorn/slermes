/* gate: port__hermes_cli_managed_scope_py___under_pytest — traced copy of managed_scope_under_pytest (int_noargs) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int managed_scope_under_pytest(void);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s managed_scope_under_pytest <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("managed_scope_under_pytest");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = managed_scope_under_pytest();
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("managed_scope_under_pytest");
    }
    return 0;
}
