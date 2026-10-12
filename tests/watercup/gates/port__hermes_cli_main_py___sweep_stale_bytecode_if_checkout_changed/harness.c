/* gate: port__hermes_cli_main_py___sweep_stale_bytecode_if_checkout_changed — traced copy of main_g_sweep_stale_bytecode_if_checkout_changed (int_str) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "watercup.h"

extern int main_g_sweep_stale_bytecode_if_checkout_changed(const char *project_root);

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_sweep_stale_bytecode_if_checkout_changed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("main_g_sweep_stale_bytecode_if_checkout_changed");
    fflush(stdout);
    int _sv = dup(1);
    FILE *_dn = fopen("/dev/null", "w");
    if (_dn) dup2(fileno(_dn), 1);
    int result = main_g_sweep_stale_bytecode_if_checkout_changed(argv[i]);
    fflush(stdout);
    if (_dn) { dup2(_sv, 1); fclose(_dn); close(_sv); }
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", result);
    WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("main_g_sweep_stale_bytecode_if_checkout_changed");
    }
    return 0;
}
