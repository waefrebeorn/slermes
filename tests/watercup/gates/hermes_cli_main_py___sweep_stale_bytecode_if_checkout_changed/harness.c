/* gate: hermes_cli_main_py___sweep_stale_bytecode_if_checkout_changed — traced copy of main_g_sweep_stale_bytecode_if_checkout_changed */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"

extern int main_g_sweep_stale_bytecode_if_checkout_changed(const char *input);

static void trace_one(const char *input) {
    WCUP_BEGIN("main_g_sweep_stale_bytecode_if_checkout_changed");
    int result = main_g_sweep_stale_bytecode_if_checkout_changed(input);
    WCUP_VALINT(result);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("main_g_sweep_stale_bytecode_if_checkout_changed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s main_g_sweep_stale_bytecode_if_checkout_changed <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        trace_one(argv[i]);
    }
    return 0;
}
