/* gate: port__hermes_cli_main_py___pytest_owns_live_checkout — traced copy of main_g_pytest_owns_live_checkout (bool_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern bool main_g_pytest_owns_live_checkout(const char *root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_pytest_owns_live_checkout <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("main_g_pytest_owns_live_checkout");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    bool result = main_g_pytest_owns_live_checkout(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    WCUP_VALSTR(result ? "true" : "false");
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("main_g_pytest_owns_live_checkout");
    }
    return 0;
}
